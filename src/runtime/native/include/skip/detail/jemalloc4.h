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

unsigned arenas_extend() {
  static const Mib mib{"arenas.extend"};
  return mallctl_by_mib_R<unsigned>(mib);
}

struct ChunkHooks {
  ~ChunkHooks() {
    je::arena_reset(m_arenaIdx); // This is basically a 'delete'
    je::arena_purge(m_arenaIdx);

    auto i = s_chunkLookup.find(m_arenaIdx);
    if (i != s_chunkLookup.end() && i->second == this) {
      static const Mib mib{"arena.0.chunk_hooks"};
      mallctl_by_mib(mib(m_arenaIdx), nullptr, nullptr, nullptr, 0);
      s_chunkLookup.erase(m_arenaIdx);
    }
  }

  ChunkHooks(Arena::Kind kind) : m_arenaIdx(-1U), m_kind(kind) {
    unsigned arenaIdx = je::arenas_extend();
    arena_chunk_hooks(arenaIdx, this);
    m_arenaIdx = arenaIdx;
  }

  enum class JeResult { success, failure };

  unsigned arenaIndex() const {
    return m_arenaIdx;
  }
  Arena::Kind kind() const {
    return m_kind;
  }

 private:
  unsigned m_arenaIdx;
  const Arena::Kind m_kind;

  static folly::AtomicHashMap<size_t, ChunkHooks*> s_chunkLookup;
  static const chunk_hooks_t s_chunkHooks;

  bool isArenaMemory(const void* p) const {
    return Arena::KindMapper::singleton().get(p) == m_kind;
  }

  JeResult chunk_alloc(
      void*& chunk,
      size_t byteSize,
      size_t alignment,
      bool& zero,
      bool& commit) {
    assert(isChunkAligned(byteSize));

    // We don't support asking for explicit addresses...
    if (chunk != nullptr) {
      return JeResult::failure;
    }

    // We require that the chunk alignment is at least a multiple of
    // kJeChunkSize (so the KindMapper will work).
    if (alignment < kJeChunkSize)
      alignment = kJeChunkSize;

#if OBSTACK_WP_FROZEN
    if (m_kind == Arena::Kind::obstack) {
      // In OBSTACK_WP_FROZEN mode we want to allocate chunks such that
      // they're 2*kReadOnlyMirrorSize aligned and mapped so at
      // address+kReadOnlyMirrorSize is a read-only mirror.
      // kReadOnlyMirrorSize must be exactly kJeChunkSize.
      static_assert(mem::kReadOnlyMirrorSize == kJeChunkSize, "");
      if (byteSize != mem::kReadOnlyMirrorSize)
        return JeResult::failure;
      alignment = mem::kReadOnlyMirrorSize * 2;

      chunk = mem::allocReadOnlyMirror(byteSize, alignment);
      commit = true;
      // Tweak the byteSize so the KindMapper registers both parts as "obstack"
      byteSize *= 2;
    } else
#endif
    {
      chunk = (commit ? mem::alloc : mem::reserve)(byteSize, alignment);
    }

    if (!chunk)
      return JeResult::failure;

    Arena::setMemoryKind(chunk, mem::add(chunk, byteSize), m_kind);
    return JeResult::success;
  }

  JeResult chunk_dalloc(void* chunk, size_t size, bool commit) {
    assert(isChunkAligned(chunk));
    assert(isChunkAligned(size));

#if OBSTACK_WP_FROZEN
    if (m_kind == Arena::Kind::obstack) {
      mem::freeReadOnlyMirror(chunk, size);
      // We really allocated double the size for the read-only mirror - See
      // chunk_alloc().
      size *= 2;
    } else
#endif
    {
      mem::free(chunk, size);
    }

    Arena::KindMapper::singleton().erase(chunk, mem::add(chunk, size));
    return JeResult::success;
  }

  JeResult
  chunk_commit(void* chunk, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(chunk));
    assert(mem::isPageAligned(chunk));
    assert(mem::isPageAligned(size));

    void* p = reinterpret_cast<char*>(chunk) + offset;
    mem::commit(p, length);

    return JeResult::success;
  }

  JeResult
  chunk_decommit(void* chunk, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(chunk));
    assert(isChunkAligned(chunk));
    assert(isChunkAligned(size));

    void* p = reinterpret_cast<char*>(chunk) + offset;
    mem::decommit(p, length);
    return JeResult::success;
  }

  JeResult chunk_purge(void* chunk, size_t size, size_t offset, size_t length) {
    assert(isArenaMemory(chunk));
    assert(isChunkAligned(chunk));
    assert(isChunkAligned(size));
    assert(mem::isPageAligned(offset));
    assert(mem::isPageAligned(length));
    mem::purge_forced(chunk, size);
    return JeResult::success;
  }

  JeResult chunk_split(
      void* chunk,
      size_t size,
      size_t size_a,
      size_t size_b,
      bool committed) {
    assert(isArenaMemory(chunk));

    // We can split any larger chunks down to smaller chunks
    if (!isChunkAligned(chunk) || !isChunkAligned(size_a) ||
        !isChunkAligned(size_b)) {
      return JeResult::failure;
    }

    return JeResult::success;
  }

  JeResult chunk_merge(
      void* chunk_a,
      size_t size_a,
      void* chunk_b,
      size_t size_b,
      bool committed) {
    assert(isArenaMemory(chunk_a));
    assert(isChunkAligned(chunk_a));
    assert(isChunkAligned(size_a));
    assert(isArenaMemory(chunk_b));
    assert(isChunkAligned(chunk_b));
    assert(isChunkAligned(size_b));
    assert(
        (mem::add(chunk_a, size_a) == chunk_b) ||
        (mem::add(chunk_b, size_b) == chunk_a));

    // Merging is just fine with us.
    return JeResult::success;
  }

  static void* static_alloc(
      void* chunk,
      size_t size,
      size_t alignment,
      bool* zero,
      bool* commit,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    if (i == s_chunkLookup.end())
      return nullptr;
    if (i->second->chunk_alloc(chunk, size, alignment, *zero, *commit) ==
        ChunkHooks::JeResult::success) {
      return chunk;
    } else {
      return nullptr;
    }
  }

  static bool
  static_dalloc(void* chunk, size_t size, bool commit, unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_dalloc(chunk, size, commit) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_commit(
      void* chunk,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_commit(chunk, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_decommit(
      void* chunk,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_decommit(chunk, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_purge(
      void* chunk,
      size_t size,
      size_t offset,
      size_t length,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_purge(chunk, size, offset, length) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_split(
      void* chunk,
      size_t size,
      size_t size_a,
      size_t size_b,
      bool committed,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_split(chunk, size, size_a, size_b, committed) ==
        ChunkHooks::JeResult::failure);
  }

  static bool static_merge(
      void* chunk_a,
      size_t size_a,
      void* chunk_b,
      size_t size_b,
      bool committed,
      unsigned arenaIndex) {
    auto i = s_chunkLookup.find(arenaIndex);
    // returns false on success!?!
    if (i == s_chunkLookup.end())
      return true;
    return (
        i->second->chunk_merge(chunk_a, size_a, chunk_b, size_b, committed) ==
        ChunkHooks::JeResult::failure);
  }

  static void arena_chunk_hooks(unsigned arena, ChunkHooks* hooks) {
    assert(hooks->m_arenaIdx == -1U);
    auto i = s_chunkLookup.insert(arena, hooks);
    if (!i.second) {
      // replace hook on this arena
      i.first->second->m_arenaIdx = -1U;
      i.first->second = hooks;
    } else {
      // first hook for this arena
      static const Mib mib{"arena.0.chunk_hooks"};
      mallctl_by_mib_W(mib(arena), s_chunkHooks);
    }
  }
};

folly::AtomicHashMap<size_t, ChunkHooks*> ChunkHooks::s_chunkLookup{4};

const chunk_hooks_t ChunkHooks::s_chunkHooks{
    static_alloc,
    static_dalloc,
    static_commit,
    static_decommit,
    static_purge,
    static_split,
    static_merge,
};
} // namespace je
} // anonymous namespace
} // namespace skip
