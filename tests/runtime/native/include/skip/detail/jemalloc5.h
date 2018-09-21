/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef ARENA_PRIVATE
#error Only include this file from within Arena.cpp
#endif

namespace skip {
namespace {
namespace je {

struct ChunkHooks {
  ~ChunkHooks() {
    purge(true);
    detach_hooks();
  }

  ChunkHooks(Arena::Kind kind)
      : m_arenaIdx(-1U),
        m_kind{kind},
        m_staticHooks{static_alloc,
                      static_dalloc,
                      static_destroy,
                      static_commit,
                      static_decommit,
                      static_purge_lazy,
                      static_purge_forced,
                      static_split,
                      static_merge} {
    Mib mib{"arenas.create"};
    size_t arenasz = sizeof(m_arenaIdx);
    const extent_hooks_t* p = &m_staticHooks;
    mallctl_by_mib(mib, &m_arenaIdx, &arenasz, &p, sizeof(p));
  }

  void purge(bool destroy) {
    if (m_arenaIdx == -1U)
      return;

    if (destroy) {
      je::arena_destroy(m_arenaIdx);
      m_arenaIdx = -1U;
    } else {
      je::arena_purge(m_arenaIdx);
    }

    // Free any chunks we didn't get a chance to free earlier.
    std::vector<void*> pendingDestruction;
    {
      auto state = m_state.lock();
      std::swap(pendingDestruction, state->m_pendingDestruction);
    }
    for (auto addr : pendingDestruction) {
      freeChunks(addr, kJeChunkSize);
    }
  }

  unsigned arenaIndex() const {
    return m_arenaIdx;
  }
  Arena::Kind kind() const {
    return m_kind;
  }

 private:
  unsigned m_arenaIdx;
  const Arena::Kind m_kind;
  const extent_hooks_t m_staticHooks;

  struct State {
    State() {
#if USE_JEMALLOC && (JEMALLOC_VERSION_MAJOR == 5) && \
    (JEMALLOC_VERSION_MINOR == 0) && (JEMALLOC_VERSION_BUGFIX == 0)
      // JeMalloc 5.0.0 has a bug where reentrant calls are not allowed from
      // extent_hooks alloc() or dalloc() calls during init and destruction of
      // an arena.
      // https://github.com/jemalloc/jemalloc/issues/923
      m_pendingDestruction.reserve(128);
#endif
    }

    void* m_pendingBegin = nullptr;
    void* m_pendingEnd = nullptr;

    // We use this array during destruction to keep a list of chunks
    // waiting to be destroyed.  We can't thread the freelist through
    // the chunks themselves because they've already been decommitted.
    std::vector<void*> m_pendingDestruction;
  };

  folly::Synchronized<State, std::mutex> m_state;

  void detach_hooks() {
    if (m_arenaIdx == -1U)
      return;
    static const Mib mib{"arena.0.extent_hooks"};
    extent_hooks_t* p = nullptr;
    size_t sz = sizeof(p);
    mallctl_by_mib(mib(m_arenaIdx), &p, &sz, &p, sizeof(p));
    m_arenaIdx = -1U;
  }

  bool isArenaMemory(const void* p) const {
    return Arena::KindMapper::singleton().get(p) == m_kind;
  }

  enum class JeResult { success, failure };

  void freeChunks(void* addr, size_t size) {
    assert(isChunkAligned(addr));
    assert(isChunkAligned(size));

#if OBSTACK_WP_FROZEN
    if (m_kind == Arena::Kind::obstack) {
      assert(size == mem::kReadOnlyMirrorSize);
      mem::freeReadOnlyMirror(addr, size);
      // We really allocated double the size for the read-only mirror - See
      // extent_alloc().
      size *= 2;
    } else
#endif
    {
      mem::free(addr, size);
    }

    Arena::KindMapper::singleton().erase(addr, mem::add(addr, size));
  }

  void*
  allocateChunks(size_t byteSize, size_t alignment, bool& zero, bool& commit) {
    assert(isChunkAligned(byteSize));

    // We require that the chunk alignment is at least a multiple of
    // kJeChunkSize (so the KindMapper will work).
    if (alignment < kJeChunkSize)
      alignment = kJeChunkSize;

    void* addr = nullptr;
#if OBSTACK_WP_FROZEN
    if (m_kind == Arena::Kind::obstack) {
      // In OBSTACK_WP_FROZEN mode we want to allocate chunks such that
      // they're 2*kReadOnlyMirrorSize aligned and mapped so at
      // address+kReadOnlyMirrorSize is a read-only mirror.
      // kReadOnlyMirrorSize must be exactly kJeChunkSize.
      static_assert(mem::kReadOnlyMirrorSize == kJeChunkSize, "");
      assert(byteSize == mem::kReadOnlyMirrorSize);
      if (byteSize != mem::kReadOnlyMirrorSize)
        return nullptr;
      alignment = mem::kReadOnlyMirrorSize * 2;

      addr = mem::allocReadOnlyMirror(byteSize, alignment);
      commit = true;
      zero = true;
      // Tweak the byteSize so the KindMapper registers both parts as "obstack"
      byteSize *= 2;
    } else
#endif
    {
      if (commit) {
        addr = mem::alloc(byteSize, alignment);
        zero = true;
      } else {
        addr = mem::reserve(byteSize, alignment);
      }
    }

    if (addr) {
      Arena::setMemoryKind(addr, mem::add(addr, byteSize), m_kind);
    }

    return addr;
  }

  JeResult extent_alloc(
      void*& chunk,
      size_t byteSize,
      size_t alignment,
      bool& zero,
      bool& commit) {
    // We don't support asking for explicit addresses...
    if (chunk != nullptr) {
      return JeResult::failure;
    }

#if OBSTACK_WP_FROZEN
    // If they're asking for a mirrored allocation and it's bigger
    // than kReadOnlyMirrorSize then refuse.
    if (m_kind == Arena::Kind::obstack && byteSize > mem::kReadOnlyMirrorSize) {
      return JeResult::failure;
    }
#endif

    // If they're asking for at least a chunk then give them an even
    // multiple of chunks.
    if (byteSize >= kJeChunkSize) {
      byteSize = roundUp(byteSize, kJeChunkSize);
      chunk = allocateChunks(byteSize, alignment, zero, commit);
      return chunk ? JeResult::success : JeResult::failure;
    }

    // When dealing with sub-page allocations we always commit.
    commit = true;
    zero = true;

    auto state = m_state.lock();
    void* nextBegin = roundUp(state->m_pendingBegin, alignment);
    ptrdiff_t avail = mem::sub(state->m_pendingEnd, nextBegin);
    if (avail < (ptrdiff_t)byteSize) {
      // Well darn.
      nextBegin = allocateChunks(kJeChunkSize, alignment, zero, commit);
      state->m_pendingBegin = nextBegin;
      state->m_pendingEnd = mem::add(state->m_pendingBegin, kJeChunkSize);
    }

    chunk = nextBegin;
    state->m_pendingBegin = mem::add(nextBegin, byteSize);
    assert(state->m_pendingBegin <= state->m_pendingEnd);

    return JeResult::success;
  }

  JeResult extent_dalloc(void* addr, size_t size, bool commit) {
    // If they're trying to free memory which isn't chunk aligned then
    // just refuse because we don't want to deal with partial chunks.
    if (!isChunkAligned(addr) || (size < kJeChunkSize)) {
      return JeResult::failure;
    }

    // When we allocated we rounded the size up to a multiple of
    // kJeChunkSize - so we need to free the same.

    size = roundUp(size, kJeChunkSize);
    freeChunks(addr, size);
    return JeResult::success;
  }

  void extent_destroy(void* addr, size_t size, bool commit) {
    // We can ignore unaligned memory - it must have been a small
    // allocation partitioned out of a larger chunk which jemalloc
    // will give us during another extent_destroy().
    if (!isChunkAligned(addr))
      return;

    if (size >= kJeChunkSize) {
      // It's a large allocation - just free it now.
      size = roundUp(size, kJeChunkSize);
      freeChunks(addr, size);
    } else {
      // It's the head of a small allocation chunk - we can't free it
      // yet because we don't know if all the other blocks in that
      // chunk have been given back to us by jemalloc yet.
      auto state = m_state.lock();
      state->m_pendingDestruction.push_back(addr);
    }
  }

  JeResult
  extent_commit(void* addr, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(addr));
    assert(mem::isPageAligned(mem::add(addr, offset)));
    assert(mem::isPageAligned(length));

    mem::commit(mem::add(addr, offset), length);
    return JeResult::success;
  }

  JeResult
  extent_decommit(void* addr, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(addr));
    assert(mem::isPageAligned(mem::add(addr, offset)));
    assert(mem::isPageAligned(length));

    mem::decommit(mem::add(addr, offset), length);
    return JeResult::success;
  }

  JeResult
  extent_purge_lazy(void* addr, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(addr));
    assert(mem::isPageAligned(mem::add(addr, offset)));
    assert(mem::isPageAligned(length));
    mem::purge_lazy(mem::add(addr, offset), length);
    return JeResult::success;
  }

  JeResult
  extent_purge_forced(void* addr, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(addr));
    assert(mem::isPageAligned(mem::add(addr, offset)));
    assert(mem::isPageAligned(length));
    mem::purge_forced(mem::add(addr, offset), length);
    return JeResult::success;
  }

  JeResult extent_split(
      void* addr,
      size_t size,
      size_t size_a,
      size_t size_b,
      bool committed) {
    // Never allow a split.
    return JeResult::failure;
  }

  JeResult extent_merge(
      void* chunk_a,
      size_t size_a,
      void* chunk_b,
      size_t size_b,
      bool committed) {
    if (chunk_a > chunk_b) {
      std::swap(chunk_a, chunk_b);
      std::swap(size_a, size_b);
    }
    assert(mem::add(chunk_a, size_a) == chunk_b);

    // Merging is just fine with us as long as it follows our
    // allocation rules (so we can reasonably free it).
    if (size_a + size_b < kJeChunkSize) {
      // The resulting block will be smaller than a chunk - allow it
      // as long as they're in the same chunk.
      if (((uintptr_t)chunk_a ^ ((uintptr_t)chunk_b + size_b - 1)) <
          kJeChunkSize) {
        return JeResult::success;
      }
    } else {
      // The resulting block will be at least a chunk - we're okay
      // with the merge as long as the resulting block is all chunky.
      if (isChunkAligned(chunk_a) &&
          isChunkAligned(mem::add(chunk_b, size_b))) {
        return JeResult::success;
      }
    }

    return JeResult::failure;
  }

  static ChunkHooks* static_chunkHooksFromExtent(extent_hooks_t* extent_hooks) {
    return static_cast<ChunkHooks*>(
        mem::sub(extent_hooks, offsetof(ChunkHooks, m_staticHooks)));
  }

  static void* static_alloc(
      extent_hooks_t* extent_hooks,
      void* new_addr,
      size_t size,
      size_t alignment,
      bool* zero,
      bool* commit,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    if (p->extent_alloc(new_addr, size, alignment, *zero, *commit) ==
        ChunkHooks::JeResult::success) {
      return new_addr;
    } else {
      return nullptr;
    }
  }

  static bool static_dalloc(
      extent_hooks_t* extent_hooks,
      void* addr,
      size_t size,
      bool committed,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_dalloc(addr, size, committed) ==
        ChunkHooks::JeResult::failure);
  }

  static void static_destroy(
      extent_hooks_t* extent_hooks,
      void* addr,
      size_t size,
      bool committed,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    p->extent_destroy(addr, size, committed);
  }

  static bool static_commit(
      extent_hooks_t* extent_hooks,
      void* addr,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_commit(addr, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_decommit(
      extent_hooks_t* extent_hooks,
      void* addr,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_decommit(addr, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_purge_lazy(
      extent_hooks_t* extent_hooks,
      void* chunk,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_purge_lazy(chunk, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_purge_forced(
      extent_hooks_t* extent_hooks,
      void* chunk,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_purge_forced(chunk, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_split(
      extent_hooks_t* extent_hooks,
      void* addr,
      size_t size,
      size_t size_a,
      size_t size_b,
      bool committed,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_split(addr, size, size_a, size_b, committed) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_merge(
      extent_hooks_t* extent_hooks,
      void* chunk_a,
      size_t size_a,
      void* chunk_b,
      size_t size_b,
      bool committed,
      unsigned arenaIndex) {
    auto p = static_chunkHooksFromExtent(extent_hooks);
    return (
        p->extent_merge(chunk_a, size_a, chunk_b, size_b, committed) ==
        ChunkHooks::JeResult::failure);
  }
};
} // namespace je
} // anonymous namespace
} // namespace skip
