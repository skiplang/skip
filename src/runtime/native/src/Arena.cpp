/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Arena.h"

#include "skip/memory.h"
#include "skip/objects.h"
#include "skip/Obstack.h"
#include "skip/Refcount.h"
#include "skip/SmallTaggedPtr.h"
#include "skip/util.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

namespace {

using namespace skip;

// These are JeMalloc constants, checked in JeMallocSanityCheck.
// Also used by the KindMapper.
constexpr size_t kJeChunkSizeLog2 = 21; // 2MiB chunk size
constexpr size_t kJeChunkSize = (size_t)1 << kJeChunkSizeLog2;

static_assert(
    kJeChunkSizeLog2 == skip::Arena::KindMapper::kKmSlabSizeLog2,
    "Block size mismatch");

constexpr size_t kKindBitsPerSlot = 2;
constexpr size_t kKindMask = ((1 << kKindBitsPerSlot) - 1);
constexpr size_t kKindBitsOnes = size_t(0x5555555555555555ULL);

template <typename T>
constexpr bool isChunkAligned(T p) {
  return mem::isAligned(p, kJeChunkSize);
}
} // anonymous namespace

#if USE_JEMALLOC
// ----------------------------------------------------------------------

#define ARENA_PRIVATE 1
#include "skip/detail/jemalloc_common.h"
static_assert(JEMALLOC_VERSION_MAJOR == 5);
#include "skip/detail/jemalloc5.h"
#undef ARENA_PRIVATE

namespace skip {
namespace {

struct JeThreadCache {
  ~JeThreadCache() {
    je::tcache_delete(m_id);
  }

  JeThreadCache() : m_id{je::tcache_create()} {}

  const unsigned m_id;
};

struct ArenaData : private je::ChunkHooks {
  explicit ArenaData(Arena::Kind kind)
      : je::ChunkHooks(kind), m_arenaTCache() {}

  void* alloc(size_t sz, size_t align) _MALLOC_ATTR(1) {
    assert(sz > 0);

    // JeMalloc has a bug where alignment of > 4k "leaks" memory.  What's going
    // on is that when you ask for an 8k alignment it has to allocate a 12k
    // extent.  It takes the aligned 8k and frees the remaining 4k - putting it
    // on the 4k free line.  When you free the 8k it puts it onto the 8k free
    // line.  Then when you ask for another aligned 8k it looks for 12k and sees
    // that no 12k blocks are free.  Eventually the 4k lines and 8k lines will
    // be merged - but that can take a while.
    assert(align <= 4096);

    // All obstack allocations must be exactly an obstack chunk.
    assert((kind() != Arena::Kind::obstack) || (sz == Obstack::kChunkSize));

    void* p = ::je_mallocx(
        sz,
        MALLOCX_ARENA(arenaIndex()) | MALLOCX_TCACHE(m_arenaTCache.m_id) |
            MALLOCX_ALIGN(align));
    if (!p)
      throw std::bad_alloc();
    return p;
  }

  void free(void* p) {
    ::je_dallocx(
        p, MALLOCX_ARENA(arenaIndex()) | MALLOCX_TCACHE(m_arenaTCache.m_id));
  }

 private:
  const JeThreadCache m_arenaTCache;
};

ArenaData& getDataForKind(Arena::Kind kind) {
  static thread_local ArenaData* iobj = new ArenaData(Arena::Kind::iobj);
  static thread_local ArenaData* large = new ArenaData(Arena::Kind::large);
  static thread_local ArenaData* obstack = new ArenaData(Arena::Kind::obstack);
  static thread_local std::array<ArenaData*, 4> s_dataPerKind{
      {nullptr, iobj, large, obstack}};

  return *s_dataPerKind[(int)kind];
}
} // anonymous namespace

void* Arena::allocAligned(size_t sz, size_t align, Kind kind) {
  return getDataForKind(kind).alloc(sz, align);
}

void Arena::free(void* p) {
  getDataForKind(Arena::rawMemoryKind(p)).free(p);
}

void Arena::free(void* p, Kind kind) {
  getDataForKind(kind).free(p);
}

Arena::Kind Arena::rawMemoryKind(const void* p) {
  return KindMapper::singleton().get(p);
}

void Arena::setMemoryKind(const void* start, const void* end, Kind kind) {
  KindMapper::singleton().set(start, end, kind);
}

void Arena::init() {
  // This is to ensure that the Arena objects are initialized before the
  // thread-local Obstacks (and thus torn down AFTER).
  (void)getDataForKind(Kind::iobj);
  (void)getDataForKind(Kind::large);
  (void)getDataForKind(Kind::obstack);
}
} // namespace skip

// ----------------------------------------------------------------------
#endif // USE_JEMALLOC

namespace skip {

void* Arena::calloc(size_t sz, Kind kind) {
  auto p = allocAligned(sz, 1, kind);
  memset(p, 0, sz);
  return p;
}

Arena::Kind Arena::getMemoryKind(const RObj* robj) {
  return rawMemoryKind(robj->interior());
}

Arena::Kind Arena::getMemoryKind(const LargeObjHeader* large) {
  return rawMemoryKind(large);
}

Arena::KindMapper& Arena::KindMapper::singleton() {
  // Allocate this in the heap so it doesn't get destroyed during
  // shutdown when jemalloc chunk hooks might still need it.
  static auto obj = new KindMapper();
  return *obj;
}

Arena::Kind Arena::KindMapper::get(const void* p) const {
  if (p >= reinterpret_cast<const void*>(1ULL << detail::kMaxPtrBits)) {
    return Kind::unknown;
  }
  auto indexShift = split(p);
  size_t block = m_data[indexShift.first].load(std::memory_order_relaxed);
  auto kind =
      static_cast<Arena::Kind>((block >> indexShift.second) & kKindMask);
  return kind;
}

void Arena::KindMapper::erase(const void* start, const void* end) {
  assert(
      isChunkAligned(start) && isChunkAligned(end) &&
      (mem::sub(end, start) > 0));

  auto indexShift = split(start);
  std::atomic<size_t>* block = &m_data[indexShift.first];

  if (mem::add(start, kKmSlabSize) == end) {
    // common case - only clearing a single slab
    block->fetch_and(
        ~((size_t)kKindMask << indexShift.second), std::memory_order_relaxed);
  } else {
    const auto indexShiftEnd = split(end);
    std::atomic<size_t>* const blockEnd = &m_data[indexShiftEnd.first];

    if ((block < blockEnd) && (indexShift.second != 0)) {
      size_t mask = ((size_t(1) << indexShift.second) - 1);
      block->fetch_and(mask, std::memory_order_relaxed);
      indexShift.second = 0;
      ++block;
    }

    while (block < blockEnd) {
      block->store(0, std::memory_order_relaxed);
      ++block;
    }

    if (indexShift.second < indexShiftEnd.second) {
      size_t mask =
          ((size_t(1) << indexShiftEnd.second) -
           (size_t(1) << indexShift.second));
      block->fetch_and(~mask, std::memory_order_relaxed);
    }
  }
}

void Arena::KindMapper::set(
    const void* start,
    const void* end,
    Arena::Kind kind) {
  assert(
      isChunkAligned(start) && isChunkAligned(end) &&
      (mem::sub(end, start) > 0));
  assert(static_cast<size_t>(kind) < (1 << kKindBitsPerSlot));

  if (kind == Kind::unknown) {
    erase(start, end);
    return;
  }

  auto indexShift = split(start);
  std::atomic<size_t>* block = &m_data[indexShift.first];

  if (mem::add(start, kKmSlabSize) == end) {
    // common case - only setting a single slab
    size_t previous = block->fetch_or(
        (size_t)kind << indexShift.second, std::memory_order_relaxed);
    (void)previous;
    // Ensure that the value was previously 'unknown'
    assert((previous & (kKindMask << indexShift.second)) == 0);
  } else {
    const auto indexShiftEnd = split(end);
    std::atomic<size_t>* const blockEnd = &m_data[indexShiftEnd.first];

    // 'kind' replicated across the entire word
    const size_t fullyMappedKind = kKindBitsOnes * (size_t)kind;

    if ((block < blockEnd) && (indexShift.second != 0)) {
      size_t mask = ~((size_t(1) << indexShift.second) - 1);
      size_t previous =
          block->fetch_or(fullyMappedKind & mask, std::memory_order_relaxed);
      (void)previous;
      assert((previous & mask) == 0);
      indexShift.second = 0;
      ++block;
    }

    while (block < blockEnd) {
#ifndef NDEBUG
      size_t previous =
          block->exchange(fullyMappedKind, std::memory_order_relaxed);
      assert(previous == 0);
#else
      block->store(fullyMappedKind, std::memory_order_relaxed);
#endif
      ++block;
    }

    if (indexShift.second < indexShiftEnd.second) {
      size_t mask =
          ((size_t(1) << indexShiftEnd.second) -
           (size_t(1) << indexShift.second));
      size_t previous =
          block->fetch_or(fullyMappedKind & mask, std::memory_order_relaxed);
      (void)previous;
      assert((previous & mask) == 0);
    }
  }
}

Arena::KindMapper::~KindMapper() {
  mem::free(m_data, dataSize());
  m_data = nullptr;
}

Arena::KindMapper::KindMapper() {
  m_data = static_cast<std::atomic<size_t>*>(mem::alloc(dataSize()));
}

constexpr size_t Arena::KindMapper::dataSize() {
  // Our address space is kMaxPtrBits but our chunks are guaranteed to
  // be aligned kJeChunkSize so we can ignore lower bits than that.
  // Number of slots we need to track.
  return (1ULL << (detail::kMaxPtrBits - kJeChunkSizeLog2)) * kKindBitsPerSlot /
      8;
}

std::pair<size_t, size_t> Arena::KindMapper::split(const void* p) {
  size_t slotNumber = reinterpret_cast<uintptr_t>(p) / kJeChunkSize;
  size_t index = slotNumber / (sizeof(size_t) * 8 / kKindBitsPerSlot);
  size_t shift =
      (slotNumber % (sizeof(size_t) * 8 / kKindBitsPerSlot)) * kKindBitsPerSlot;
  return std::make_pair(index, shift);
}
} // namespace skip
