/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Obstack.h"
#include "skip/Obstack-extc.h"

#include "skip/AllocProfiler.h"
#include "skip/Arena.h"
#include "skip/intern.h"
#include "skip/memory.h"
#include "skip/objects.h"
#include "skip/Process.h"
#include "skip/Refcount.h"
#include "skip/set.h"
#include "skip/SmallTaggedPtr.h"
#include "skip/System.h"
#include "skip/System-extc.h"
#include "skip/util.h"

#include "ObstackDetail.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <map>
#include <stdexcept>
#include <set>
#include <utility>
#include <vector>
#include <array>

#if ENABLE_VALGRIND
#include <valgrind/memcheck.h>
#endif

/*
 * General Obstack layout:
 *
 *
 *                                                        +------m_nextAlloc
 *   Chunk 0        Chunk 1        Chunk 2        Chunk 3 v
 *   +----------+   +----------+   +----------+   +----------+
 *   |    :     +<--|          +<--|  :    :  +<--|    :  :  +<--m_chunk
 *   |    :     |   |          |   |  :    :  |   |    :  :  |
 *   +----------+   +----------+   +----------+   +----------+
 *        ^                           ^    ^           ^
 *        P         :              :  P    P      :    P
 *                  :              :              :
 *0<--Lo(0)<-Lo(0)<-:---Lo(1)<-----:--------------:Lo(3)<-Lo(3)<--+
 *                  :              :              :          m_currentLargeObj
 *                  :              :              :
 *0<------Io(0)<----:--------------:--------------:--Io(3)<--m_currentIObj
 *
 * The Obstack is made up of a chained series of Chunks.  Memory is allocated
 * out of a Chunk until there's no more space left in the Chunk at which point a
 * new chunk is allocated and added to the chain.  m_nextAlloc tracks where the
 * next allocation will take place.
 *
 * Obstack::Pos (noted above as 'P') are just reference points within the
 * Chunks.
 *
 * Large allocations are tracked via a chain of LargeObjHeader
 * structures (noted above as 'Lo') which have a reference back to the
 * owning Chunk.  The newest one is m_currentLargeObj.
 *
 * IObj references (noted above as 'Io') are also tracked so the runtime can
 * decref() them when they are no longer referenced.  These are kept in a
 * hash table chain of IObjRefs.
 *
 * Collectively the externally tracked IObjs and LargeObjHeaders are
 * called Extrefs. Each extref has an associated Pos, indicating its age.
 *
 * Chunks are small ranges with a power-of-2 size, allocated from slabs,
 * which are large ranges with a power-of-2 size. Slabs are currently
 * not shared between Obstacks, so in principle, a Chunk could be up to
 * one whole Slab.
 *
 * The reason for chunks smaller than slabs is they act like semi-spaces
 * when we gc: when we collect, only the end of the chunk containing note
 * needs to be shadow-copied to compact (move vs copy to fresh chunk).
 * A smaller chunk leads to less shadow-copying.
 *
 * When we spawn worker threads, they create new Obstacks with their own
 * slabs and chunks. Chunks are numbered starting with the parent chunk,
 * so the view of each worker is a linear history of chunks. Across all
 * threads they form a tree, with Pos values increasing as you walk from
 * root to leaves.
 *
 * Some terminology:
 *
 * note         An Obstack::Pos or SkipObstackPos dividing the obstack into old
 *              and young objects. A note is "empty" if note==nextAlloc,
 *              when no allocations have occurred. Any note containing an
 *              extref must be non-empty: note < nextAlloc.
 *
 * placeholder  A small allocation whose only purpose is to ensure a note
 *              containing an extref is non-empty.
 *
 * young        An object or extref is "young" relative to a given note,
 *              if its position is >= note.
 *
 * old          Likewise, an object is old relative to a note, if its
 *              position < note.
 *
 * marked       In any collection (see struct Collector), an object is marked
 *              if we have visited it. The mark has different forms:
 *              * young small objects are marked by leaving a forwarding pointer
 *              * young extrefs are marked by updating their pos to note-1.
 *              * old objects and extrefs are marked by inserting them in a
 *              hashtable.
 *
 * pinned       Pinned generally means we cannot copy an object. However, we
 *              intend to use the term more specifically, referring to an object
 *              explicitly created or marked as non-copyable by an external
 *              agent, because there are untracked pointers to it.
 *              Specifically, large objects are not pinned, even though we never
 *              copy them. Old small objects are not considered pinned, because
 *              once the call stack unwinds and they become young (relative to
 *              an older note), we will copy them.
 */

namespace skip {

// tl_obstack holds state for the currently active Obstack; access via
// Obstack::cur() (except for Obstack-extc.h, which is allowed to use
// it directly).
//
// We don't technically need tl_obstack; each Process "knows" its
// associated Obstack, so we could write Obstack::cur() to load an Obstack
// field from Process:cur(). But that would be slower -- the ONLY reason
// tl_obstack exists is to save one level of indirection accessing Obstack
// fields. This complexity is worth it because those fields are so hot.
//
// When we Process::contextSwitchTo a process, the Process moves its Obstack
// fields here for fast access, and when we context switch away from a
// Process, it moves them back to get the latest values.
__thread ObstackBase tl_obstack;

const char* const kCollectModeNames[] = {"runtime-collect",
                                         "manual-collect",
                                         "auto-collect"};
const char* const kSweepModeNames[] = {"runtime-sweep",
                                       "manual-sweep",
                                       "auto-sweep"};

namespace {

#if SKIP_PARANOID
// trash-fill constants
constexpr char kNewChunkTrash ATTR_UNUSED = 0xcc;
constexpr char kDeadChunkTrash ATTR_UNUSED = 0xcd;
constexpr char kSweptChunkTrash ATTR_UNUSED = 0xce;
#endif

// Anything this size or larger is considered a "large" allocation and will
// always be allocated in a side allocation.
constexpr size_t kLargeAllocSize = Obstack::kChunkSize / 2;
constexpr auto kKmSlabSize = Arena::KindMapper::kKmSlabSize;
constexpr auto kChunkSize = ObstackBase::kChunkSize;

struct ObjectSize final {
  ObjectSize(size_t metadataSize, size_t userSize)
      : m_metadataSize(metadataSize), m_userSize(userSize) {}

  size_t metadataSize() const {
    return m_metadataSize;
  }
  size_t userSize() const {
    return m_userSize;
  }
  size_t totalSize() const {
    return m_metadataSize + m_userSize;
  }

  void memcpy(RObj* dst, const RObj* src) const {
    ::memcpy(
        mem::sub(dst, m_metadataSize),
        mem::sub(src, m_metadataSize),
        totalSize());
  }

  void memmove(RObj* dst, const RObj* src) const {
    ::memmove(
        mem::sub(dst, m_metadataSize),
        mem::sub(src, m_metadataSize),
        totalSize());
  }

 private:
  size_t m_metadataSize;
  size_t m_userSize;
};

#if OBSTACK_VERIFY_NOTE
const auto kVerifyNote = parseEnv("OBSTACK_VERIFY_NOTE", 0) != 0;
#else
const auto kVerifyNote = false;
#endif

#if OBSTACK_VERIFY_PARANOID
const auto kVerifyParanoid = parseEnv("OBSTACK_VERIFY_PARANOID", 1) != 0;
#else
const auto kVerifyParanoid = false;
#endif

// kGCManual=false disables explicit collections (@gc and localGc())
const auto kGCManual = parseEnv("SKIP_GC_MANUAL", 1) != 0;

// kGCRatio = (minUsage for next collect / estimate of gc work)
const auto kGCRatio = parseEnvD("SKIP_GC_RATIO", 3);

// kGCSquawk = threshold at which to complain about gc work / reclaimed
const auto kGCSquawk = parseEnvD("SKIP_GC_SQUAWK", kGCRatio* kGCRatio);

// 0 = none
// 1 = abnormalities
// 2 = all collections (1+ roots)
// 3 = include sweeps (0-root collections)
const auto kGCVerbose = parseEnv("SKIP_GC_VERBOSE", 0);

std::chrono::time_point<std::chrono::high_resolution_clock> threadNanos() {
  return std::chrono::high_resolution_clock::now();
}
} // anonymous namespace

const uint64_t kMemstatsLevel = parseEnv("SKIP_MEMSTATS", 0);

struct LargeObjHeader final : private skip::noncopyable {
  using Pos = ObstackDetail::Pos;

  LargeObjHeader* m_prev; // Next older large object
  Pos m_pos; // Position at time of allocation
  const size_t m_size; // not including sizeof(LargeObjHeader)

  static LargeObjHeader* alloc(size_t sz, LargeObjHeader* prev, Pos pos) {
    size_t allocSize = sizeof(LargeObjHeader) + sz;
    return new (Arena::alloc(allocSize, Arena::Kind::large))
        LargeObjHeader(prev, pos, sz);
  }

  static const LargeObjHeader* fromObject(const RObj* obj) {
    // The header is just before the pointed-at object
    auto& type = obj->type();
    size_t const metadataSize = type.uninternedMetadataByteSize();
    return reinterpret_cast<const LargeObjHeader*>(
        mem::sub(obj, metadataSize + sizeof(LargeObjHeader)));
  }

  static LargeObjHeader* fromObject(RObj* obj) {
    // Reuse the const implementation.
    return const_cast<LargeObjHeader*>(
        fromObject(const_cast<const RObj*>(obj)));
  }

  LargeObjHeader(LargeObjHeader* prev, Pos pos, size_t sz)
      : m_prev(prev), m_pos(std::move(pos)), m_size(sz) {}

  void attach(ObstackDetail& stack) {
    stack.m_stats.modifyLarge(1, m_size);
    stack.m_stats.allocLarge(m_size);
  }

  void detach(ObstackDetail& stack) {
    stack.m_stats.modifyLarge(-1, -(ssize_t)m_size);
  }
};

const size_t numAlignBits = 14;

struct Chunk;
struct ChunkHeader {
  explicit ChunkHeader(Chunk* prev, size_t startingGeneration)
      : m_prev_gen(prev, startingGeneration) {}

  // tag: generation of chunk, ptr: previous chunk
  SmallTaggedPtr<
      Chunk, // type of object pointed to
      -1, // numTagBits = max that fit in 64 bits
      false, // safeToLoadBefore
      false, // safeToLoadAfter
      false, // pack
      numAlignBits> // numAlignBits
      m_prev_gen;
};

constexpr size_t kChunkCapacity =
    kChunkSize - sizeof(ChunkHeader) - Obstack::kAllocAlign;

// Chunks are always aligned to kChunkSize. Using alignas(128)
// ensures sizeof(Chunk) == kChunkSize when kAllocAlign < 128.
struct alignas(128) Chunk final : private ChunkHeader {
  static constexpr auto kAllocAlign = Obstack::kAllocAlign;
  using Pos = ObstackDetail::Pos;

  Chunk* prev() const {
    return m_prev_gen.ptr();
  }

  size_t generation() const {
    return m_prev_gen.tag();
  }

  void setPrev(Chunk* prev) {
    m_prev_gen.assign(prev, prev->generation() + 1);
  }

  void setGeneration(size_t gen) {
    m_prev_gen.assign(prev(), gen);
  }

  static Chunk* fromRaw(void* ptr) {
    return reinterpret_cast<Chunk*>(
        reinterpret_cast<uintptr_t>(ptr) & ~(kChunkSize - 1));
  }

  Pos beginPos() const {
    return Pos::fromRaw(beginAddr());
  }

  Pos endPos() const {
    return Pos::fromRaw(endAddr());
  }

  const void* beginAddr() const {
    return m_data.begin();
  }

  void* beginAddr() {
    return m_data.begin();
  }

  const void* endAddr() const {
    return m_data.end();
  }

  explicit Chunk(Chunk* prev)
      : Chunk(prev, prev ? prev->generation() + 1 : 0) {}

  explicit Chunk(Pos pin) : Chunk(nullptr, pin.generation() + 1) {}

  ~Chunk() {
#if SKIP_PARANOID && !FOLLY_SANITIZE_ADDRESS
    m_data.fill(kDeadChunkTrash);
#endif
#if ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(beginAddr(), mem::sub(endAddr(), beginAddr()));
#endif
  }

 private:
  explicit Chunk(Chunk* prev, size_t startingGeneration)
      : ChunkHeader(prev, startingGeneration) {
#if SKIP_PARANOID && !FOLLY_SANITIZE_ADDRESS
    m_data.fill(kNewChunkTrash);
#endif
#if ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(beginAddr(), mem::sub(endAddr(), beginAddr()));
#endif
  }

  std::array<char, kChunkCapacity> m_data alignas(kAllocAlign);
};

ObstackDetail::Pos ObstackDetail::Pos::fromRaw(const void* addr) {
  assert(Arena::rawMemoryKind(addr) == Arena::Kind::obstack);
  const auto chunk = reinterpret_cast<const Chunk*>(
      reinterpret_cast<uintptr_t>(addr) & ~(kChunkSize - 1));
  return Pos(
      chunk->generation() * kChunkSize +
      (reinterpret_cast<uintptr_t>(addr) & (kChunkSize - 1)));
}

ObstackDetail::Pos ObstackDetail::Pos::fromAddress(SkipObstackPos p) {
  return fromRaw(p.ptr);
}

ObstackDetail::Pos ObstackDetail::Pos::fromAddress(const RObj* robj) {
  return fromRaw(robj->interior());
}

size_t ObstackDetail::Pos::generation() const {
  return m_position / kChunkSize;
}

size_t ObstackDetail::Pos::offsetInChunk() const {
  return m_position & (kChunkSize - 1);
}

Obstack::~Obstack() {
  if (const auto detail = m_detail) {
    const auto nextAlloc = m_nextAlloc;
    m_nextAlloc = nullptr; // guard against re-entry
    m_detail = nullptr;
    for (Chunk *prev, *chunk = Chunk::fromRaw(nextAlloc); chunk; chunk = prev) {
      prev = chunk->prev();
      if (!prev && kMemstatsLevel && chunk->generation() == 0) {
        detail->m_stats.reportFinal();
      }
      detail->m_chunkAllocator.deleteChunk(chunk);
    }
    delete detail;
  }
}

ObstackDetail::ObstackDetail() : m_handles(nullptr, nullptr) {}

ObstackDetail::~ObstackDetail() {
  // Delete IObjs
  for (auto& i : m_iobjRefs) {
    decref(i.first);
  }

  // Delete largeObj
  for (LargeObjHeader *prev, *largeObj = m_currentLargeObj; largeObj;
       largeObj = prev) {
    prev = largeObj->m_prev;
    m_stats.modifyLarge(-1, -(ssize_t)largeObj->m_size);
    Arena::free(largeObj, Arena::Kind::large);
  }

  assert(!anyHandles()); // otherwise, who owns them?
}

Obstack::Obstack(std::nullptr_t) noexcept {
  m_nextAlloc = nullptr;
  m_detail = nullptr;
}

Obstack::Obstack(Obstack&& o) noexcept {
  m_nextAlloc = o.m_nextAlloc;
  m_detail = o.m_detail;
  o.m_nextAlloc = nullptr;
  o.m_detail = nullptr;
}

Obstack& Obstack::operator=(Obstack&& o) noexcept {
  assert(!(*this)); // don't clobber valid obstack
  m_nextAlloc = o.m_nextAlloc;
  m_detail = o.m_detail;
  o.m_nextAlloc = nullptr;
  o.m_detail = nullptr;
  return *this;
}

void Obstack::init(void* nextAlloc) {
  m_detail->m_stats.modifyChunk(1);
  m_nextAlloc = nextAlloc;
  // Preallocate a small block.  This guarantees that we can't accidentally try
  // to refer to a negative offset while collecting.
  (void)alloc(kAllocAlign);
  m_detail->m_firstNote = note();
}

Obstack::Obstack() {
  m_detail = new ObstackDetail();
  init(m_detail->m_chunkAllocator.newChunk(nullptr)->beginAddr());
}

Obstack::Obstack(SkipObstackPos resultNote) {
  m_detail = new ObstackDetail();
  init(m_detail->m_chunkAllocator
           .newChunk(ObstackDetail::Pos::fromAddress(resultNote))
           ->beginAddr());
}

// is this Obstack initialized?
Obstack::operator bool() const {
  return m_nextAlloc != nullptr;
}

void ObstackDetail::printMemoryStatistics(Obstack& obstack) const {
  obstack.m_detail->m_stats.report(obstack);
}

void ObstackDetail::printObjectSize(const RObj* o) const {
  size_t total = 0;
  std::vector<const RObj*> pending;
  skip::fast_set<const RObj*> seen;
  seen.insert(o);
  pending.push_back(o);
  while (!pending.empty()) {
    auto next = pending.back();
    pending.pop_back();
    total += next->userByteSize();
    next->forEachRef([&pending, &seen](const RObjOrFakePtr& ref) {
      if (auto obj = ref.asPtr()) {
        if (seen.insert(obj).second)
          pending.push_back(obj);
      }
    });
  }

  fprintf(stderr, "Obstack size of %p: %lu\n", (void*)o, total);
}

size_t Obstack::usage(SkipObstackPos noteAddr) const {
  return m_detail->usage(*this, noteAddr);
}

size_t ObstackDetail::usage(const Obstack& obstack, SkipObstackPos noteAddr)
    const {
  const auto note = Pos::fromAddress(noteAddr);
  size_t sum = Pos::fromRaw(obstack.m_nextAlloc) - note;
  for (auto p = m_currentLargeObj; p && p->m_pos >= note; p = p->m_prev) {
    sum += p->m_size;
  }
  return sum;
}

size_t ObstackDetail::totalUsage(const Obstack& obstack) const {
  return usage(obstack, m_firstNote);
}

struct ObstackDetail::ChunkMap : private skip::noncopyable {
  // Build a set of Chunks. This lets us quickly determine if a pointer is
  // allocated within the Chunks.
  std::set<const void*> m_chunks;
  const Chunk* const m_currentChunk;
  const void* m_currentChunkEnd;

  explicit ChunkMap(const Obstack& obstack)
      : m_currentChunk(Chunk::fromRaw(obstack.m_nextAlloc)),
        m_currentChunkEnd(obstack.m_nextAlloc) {
    for (auto header = m_currentChunk; header; header = header->prev()) {
      m_chunks.insert(header);
    }
  }

  bool contains(void* p) {
    auto it = m_chunks.upper_bound(p);
    if (it == m_chunks.begin())
      return false;
    --it;
    const auto chunk = static_cast<const Chunk*>(*it);
    size_t size = kChunkSize;

    // If p is in the current chunk we want to ensure that it's in the allocated
    // portion.
    if (chunk == m_currentChunk) {
      size = mem::sub(m_currentChunkEnd, chunk);
    }

    return mem::within(p, chunk->beginAddr(), size);
  }
};

void ObstackDetail::verifyInvariants(const Obstack& obstack) const {
  if (!kVerifyParanoid)
    return;

  ChunkMap chunkMap(obstack);

  DEBUG_ONLY const auto nextAllocPos = Pos::fromRaw(obstack.m_nextAlloc);

  if (kVerifyNote) {
    // visit the valid notes youngest to oldest. Between each note,
    // large object and iobj positions must be between the current and next
    // younger note, but positions may be in any order within that range.
    auto max = nextAllocPos;
    auto large = m_currentLargeObj;
    auto iobj = m_currentIObj;
    for (auto it = m_validNotes.crbegin(); it != m_validNotes.crend(); ++it) {
      while (large && large->m_pos >= *it) {
        assert(large->m_pos < max);
        large = large->m_prev;
      }
      while (iobj && m_iobjRefs.at(iobj).m_pos >= *it) {
        assert(m_iobjRefs.at(iobj).m_pos < max);
        iobj = m_iobjRefs.at(iobj).m_prev;
      }
      max = *it;
    }
    assert(!large);
    assert(!iobj);
  }

  // check iobjRefs table and stats
  {
    size_t count = 0;
    for (auto iobj = m_currentIObj; iobj; iobj = m_iobjRefs.at(iobj).m_prev) {
      assert(Arena::getMemoryKind(iobj) == Arena::Kind::iobj);
      ++count;
    }
    assert(m_stats.getCurInternCount() == count);
    assert(m_iobjRefs.size() == count);
  }

  // check large object stats
  {
    size_t size = 0;
    size_t count = 0;
    for (auto large = m_currentLargeObj; large; large = large->m_prev) {
      assert(Arena::getMemoryKind(large) == Arena::Kind::large);
      size += large->m_size;
      ++count;
    }
    assert(m_stats.getCurLargeCount() == count);
    assert(m_stats.getCurLargeSize() == size);
  }

  // chunk allocation stats
  {
    size_t count = 0;
    for (auto chunk = Chunk::fromRaw(obstack.m_nextAlloc); chunk;
         chunk = chunk->prev()) {
      ++count;
    }
    assert(m_stats.getCurChunkCount() == count);
  }
}

#ifndef NDEBUG

bool Obstack::DEBUG_isAlive(const RObj* robj) {
  using Pos = ObstackDetail::Pos;
  // For tests, this is just a rough approximation of "is it a valid pointer?"
  auto kind = Arena::getMemoryKind(robj);
  if (kind == Arena::Kind::large) {
    const auto header = LargeObjHeader::fromObject(robj);
    if (header->m_pos <= Pos::fromRaw(m_nextAlloc)) {
      for (auto p = m_detail->m_currentLargeObj; p; p = p->m_prev) {
        if (p == header)
          return true;
      }
    }
    return false;
  } else if (kind == Arena::Kind::obstack) {
    return Pos::fromAddress(robj) <= Pos::fromRaw(m_nextAlloc);
  } else {
    // Maybe IObj should be considered "alive"?
    return false;
  }
}

#endif

void Obstack::threadInit() {
  static_assert(sizeof(Obstack) == sizeof(ObstackBase), "");
  static_assert(alignof(Obstack) <= alignof(ObstackBase), "");

  // Start this out as "no obstack".
  new (&tl_obstack) Obstack(nullptr);
}

// Return the Obstack for the current thread.
Obstack& Obstack::cur() {
  return static_cast<Obstack&>(tl_obstack);
}

Obstack* Obstack::curIfInitialized() {
  return cur().m_nextAlloc ? &cur() : nullptr;
}

SkipObstackPos Obstack::note() {
  if (kVerifyNote) {
    // In OBSTACK_VERIFY_NOTE mode we keep a list of noted positions.
    // Do not remember duplicates.
    auto note = ObstackDetail::Pos::fromRaw(m_nextAlloc);
    auto& validNotes = m_detail->m_validNotes;
    assert(validNotes.empty() || validNotes.back() <= note);
    if (validNotes.empty() || validNotes.back() < note) {
      validNotes.push_back(note);
    }
  }
  return {m_nextAlloc};
}

// Used internally when we know sz is small. Overflow may occur,
// and we guarantee not to trash fill.
void* Obstack::allocSmall(size_t sz) {
  assert(sz == roundUp(sz, kAllocAlign));
  assert(sz < kLargeAllocSize);
  const auto mem = m_nextAlloc;
  const auto next = mem::add(mem, sz);
  if (LIKELY(((uintptr_t)next ^ (uintptr_t)mem) < kChunkSize)) {
    m_nextAlloc = next;
    // if we're in the first chunk, may read from newly alloc'd memory, but
    // copy to shadow chunk.
    m_detail->m_stats.allocSmall(sz);
    return mem;
  }
  return m_detail->allocOverflow(sz, *this);
}

void* ObstackDetail::allocOverflow(size_t sz, Obstack& obstack) {
  // chunk overflow
  const auto prev = Chunk::fromRaw(obstack.m_nextAlloc);
  m_stats.allocFragment(mem::sub(prev->endAddr(), obstack.m_nextAlloc));
  const auto chunk = m_chunkAllocator.newChunk(prev);
  m_stats.modifyChunk(1);
  // alloc is guaranteed to succeed now
  const auto mem = chunk->beginAddr();
  obstack.m_nextAlloc = mem::add(mem, sz);
  assert((uintptr_t(mem) ^ uintptr_t(obstack.m_nextAlloc)) < kChunkSize);
#if ENABLE_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(mem, sz);
#endif
  m_stats.allocSmall(sz);
  return mem;
}

void* Obstack::alloc(size_t sz) {
  // To keep things simple all allocations must be pointer aligned.
  sz = roundUp(sz, kAllocAlign);

  if (AllocProfiler::enabled()) {
    m_detail->m_allocProfiler.logAllocation(sz);
  }

  // NOTE: The allocator is split into alloc() and allocLarge() so that the
  // easy part can be inlined (need to expose kLargeAllocSize).
  static_assert(kLargeAllocSize < kChunkSize - sizeof(Chunk) - kAllocAlign, "");
  if (LIKELY(sz < kLargeAllocSize)) {
    const auto mem = allocSmall(sz);
#if ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_UNDEFINED(mem, sz);
#endif
    return mem;
  }
  return m_detail->allocLarge(sz, *this);
}

// this does double duty for large and pinned objects.
void* ObstackDetail::allocLarge(size_t sz, Obstack& obstack) {
  assert(sz == roundUp(sz, kAllocAlign));
  m_stats.allocPlaceholder(kAllocAlign);
  const auto placeholder = obstack.allocSmall(kAllocAlign);
  const auto pos = Pos::fromRaw(placeholder);
  m_currentLargeObj = LargeObjHeader::alloc(sz, m_currentLargeObj, pos);
  m_currentLargeObj->attach(*this);
#if ENABLE_VALGRIND
  VALGRIND_MAKE_MEM_UNDEFINED(m_currentLargeObj + 1, sz);
#endif
  return m_currentLargeObj + 1;
}

void* Obstack::allocPinned(size_t sz) {
  return m_detail->allocPinned(sz, *this);
}

void* ObstackDetail::allocPinned(size_t sz, Obstack& obstack) {
  return allocLarge(roundUp(sz, kAllocAlign), obstack);
}

IObjOrFakePtr Obstack::intern(RObjOrFakePtr obj) {
  return m_detail->intern(obj, *this);
}

IObjOrFakePtr ObstackDetail::intern(RObjOrFakePtr obj, Obstack& obstack) {
  if (auto p = obj.asPtr()) {
    // maybe allocate a new IObjRef
    auto iobj = skip::intern(p);
    auto& delegate = iobj->refcountDelegate();
    auto inserted = m_iobjRefs.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(&delegate),
        std::forward_as_tuple(Pos{0}, m_currentIObj));
    if (inserted.second) {
      // We didn't know about this IObj yet.
      m_currentIObj = &delegate;
      m_stats.modifyIntern(1);
      // Replace Pos{0} with new placeholder.
      m_stats.allocPlaceholder(kAllocAlign);
      const auto placeholder = obstack.allocSmall(kAllocAlign);
      inserted.first->second.m_pos = Pos::fromRaw(placeholder);
    } else {
      // We already knew about this IObj - ditch the implied reference from
      // skip::intern().
      bool check ATTR_UNUSED = decrefToNonZero(delegate.refcount());
      assert(check);
    }
    return iobj;
  }
  // obj is fake ptr
  return IObjOrFakePtr(obj.bits());
}

IObjOrFakePtr Obstack::registerIObj(IObjOrFakePtr obj) {
  return m_detail->registerIObj(obj, *this);
}

IObjOrFakePtr ObstackDetail::registerIObj(IObjOrFakePtr obj, Obstack& obstack) {
  if (auto iobj = obj.asPtr()) {
    assert(iobj->isInterned());

    auto& delegate = iobj->refcountDelegate();
    auto inserted = m_iobjRefs.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(&delegate),
        std::forward_as_tuple(Pos{0}, m_currentIObj));
    if (inserted.second) {
      // We didn't know about this IObj yet.
      incref(&delegate);
      m_currentIObj = &delegate;
      m_stats.modifyIntern(1);
      // Replace Pos{0} with new placeholder.
      m_stats.allocPlaceholder(kAllocAlign);
      const auto placeholder = obstack.allocSmall(kAllocAlign);
      inserted.first->second.m_pos = Pos::fromRaw(placeholder);
    }
  }

  return obj;
}

void ObstackDetail::stealIObj(IObj* iobj, Pos pos) {
  auto& delegate = iobj->refcountDelegate();
  auto it = m_iobjRefs.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(&delegate), // IObj* key
      std::forward_as_tuple(pos, m_currentIObj) // IObjRef val
  );
  if (it.second) {
    // first time we've seen this iobj; transfer ownership, preserve count
    m_currentIObj = &delegate;
    m_stats.modifyIntern(1);
  } else {
    // dest and source both tracked this iobj
    decrefToNonZero(delegate.refcount());
  }
}

void* Obstack::calloc(size_t sz) {
  return memset(alloc(sz), 0, sz);
}

#if OBSTACK_VERIFY_NOTE
// Discard any m_validNotes younger (higher) than note, then assert that
// note is valid (present in the vector).
void ObstackDetail::verifyNote(const Pos note) {
  while (!m_validNotes.empty() && m_validNotes.back() > note) {
    m_validNotes.pop_back();
  }
  assert(!m_validNotes.empty() && m_validNotes.back() == note);
}
#endif

// We need a placeholder if this note is completely empty, so the next
// note will be guaranteed to be higher than this note.
void ObstackDetail::allocPlaceholder(Pos note, Obstack& obstack) {
  if (Pos::fromRaw(obstack.m_nextAlloc) == note) {
    m_stats.allocPlaceholder(kAllocAlign);
    (void)obstack.allocSmall(kAllocAlign);
  }
  assert(note < Pos::fromRaw(obstack.m_nextAlloc));
}

// Visit all large objects at pos >= markPos. Any at markPos are moved
// to note. Any higher than markPos are reclaimed. Return the oldest
// surviving large object (if any).
LargeObjHeader* ObstackDetail::sweepLargeObjects(
    Obstack& obstack,
    const Pos markPos,
    const Pos note,
    SweepStats& sweepStats) {
  assert(markPos <= note);
  auto large = m_currentLargeObj;
  if (!large || large->m_pos < markPos) {
    return large;
  }
  // Go through and delete large objects. Be careful: large objects which
  // were marked had their m_pos changed but were not moved in the chain.
  // Once we see anything older than markPos we can stop.
  LargeObjHeader* survivor = nullptr;
  auto lastPrevPtr = &m_currentLargeObj;
  for (LargeObjHeader* prev; large && large->m_pos >= markPos; large = prev) {
    prev = large->m_prev;
    if (large->m_pos > markPos) {
      // unmarked: delete it!
      large->detach(*this);
      Arena::free(large, Arena::Kind::large);
    } else {
      // m_pos == markPos: Keep it. NB: survivors are re-linked in reverse
      // order, but they all get the same pos.
      large->m_pos = note;
      *lastPrevPtr = large;
      lastPrevPtr = &large->m_prev;
      survivor = large;
      sweepStats.largeYoungSurvivors++;
    }
  }
  if (survivor)
    allocPlaceholder(note, obstack);
  *lastPrevPtr = large;
  return survivor;
}

void ObstackDetail::sweepChunks(Chunk* collectChunk, void* oldNextAlloc) {
  auto chunk = Chunk::fromRaw(oldNextAlloc);
  if (chunk != collectChunk) {
    ssize_t delta = 0;
    do {
      auto prev = chunk->prev();
      m_chunkAllocator.deleteChunk(chunk);
      chunk = prev;
      --delta;
    } while (chunk != collectChunk);
    m_stats.modifyChunk(delta);
    m_chunkAllocator.collectGarbage();
  }
}

void ObstackDetail::sweepIObjs(Obstack& obstack, Pos markPos, Pos collectNote) {
  auto currentIObj = m_currentIObj;
  if (!currentIObj)
    return;
  auto& currentRef = m_iobjRefs.find(currentIObj)->second;
  if (currentRef.m_pos < markPos) {
    verifyInvariants(obstack);
    return;
  }

  // Go through and delete IObjs. Be careful: IObjs which were kept
  // alive have had their m_position changed but were not moved in the chain.

  // list of IObj* we will decref after re-positioning marked refs.
  std::vector<IObj*> pendingDecrefs;

  // We've been putting marked IObjs at markPos(). Once we see
  // anything older than that we can stop.

  auto iobj = currentIObj;
  IObj* prevSaved = nullptr;
  IObj* oldestSaved = nullptr;
  for (IObj* prev; iobj; iobj = prev) {
    const auto it = m_iobjRefs.find(iobj);
    auto& ref = it->second;
    if (ref.m_pos < markPos) {
      break;
    }
    prev = ref.m_prev;

    if (ref.m_pos >= collectNote) {
      // delete the IObjRef, plan to decref the IObj
      m_stats.modifyIntern(-1);
      m_iobjRefs.erase(it);
      if (!decrefToNonZero(iobj->refcount())) {
        pendingDecrefs.push_back(iobj);
      }
    } else {
      // Keep it. ref.m_pos < collectNote; and we only get this far if
      // ref.m_pos >= markPos(), implying ref.m_pos == markPos().
      assert(ref.m_pos == markPos);
      ref.m_pos = collectNote;
      ref.m_prev = prevSaved;
      prevSaved = iobj;
      if (!oldestSaved)
        oldestSaved = iobj;
    }
  }
  if (prevSaved) {
    allocPlaceholder(collectNote, obstack);
    m_iobjRefs.at(oldestSaved).m_prev = iobj;
    m_currentIObj = prevSaved;
  } else {
    m_currentIObj = iobj;
  }

  // this assumes we swept large objects and chunks before iobjs
  verifyInvariants(obstack);
  if (!pendingDecrefs.empty()) {
    for (auto dead : pendingDecrefs) {
      decref(dead);
    }
    verifyInvariants(obstack);
  }
}

ObstackDetail::SweepStats ObstackDetail::sweep(
    Obstack& obstack,
    SkipObstackPos note,
    void* oldNextAlloc) {
#if SKIP_PARANOID
  // trash fill remainder of current chunk
  memset(
      obstack.m_nextAlloc,
      kSweptChunkTrash,
      mem::sub(
          Chunk::fromRaw(obstack.m_nextAlloc)->endAddr(), obstack.m_nextAlloc));
#endif
  SweepStats stats;
  // If collectNote == nextAlloc, we may allocate one placeholder for any
  // surviving extrefs.
  const auto collectNote = Pos::fromAddress(note);
  const auto markPos = collectNote - 1;
  sweepLargeObjects(obstack, markPos, collectNote, stats);
  sweepChunks(Chunk::fromRaw(note.ptr), oldNextAlloc);
  // this must be last, so decrefs may safely re-enter
  sweepIObjs(obstack, markPos, collectNote);
  return stats;
}

/*
 * Obstack garbage collector: a semi-space(*) collector for the young portion
 * of the Obstack [note, nextAlloc).
 *
 * The Collector constructor initializes state and immediately resets the
 * the Obstack to the note being collected, before visiting roots, so we
 * can begin allocating immediately during the copying phase of collect().
 *
 * Copying:
 *
 * The range [note, chunkEnd) of the oldest chunk will contain compacted
 * live small objects, by first copying objects to m_shadow. Once the range
 * is filled, objects are copied to new chunks, which are immediately added
 * to the active Obstack. Once collection is complete, the active portion
 * of m_shadow is copied back to the oldest chunk, and the remaining old
 * chunks are freed by sweepChunks().
 *
 * (*) Unlike a textbook semi-space collector where all live objects are
 * copied to to-space, we compact the tail of the oldest chunk by double
 * copying, as described above.
 *
 * Small objects that are copied leave behind a forwarding pointer in the
 * vtable slot, which also serves as a "mark". Subsequent pointers to the
 * same object are updated to the target address.
 *
 * Live large objects are not copied. Instead, they are logically attached
 * to the current note position, and marked as seen, by updating
 * LargeObjHeader::m_pos to collectNote-1, aka markPos(). Since they
 * are not copied, no subsequent pointers are updated.
 *
 * During the copying phase, we also encounter pointers to older large or
 * small obstack objects. We don't copy them, and cannot mutate them, so they
 * are marked by inserting their address into the m_marked hash-set.
 *
 * Any IObjs found during the copying phase are inserted into
 * Obstack::m_iobjRefs (possibly creating & linking a new IObjRef), and
 * incref'd the first time.
 *
 * Cleanup:
 *
 * After copying is finished, young reachable large objects that were attached
 * to markPos are now updated again to collectNote. This way, we leave marked
 * large objects in the current note, so they are subject to reclaimation in
 * a subsequent collect().
 *
 * Visit young IObjRefs in Obstack::m_iobjRefs, reset live ones to collectNote,
 * and decref the rest.
 *
 * Young chunks that were evicted are deleted by walking the chunk chain
 * starting at m_oldNextAlloc back to m_collectChunk, then giving
 * ChunkAllocator a chance to shrink its freelist and release slabs in
 * collectGarbage().
 */
struct ObstackDetail::Collector {
  Obstack& m_obstack;

  // "Young" poitions >= collectNote are eligible for collection.
  const SkipObstackPos m_collectAddr;
  const Pos m_collectNote;
  Chunk* const m_collectChunk; // Chunk containing collectAddr

  // Allocation cursor before collection, saved so we can walk the evicted
  // young chunks after collection, in sweepChunks().
  void* const m_oldNextAlloc;

  const size_t m_preUsage; // memory usage at collection start
  const std::chrono::time_point<std::chrono::high_resolution_clock>
      m_preNs; // timestamp at start
  int64_t m_markNs;
  const CollectMode m_mode;
  size_t m_rootSize{0};
  size_t m_markCount{0}; // number objects marked reachable
  size_t m_scanCount{0}; // number of pointers scanned, including roots
  size_t m_shadowVol{0}; // bytes copied back from m_shadow
  size_t m_copyVol{0}; // bytes copied out of young chunks
  size_t m_largeYoungCount{0}; // number of large young objects marked

  struct WorkItem {
    RObj* m_target;
    Type* m_type;

    WorkItem(RObj* target, Type& type) : m_target(target), m_type(&type) {}
  };

  std::vector<WorkItem> m_workQueue;

  // When copying, this data is used instead of the actual chunk for
  // the first chunk (which is shared between old and young objects).
  std::array<char, kChunkSize> m_shadow alignas(kAllocAlign);

  // Mark state for large objects, and older small objects that were not
  // copied: objects in the set are considered marked. We only insert objects
  // that we can't mark some other way: either a forwarding pointer or
  // temporarily setting m_pos=markPos().
  skip::fast_set<void*> m_marked;

  Collector(Obstack& obstack, SkipObstackPos note, CollectMode mode)
      : m_obstack(obstack),
        m_collectAddr(note),
        m_collectNote(Pos::fromAddress(note)),
        m_collectChunk(Chunk::fromRaw(note.ptr)),
        m_oldNextAlloc(obstack.m_nextAlloc),
        m_preUsage(obstack.usage(note)),
        m_preNs(threadNanos()),
        m_mode(mode) {}

  ~Collector() {
    m_obstack.m_detail->m_stats.allocGc(m_copyVol);
    m_obstack.m_detail->m_stats.allocShadow(m_shadowVol);
    const auto postUsage = m_obstack.usage(m_collectAddr);
    const auto freed = m_preUsage - postUsage;
    const auto scanVol = m_scanCount * sizeof(void*);
    auto& stats = m_obstack.m_detail->m_stats;
    stats.gcReclaim(freed);
    stats.gcVisit(m_markCount);
    stats.gcScan(scanVol);
    if (kGCVerbose >= 1) {
      const auto min = m_obstack.m_detail->m_minUsage;
      const auto total = m_obstack.m_detail->totalUsage(m_obstack);
      const auto workVol = scanVol + m_copyVol + m_shadowVol;
      if (workVol > std::max(freed, kChunkSize) * kGCSquawk) {
        fprintf(
            stderr,
            "%s low-yield: eligible %lu min %lu scan %lu copy %lu "
            "freed %lu survived %lu total %lu\n",
            kCollectModeNames[(int)m_mode],
            m_preUsage,
            min,
            scanVol,
            m_copyVol,
            freed,
            postUsage,
            total);
      } else if (kGCVerbose >= 2) {
        fprintf(
            stderr,
            "%s eligible %lu %s min %lu survived %lu work %lu total %lu\n",
            kCollectModeNames[(int)m_mode],
            m_preUsage,
            (m_preUsage >= min ? ">=" : "<"),
            min,
            postUsage,
            workVol,
            total);
      }
    }
  }

  size_t collect(RObjOrFakePtr* const roots, const size_t rootSize) {
    m_rootSize += rootSize;

    // reset the allocation cursor
    m_obstack.m_nextAlloc = m_collectAddr.ptr;

    // discover & copy root-reachable objects
    if (rootSize != 1 || !quickCollectOne(roots[0])) {
      // we may have to use m_shadow to compact the collect chunk.

      // visit all explicit roots
      for (size_t i = 0; i < rootSize; ++i) {
        visitRoot(roots[i]);
      }

      // visit all handles.
      m_obstack.m_detail->eachHandle(
          [this](RObjHandle& h) { visitRoot(h.m_robj); });

      // Now that we finished copying objects we need to copy from our
      // shadow to the collect chunk.
      copyShadowToCollectChunk();
    }
    m_markNs = (threadNanos() - m_preNs).count();
    DEBUG_ONLY const auto stats =
        m_obstack.m_detail->sweep(m_obstack, m_collectAddr, m_oldNextAlloc);
    assert(stats.largeYoungSurvivors == m_largeYoungCount);

    // compute new usage threshold as a simple multiple of the "work"
    // this collection performed, expressed as bytes. The work is proportional
    // to the amount of memory scanned (pointers) and copied (pointers and
    // non-pointers). Importantly, it includes old and pinned objects we
    // scanned but did not copy.
    return (m_scanCount * sizeof(RObjOrFakePtr) + m_copyVol + m_shadowVol) *
        kGCRatio;
  }

 private:
  Pos markPos() const {
    // This is the pos where we're going to stick Extrefs which are
    // 'marked'. It's just behind m_collectNote to distinguish them from
    // young objects (>= collectNote) and unmarked old objects (< markPos()).
    return m_collectNote - 1;
  }

  // positions < markPos are "old" objects (allocated before collectNote)
  // that we must mark and scan, but not mutate.
  //
  // positions == markPos are for marked ExtRefs. After collection, they will
  // be moved to a newly allocated placeholder pos.
  //
  // positions > markPos (i.e. >= collectNote) are "young" objects and
  // are candidates to be reclaimed.

  bool isOldSmallObject(const RObj* p) const {
    return Pos::fromAddress(p) < markPos();
  }

  bool isOldLargeObject(const RObj* p) const {
    auto header = LargeObjHeader::fromObject(p);
    return header->m_pos < markPos();
  }

  // Return true if root does not contain any references. If root is an object,
  // copy it (or adjust its pos) to m_collectNote.
  bool quickCollectOne(RObjOrFakePtr& root) {
    if (m_obstack.anyHandles()) {
      // the general collector supports handles as roots
      return false;
    }
    // Right now, only objects which are guaranteed not to contain any
    // references are considered quick. We could do more work and actually
    // analyze the object to see if all the references point into older chunks,
    // but at some point doing the shadow copy is just easier/faster.
    // It is always safe for this function to do nothing, return "false" and
    // let collect() do the heavy lifting.
    m_scanCount++;
    if (const auto robj = root.asPtr()) {
      switch (Arena::getMemoryKind(robj)) {
        case Arena::Kind::obstack: {
          auto& type = robj->type();
          // If the object is older than m_collectNote then we don't need to
          // copy but might need to scan.
          if (isOldSmallObject(robj)) {
            const auto done = !type.hasRefs() || robj->isFrozen();
            if (done)
              m_markCount++;
            return done;
          }
          if (type.hasRefs()) {
            // This object might have references.  Use the slower method.
            return false;
          }
          const ObjectSize objSize(
              type.uninternedMetadataByteSize(), robj->userByteSize());
          const auto objCopy = allocDuringGC(objSize);
          // Normally we would want to copy into objCopy.write but since
          // we're just doing a quick collect we copy straight into
          // objCopy.read ignoring the shadow.
          if (objCopy.read != robj) {
            objSize.memmove(const_cast<RObj*>(objCopy.read), robj);
            root.setPtr(const_cast<RObj*>(objCopy.read));
          }
          m_markCount++;
          return true;
        }

        case Arena::Kind::large: {
          auto& type = robj->type();

          // Large objects won't use any shadow but need a little extra
          // bookkeeping.

          if (isOldLargeObject(robj)) {
            // Like small objects, old large objects may need to be scanned.
            const auto done = !type.hasRefs() || robj->isFrozen();
            if (done)
              m_markCount++;
            return done;
          }
          if (type.hasRefs()) {
            // This object might have references.  Use the slower method.
            return false;
          }
          // This is a specialized version of updateLargeObject()
          auto header = LargeObjHeader::fromObject(robj);
          header->m_pos = markPos();
          m_markCount++;
          m_largeYoungCount++;
          return true;
        }

        case Arena::Kind::iobj: {
          updateIObj(static_cast<IObj*>(robj));
          return true;
        }

        default:
          // It's not a pointer type we care about (global?).  Hopefully it's
          // immutable.
          return true;
      }
    } else {
      // else: A FakePtr is always quick.  This covers returning a short string,
      // for example.
      return true;
    }
  }

  // Visit robj. If it's unmarked, mark it, and maybe queue it to be scanned.
  // If it's young and copyable, copy obj to new location, queue the copy to
  // be scanned, and update ref. If robj has already been copied, update ref
  // to point to the copy.
  void copyObject(RObj* const robj, RObjOrFakePtr& ref) {
    // See what kind of object this is
    auto kind = Arena::getMemoryKind(robj);

    if (kind == Arena::Kind::iobj) {
      // We need to handle iobj specially.
      return updateIObj(static_cast<IObj*>(robj));
    }

    if (kind == Arena::Kind::large) {
      // For large objects we don't need to copy them but we do need
      // to update their internal pointers.
      return updateLargeObject(robj);
    }

    if (kind != Arena::Kind::obstack) {
      // No idea what this is.  Don't touch it.
      return;
    }

    // This object lives in the Obstack heap.
    if (isOldSmallObject(robj)) {
      // This object is considered old because it is before markPos().
      // We can't move it, but we may need to scan it. Only count it as marked
      // if we are going to scan it, otherwise we would need to insert it
      // in m_marked just for the sake of m_markCount (not worth it).
      auto& type = robj->type();
      if (type.hasRefs() && !robj->isFrozen() && m_marked.insert(robj).second) {
        m_markCount++;
        m_workQueue.emplace_back(robj, type);
      }
      return;
    }

    auto& forwarded = robj->vtable();
    if (auto target = forwarded.asForwardedPtr()) {
      // This object has already been forwarded to its final location
      ref.setPtr(target);
      return;
    }

    m_markCount++;
    // We need to move this object
    auto& type = robj->type();
    const ObjectSize objSize(
        type.uninternedMetadataByteSize(), robj->userByteSize());

    const auto objCopy = allocDuringGC(objSize);
    const auto addrTarget = objCopy.read;
    const auto shadowTarget = objCopy.write;

    // copy the object - we'll fix up the pointers later - since we mark frozen
    // objects by changing the VTable pointer the object copy will be frozen.
    objSize.memcpy(shadowTarget, robj);

    // mark the object as forwarded
    robj->vtable().setForwardedPtr(addrTarget);

    if (type.hasRefs()) {
      m_workQueue.emplace_back(shadowTarget, type);
    }

    ref.setPtr(addrTarget);
  }

  void visitRoot(RObjOrFakePtr& root) {
    m_scanCount++;
    if (auto robj = root.asPtr()) {
      copyObject(robj, root);
      while (!m_workQueue.empty()) {
        const auto item = m_workQueue.back();
        m_workQueue.pop_back();
        item.m_type->forEachRef(*item.m_target, [this](RObjOrFakePtr& ref) {
          m_scanCount++;
          if (auto obj = ref.asPtr()) {
            copyObject(obj, ref);
          }
        });
      }
    }
  }

  void updateLargeObject(RObj* const obj) {
    if (isOldLargeObject(obj)) {
      if (m_marked.insert(obj).second) {
        // first time we've seen it during this collection
        m_workQueue.emplace_back(obj, obj->type());
        m_markCount++;
      }
    } else {
      auto header = LargeObjHeader::fromObject(obj);
      if (header->m_pos != markPos()) {
        // Move the object's position to markPos() instead of needing
        // a GC mark of some kind. This is safe because its placeholder
        // guaranteed that any subsequent notes are at newer positions.
        header->m_pos = markPos();
        m_workQueue.emplace_back(obj, obj->type());
        m_markCount++;
        m_largeYoungCount++;
      }
    }
  }

  // See allocDuringGC()
  struct CopyPtrs {
    RObj* read;
    RObj* write;
  };

  // Allocate memory on the Obstack to be added to the live set. Two pointers
  // are returned - 'read' is the address that will be used after GC is complete
  // and other objects should use that address as the new address of the
  // allocated object. 'write' is the address where the object should actually
  // be copied for now. For addresses in the note chunk (the one containing
  // m_collectNote) they will be different. For addresses outside the note
  // chunk they will be the same.
  CopyPtrs allocDuringGC(const ObjectSize& objSize) {
    const auto sz = roundUp(objSize.totalSize(), kAllocAlign);
    const auto read = static_cast<RObj*>(
        mem::add(m_obstack.allocSmall(sz), objSize.metadataSize()));
    m_copyVol += sz;
    if (Chunk::fromRaw(read) != m_collectChunk) {
      return CopyPtrs{read, read};
    }
    const auto write = static_cast<RObj*>(mem::add(
        m_shadow.data(), reinterpret_cast<uintptr_t>(read) & (kChunkSize - 1)));
    return CopyPtrs{read, write};
  }

  void updateIObj(IObj* iobj) {
    // Move the object's position to markPos() (collectNote-1),
    // instead of needing a GC mark of some kind.
    auto& detail = *m_obstack.m_detail;
    auto& delegate = iobj->refcountDelegate();
    auto inserted = detail.m_iobjRefs.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(&delegate),
        std::forward_as_tuple(markPos(), detail.m_currentIObj));
    if (inserted.second) {
      // We don't know about this IObj. This is actually a valid and reasonable
      // situation if the user code has a known IObj and it ends up saving a
      // pointer to an internal reference that the GC didn't previously know
      // about. Add it to the IObj mapping and increment its reference count.
      detail.m_currentIObj = &delegate;
      incref(&delegate);
      detail.m_stats.modifyIntern(1);
      m_markCount++;
      // if necessary, deleteUnusedIObjs() will create a placeholder.
    } else if (inserted.first->second.m_pos > markPos()) {
      // we already knew about it, and it's young, so mark it.
      inserted.first->second.m_pos = markPos();
      m_markCount++;
    }
  }

  void copyShadowToCollectChunk() {
    Pos endPos = Pos::fromRaw(m_obstack.m_nextAlloc);
    if (endPos > m_collectChunk->endPos()) {
      endPos = m_collectChunk->endPos();
    }
    m_shadowVol = endPos - m_collectNote;
    memcpy(
        m_collectAddr.ptr,
        mem::add(m_shadow.data(), m_collectNote.offsetInChunk()),
        m_shadowVol);
  }
};

void Obstack::collect() {
  m_detail->collect(*this, m_detail->m_firstNote);
}

void Obstack::collect(SkipObstackPos note) {
  m_detail->collect(*this, note);
}

void Obstack::collect(
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize) {
  m_detail->collect(*this, note, roots, rootSize);
}

// Specialized version of Collector::collect, without all the setup required
// for visiting roots. Just resets the allocation cursor and sweeps chunks,
// large objects, and iobjRefs.
void ObstackDetail::collect(
    Obstack& obstack,
    const SkipObstackPos note,
    const CollectMode mode) {
  if (anyHandles()) {
    // handles are extra roots, punt to the general collector
    collect(obstack, note, nullptr, 0, mode);
    return;
  }
  assert(Pos::fromAddress(note) >= Pos::fromAddress(m_firstNote));
  if (kVerifyNote)
    verifyNote(Pos::fromAddress(note));
  if (UNLIKELY(kMemstatsLevel)) {
    m_stats.countSweep(mode);
    m_stats.gcReclaim(obstack.usage(note));
  }
  const bool verbose = kGCVerbose >= 3;
  size_t preUsage = 0;
  if (UNLIKELY(verbose)) {
    preUsage = obstack.usage(note);
  }

  // Reset allocation cursor, then sweep.
  const auto oldNextAlloc = obstack.m_nextAlloc;
  obstack.m_nextAlloc = note.ptr;
  sweep(obstack, note, oldNextAlloc);

  if (UNLIKELY(verbose)) {
    const auto min = obstack.m_detail->m_minUsage;
    const auto total = obstack.m_detail->totalUsage(obstack);
    fprintf(
        stderr,
        "%s eligible %lu %s min %lu total %lu\n",
        kSweepModeNames[(int)mode],
        preUsage,
        (preUsage >= min ? ">=" : "<"),
        min,
        total);
  }
}

size_t ObstackDetail::collect(
    Obstack& obstack,
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize,
    CollectMode mode) {
  assert(Pos::fromAddress(note) >= Pos::fromAddress(m_firstNote));
  if (kVerifyNote)
    verifyNote(Pos::fromAddress(note));
  m_stats.countCollect(mode);

  // Clear back to mark, keeping all the roots (and everything they
  // transitively point to) alive.
  return Collector(obstack, note, mode).collect(roots, rootSize);
}

namespace {
using CollectMode = ObstackDetail::CollectMode;
using Pos = ObstackDetail::Pos;

void collectAuto(Obstack& obstack, SkipObstackPos note) {
  // no roots: don't look at usage or anything else expensive to calculate
  obstack.m_detail->collect(obstack, note, CollectMode::Auto);
}

void collectAuto(
    Obstack& obstack,
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize) {
  auto& detail = *obstack.m_detail;
  const auto eligible = obstack.usage(note);
  if (eligible >= detail.m_minUsage) {
    detail.m_minUsage =
        detail.collect(obstack, note, roots, rootSize, CollectMode::Auto);
  }
}

void collectManual(Obstack& obstack, SkipObstackPos note) {
  if (kGCManual) {
    obstack.m_detail->collect(obstack, note, CollectMode::Manual);
  } else {
    collectAuto(obstack, note);
  }
}

void collectManual(
    Obstack& obstack,
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize) {
  if (!kGCManual) {
    return collectAuto(obstack, note, roots, rootSize);
  }
  obstack.m_detail->collect(
      obstack, note, roots, rootSize, CollectMode::Manual);
}

template <typename Derived>
struct DelayedWorkQueue {
 protected:
  struct WorkItem {
    RObj* m_target;
    Type* m_type;

    WorkItem(RObj& target, Type& type) : m_target(&target), m_type(&type) {}
  };

  std::vector<WorkItem> m_queue;

  Derived& derived() {
    return *static_cast<Derived*>(this);
  }

  void processWorkQueue() {
    while (!m_queue.empty()) {
      auto next = m_queue.back();
      m_queue.pop_back();
      derived().processObject(*next.m_type, *next.m_target);
    }
  }

  void enqueueWork(Type& type, RObj& obj) {
    if (derived().shouldCopyType(type)) {
      m_queue.push_back({obj, type});
    }
  }
};

template <typename Derived>
struct ImmediateWorkQueue {
 protected:
  void processWorkQueue() {}

  Derived& derived() {
    return *static_cast<Derived*>(this);
  }

  void enqueueWork(Type& type, RObj& obj) {
    if (derived().shouldCopyType(type)) {
      derived().processObject(type, obj);
    }
  }
};

// A DeepCopyHelper which doesn't handle aliases in the graph at all.
template <typename Derived, typename WorkQueue, int GcStripe>
struct BasicDeepCopyHelper : WorkQueue {
  static RObjOrFakePtr copy(Obstack& obstack, RObjOrFakePtr root) {
    if (auto p = root.asPtr()) {
      Derived copier(obstack);
      root = copier.copyObject(p);
      copier.processWorkQueue();
    }
    return root;
  }

 protected:
  explicit BasicDeepCopyHelper(Obstack& obstack) : m_obstack(obstack) {}

  Derived& derived() {
    return *static_cast<Derived*>(this);
  }

  RObj* checkCopyObject(RObj* const obj) {
    if (!derived().shouldCopyObject(obj)) {
      return obj;
    }

    return nullptr;
  }

  RObj* shallowCloneObject(Type& type, RObj* const obj) {
    const ObjectSize objSize(
        type.uninternedMetadataByteSize(), obj->userByteSize());
    const auto objCopy = static_cast<RObj*>(
        mem::add(m_obstack.alloc(objSize.totalSize()), objSize.metadataSize()));

    // copy the object - we'll fix up the pointers later
    objSize.memcpy(objCopy, obj);

    return objCopy;
  }

  RObj* deepCopyObject(RObj* const obj) {
    auto& type = obj->type();
    auto objCopy = derived().shallowCloneObject(type, obj);
    derived().enqueueWork(type, *objCopy);

    return objCopy;
  }

  RObj* copyObject(RObj* const obj) {
    if (auto res = derived().checkCopyObject(obj)) {
      return res;
    }

    return deepCopyObject(obj);
  }

  void processObject(Type& type, RObj& target) {
    // run through the object and fix up the pointers
    type.forEachRef(
        target,
        [this](RObjOrFakePtr& ref) {
          if (auto obj = ref.asPtr()) {
            ref.setPtr(derived().copyObject(obj));
          }
        },
        GcStripe);
  }

 protected:
  Obstack& m_obstack;

 private:
  BasicDeepCopyHelper(const BasicDeepCopyHelper&) = delete;
  BasicDeepCopyHelper& operator=(const BasicDeepCopyHelper&) = delete;
};

struct Freezer : BasicDeepCopyHelper<
                     Freezer,
                     DelayedWorkQueue<Freezer>,
                     kSkipFreezeStripeIndex> {
 protected:
  using Base = BasicDeepCopyHelper<
      Freezer,
      DelayedWorkQueue<Freezer>,
      kSkipFreezeStripeIndex>;
  explicit Freezer(Obstack& obstack) : Base(obstack) {}

  RObj* checkCopyObject(RObj* const obj) {
    if (obj->vtable().isFrozen()) {
      // If it's already frozen then we're done.
      return obj;
    }

    // See if we already copied it.
    auto i = m_copyMap.find(obj);
    if (i != m_copyMap.end())
      return i->second;

    return nullptr;
  }

  bool shouldCopyType(Type& type) {
    // If there are no refs or all refs are guaranteed to be frozen we can just
    // copy the memory and be done.
    // Note - isAllFrozenRefs() implies !hasRefs().
    return !type.isAllFrozenRefs();
  }

  RObj* shallowCloneObject(Type& type, RObj* const obj) {
    auto copy = Base::shallowCloneObject(type, obj);
    copy->metadata().setFrozen();
    m_copyMap.insert(std::make_pair(obj, copy));
    return copy;
  }

 private:
  skip::fast_map<RObj*, RObj*> m_copyMap;

  friend struct DeepCopyHelper;
  friend struct BasicDeepCopyHelper;
  friend struct DelayedWorkQueue;
};

// This is the freezer used when the compiler has hinted that the
// object being frozen has no mutable aliases so we don't need to
// worry that a subobject could be referenced from two different
// places in the same acyclic graph.
//
struct NoAliasFreezer : BasicDeepCopyHelper<
                            NoAliasFreezer,
                            ImmediateWorkQueue<NoAliasFreezer>,
                            kSkipFreezeStripeIndex> {
 protected:
  explicit NoAliasFreezer(Obstack& obstack) : BasicDeepCopyHelper(obstack) {}

  bool shouldCopyObject(RObj* const obj) {
    // If it's already frozen then we're done.
    return !obj->vtable().isFrozen();
  }

  bool shouldCopyType(Type& type) {
    // If there are no refs or all refs are guaranteed to be frozen we can just
    // copy the memory and be done.
    // Note - isAllFrozenRefs() implies !hasRefs().
    return !type.isAllFrozenRefs();
  }

  RObj* shallowCloneObject(Type& type, RObj* const obj) {
    auto objCopy = BasicDeepCopyHelper::shallowCloneObject(type, obj);
    objCopy->metadata().setFrozen();
    return objCopy;
  }

  friend struct BasicDeepCopyHelper;
  friend struct ImmediateWorkQueue;
};
} // anonymous namespace

RObjOrFakePtr Obstack::freeze(RObjOrFakePtr obj) {
  return m_detail->freeze(obj, *this);
}

RObjOrFakePtr ObstackDetail::freeze(RObjOrFakePtr obj, Obstack& obstack) {
  if (auto p = obj.asPtr()) {
    auto& type = p->type();
    if (type.hasNoMutableAliases()) {
      obj = NoAliasFreezer::copy(obstack, obj);
    } else {
      obj = Freezer::copy(obstack, obj);
    }
    verifyInvariants(obstack);
  }
  return obj;
}

#if OBSTACK_VERIFY_NOTE
void ObstackDetail::verifyStore(void* field, Obstack& obstack) {
  using Pos = ObstackDetail::Pos;
  auto check = [&](Pos fieldPos) {
    // Check (field < nextAlloc), i.e. we aren't storing to unallocated memory.
    assert(fieldPos < Pos::fromRaw(obstack.m_nextAlloc));
    // Check (note <= field), i.e. field is young.
    // Disabled until we have a reliable way to obtain the current in-scope
    // note, even when the mutator may unwind the call stack past a note
    // without a collect(), or otherwise informing the runtime.
    // assert((m_verifyLastNote <= fieldPos));
  };
  switch (Arena::rawMemoryKind(field)) {
    case Arena::Kind::obstack:
      check(Pos::fromRaw(field));
      break;
    case Arena::Kind::large:
      for (auto large = m_currentLargeObj; large; large = large->m_prev) {
        if (mem::within(
                field,
                mem::add(large, sizeof(LargeObjHeader)),
                large->m_size)) {
          return check(large->m_pos);
        }
      }
      assert(false && "field in large space, but not in any large object");
      break;
    case Arena::Kind::iobj:
    case Arena::Kind::unknown:
      // This can happen for things like setting up initial constants.
      break;
  }
}
#endif

void Obstack::swapCur(Obstack& obstack) {
  std::swap(Obstack::cur(), obstack);
}

#ifndef NDEBUG
// only for testing
void Obstack::TEST_stealObjects(SkipObstackPos note, Obstack& source) {
  m_detail->stealObjectsAndHandles(*Process::cur(), *this, note, source);
}
#endif

// Copy the specified objects and their dependencies from the source obstack
// to this obstack, leaving source reset to its initial state.
void ObstackDetail::stealObjectsAndHandles(
    Process& destProcess,
    Obstack& dest,
    SkipObstackPos noteAddr,
    Obstack& source) {
  assert(this == dest.m_detail);
  auto& sourceDetail = *source.m_detail;
  const auto note = Pos::fromAddress(noteAddr);

  if (kVerifyNote) {
    // all chunks, large objects, and iobjs in source must be strictly newer
    // than note; when stolen we won't have to update their positions.
    for (auto chunk = Chunk::fromRaw(source.m_nextAlloc); chunk;
         chunk = chunk->prev()) {
      assert(chunk->beginPos() > note);
    }
    for (auto large = sourceDetail.m_currentLargeObj; large;
         large = large->m_prev) {
      assert(large->m_pos > note);
    }
    for (auto iobj = sourceDetail.m_currentIObj; iobj;
         iobj = sourceDetail.m_iobjRefs.at(iobj).m_prev) {
      assert(sourceDetail.m_iobjRefs.at(iobj).m_pos > note);
    }
  }

  // Absorb all stats from the source.
  mergeStats(source);

  if (!sourceDetail.empty(source)) {
    // steal chunks and slabs, update chunk generation numbers
    auto gen = [](void* p) { return Chunk::fromRaw(p)->generation(); };
    const auto numChunks = gen(source.m_nextAlloc) - gen(noteAddr.ptr);
    auto srcGen = gen(dest.m_nextAlloc) + numChunks;
    auto chunk = Chunk::fromRaw(source.m_nextAlloc);
    while (auto prev = chunk->prev()) {
      chunk->setGeneration(srcGen--);
      chunk = prev;
    }
    assert(srcGen == gen(dest.m_nextAlloc) + 1);
    chunk->setPrev(Chunk::fromRaw(dest.m_nextAlloc));
    dest.m_nextAlloc = source.m_nextAlloc;
    source.m_nextAlloc = nullptr;

    // chunks live in slabs; steal those too.
    m_chunkAllocator.stealSlabs(sourceDetail.m_chunkAllocator);

    // steal large objects. Their pos was contained within the range of
    // source's chunks, which were appended to dest's chunk list, ensuring
    // every stolen large object already has a valid pos in dest.
    if (auto large = sourceDetail.m_currentLargeObj) {
      allocPlaceholder(note, dest);
      assert(large->m_pos < Pos::fromRaw(dest.m_nextAlloc));
      while (auto prev = large->m_prev) {
        large = prev;
        assert(large->m_pos < Pos::fromRaw(dest.m_nextAlloc));
      }
      large->m_prev = m_currentLargeObj;
      m_currentLargeObj = sourceDetail.m_currentLargeObj;
      sourceDetail.m_currentLargeObj = nullptr;
    }

    // steal iobjRefs
    if (auto iobj = sourceDetail.m_currentIObj) {
      // slightly premature, if dest already knows about all src iobjs
      allocPlaceholder(note, dest);
      do {
        // maybe create iobjRef for iobj; if dest didn't already track iobj,
        // no incref is needed because we stole it from source. if dest did
        // already track iobj, decref-to-nonzero.
        stealIObj(iobj, note);
        iobj = sourceDetail.m_iobjRefs.find(iobj)->second.m_prev;
      } while (iobj);
      sourceDetail.m_currentIObj = nullptr;
      sourceDetail.m_iobjRefs.clear();
    }

    // source is now a zombie; we have moved all chunks, slabs, large objects,
    // and iobjRefs to dest.
  }

  // Take ownership of all RObjHandles. The underlying objects have already
  // been moved over. Even if source was empty, it could have some handles
  // that wrap null/fake pointers.
  sourceDetail.eachHandle([this, &destProcess](RObjHandle& h) {
    // Move the Handle to the target Process.
    h.unlink();
    prependHandle(h);

    // Update the owning Process; this is the only step that needs a lock.
    // Do this in a way that if we end up destroying the old Process, we
    // do it while the lock is no longer held.
    Process::Ptr destOwner{&destProcess};
    {
      std::lock_guard<std::mutex> lock{h.m_ownerMutex};
      destOwner.swap(h.m_owner);
    }
  });

  verifyInvariants(dest);
}

#ifndef NDEBUG

size_t Obstack::DEBUG_getSmallAllocTotal() {
  return Pos::fromRaw(m_nextAlloc) - Pos::fromAddress(m_detail->m_firstNote);
}

size_t Obstack::DEBUG_getLargeAllocTotal() {
  return m_detail->m_stats.getCurLargeSize();
}

size_t Obstack::DEBUG_getIobjCount() {
  return m_detail->m_stats.getCurInternCount();
}

size_t Obstack::DEBUG_allocatedChunks() {
  return m_detail->m_stats.getCurChunkCount();
}

void Obstack::verifyInvariants() const {
  return m_detail->verifyInvariants(*this);
}

#endif

// -----------------------------------------------------------------------------

namespace {

constexpr size_t kChunksPerSlab = kKmSlabSize / Obstack::kChunkSize;
} // anonymous namespace

// A RawChunk is a chunk which isn't initialized.  It gets turned into a Chunk
// by ChunkAllocator::newChunk() (and a Chunk turns into a RawChunk via
// ChunkAllocator::deleteChunk).
struct alignas(128) ObstackDetail::RawChunk {
  char data[Obstack::kChunkSize];
};

struct alignas(128) ObstackDetail::Slab {
  std::array<RawChunk, kChunksPerSlab> m_chunks;

  static Slab* fromChunk(RawChunk* chunk) {
    return reinterpret_cast<Slab*>((uintptr_t)chunk & ~(kKmSlabSize - 1));
  }

  void* operator new(size_t size) {
    assert(size == sizeof(Slab));
    static_assert(sizeof(Slab) == kKmSlabSize, "invalid slab size");
    void* slab = mem::alloc(kKmSlabSize, kKmSlabSize);
    Arena::setMemoryKind(
        slab, mem::add(slab, kKmSlabSize), Arena::Kind::obstack);
    assert(mem::isAligned(slab, kKmSlabSize));
    return slab;
  }

  void operator delete(void* p) {
    assert(mem::isAligned(p, kKmSlabSize));
    Arena::KindMapper::singleton().erase(p, mem::add(p, kKmSlabSize));
    mem::free(p, kKmSlabSize);
  }
};

ObstackDetail::ChunkAllocator::~ChunkAllocator() {
  // By setting this to 0 we force a clean up of all pending data.
  m_garbageLimit = 0;
  collectGarbage();

  assert(m_freelist.empty());
  assert(m_allocatedSlabs == 0);
}

ObstackDetail::ChunkAllocator::ChunkAllocator() {
  // reserve enough space for 2 slabs worth of freespace
  m_freelist.reserve(2 * kChunksPerSlab);

  // There's no point in collecting until we have at least 2 slabs worth of
  // chunks.
  m_garbageLimit = kChunksPerSlab * 2;
}

void ObstackDetail::ChunkAllocator::deleteChunk(Chunk* chunk) {
  chunk->~Chunk();
  m_freelist.push_back(reinterpret_cast<RawChunk*>(chunk));
}

Chunk* ObstackDetail::ChunkAllocator::newChunk(Chunk* prev) {
  static_assert(sizeof(RawChunk) == sizeof(Chunk), "");
  return new (newRawChunk()) Chunk(prev);
}

Chunk* ObstackDetail::ChunkAllocator::newChunk(Pos pin) {
  static_assert(sizeof(RawChunk) == sizeof(Chunk), "");
  return new (newRawChunk()) Chunk(pin);
}

ObstackDetail::RawChunk* ObstackDetail::ChunkAllocator::newRawChunk() {
  if (m_freelist.empty()) {
    // Allocate a new slab and add all of its chunks to the freelist.
    Slab* const slab = allocSlab();
    m_freelist.resize(kChunksPerSlab);
    for (size_t i = 0; i < kChunksPerSlab; ++i) {
      m_freelist[kChunksPerSlab - 1 - i] = &slab->m_chunks[i];
    }
  }

  RawChunk* ptr = m_freelist.back();
  m_freelist.pop_back();

  return ptr;
}

void ObstackDetail::ChunkAllocator::collectGarbage() {
  if (m_freelist.size() < m_garbageLimit) {
    return;
  }

  // Count the number of chunks in each slab and if we have an entire free slab
  // then we free it up.
  skip::fast_map<Slab*, int> slabFreeChunkCount;

  for (ssize_t index = (ssize_t)m_freelist.size() - 1; index >= 0; --index) {
    Slab* slab = Slab::fromChunk(m_freelist[index]);
    auto insert = slabFreeChunkCount.insert({slab, 1});
    if (insert.second)
      continue;
    if (++insert.first->second == kChunksPerSlab) {
      // We have a fully freed slab
      freeSlab(slab);

      // only slots >= index can be erased here, so index is still valid
      // after this call.
      m_freelist.erase(
          std::remove_if(
              m_freelist.begin() + index,
              m_freelist.end(),
              [slab](RawChunk* p) { return slab == Slab::fromChunk(p); }),
          m_freelist.end());

      // See if we've cleaned up enough
      if (m_freelist.size() <= m_garbageLimit / 2)
        break;
    }
  }

  // TODO: Worry about fragmentation here.  It's unlikely but possible for a
  // thread to keep alive a pile of slabs with just a small amount allocated out
  // of each one.  One possible mitigation is to periodically sort the freelist
  // by chunk address so we hand out all chunks from a given slab before moving
  // to a new slab.

  if (m_freelist.size() <= m_garbageLimit / 2) {
    // Whenever we clean up successfully we want to decrease the limit
    // (i.e. keep around less space and leave around less when we do clean up).
    m_garbageLimit = std::max(kChunksPerSlab * 2, m_garbageLimit * 2 / 3);
  } else {
    // Whenever we fail to clean up we want to increase the limit (so we don't
    // just try again on the next free).
    m_garbageLimit = (m_garbageLimit * 3) / 2;
  }
}

ObstackDetail::Slab* ObstackDetail::ChunkAllocator::allocSlab() {
  ++m_allocatedSlabs;
  // WARNING: It is critical to say "new Slab", not "new Slab()",
  // to avoid a gigantic memset here. We want its bytes uninitialized.
  return new Slab;
}

void ObstackDetail::ChunkAllocator::freeSlab(Slab* ptr) {
  --m_allocatedSlabs;
  delete ptr;
}

void ObstackDetail::ChunkAllocator::stealSlabs(ChunkAllocator& source) {
  m_allocatedSlabs += source.m_allocatedSlabs;
  m_freelist.insert(
      m_freelist.end(), source.m_freelist.begin(), source.m_freelist.end());
  source.m_allocatedSlabs = 0;
  source.m_freelist.clear();
}

void ObstackDetail::AllocStats::modifyChunk(ssize_t deltaCount) {
  m_curChunkCount = std::max<ssize_t>(0, (ssize_t)m_curChunkCount + deltaCount);
  m_maxChunkCount = std::max(m_maxChunkCount, m_curChunkCount);

  m_maxTotalSize =
      std::max(m_maxTotalSize, m_curChunkCount * kChunkSize + m_curLargeSize);
}

void ObstackDetail::AllocStats::modifyLarge(
    ssize_t deltaCount,
    ssize_t deltaBytes) {
  m_curLargeCount = std::max<ssize_t>(0, (ssize_t)m_curLargeCount + deltaCount);
  m_curLargeSize = std::max<ssize_t>(0, (ssize_t)m_curLargeSize + deltaBytes);

  m_maxLargeCount = std::max(m_maxLargeCount, m_curLargeCount);
  m_maxLargeSize = std::max(m_maxLargeSize, m_curLargeSize);

  m_maxTotalSize =
      std::max(m_maxTotalSize, m_curChunkCount * kChunkSize + m_curLargeSize);
}

void ObstackDetail::AllocStats::modifyIntern(ssize_t deltaCount) {
  m_curInternCount =
      std::max<ssize_t>(0, (ssize_t)m_curInternCount + deltaCount);
  m_maxInternCount = std::max(m_maxInternCount, m_curInternCount);
}

void ObstackDetail::AllocStats::resetLarge() {
  m_curLargeCount = 0;
  m_maxLargeCount = 0;
  m_curLargeSize = 0;
  m_maxLargeSize = 0;
}

void ObstackDetail::mergeStats(Obstack& source) {
  m_stats.merge(source.m_detail->m_stats);
}

void ObstackDetail::AllocStats::merge(AllocStats& other) {
  auto M = [](uint64_t& l, uint64_t& r) {
    l += r;
    r = 0;
  };
  // cur counters were handled by stealExtRefs().
  // max counters are ignored.
  M(m_smallVol, other.m_smallVol);
  M(m_largeVol, other.m_largeVol);
  M(m_fragmentVol, other.m_fragmentVol);
  M(m_gcVol, other.m_gcVol);
  M(m_shadowVol, other.m_shadowVol);
  M(m_gcReclaimVol, other.m_gcReclaimVol);
  M(m_gcVisitCount, other.m_gcVisitCount);
  M(m_gcScanVol, other.m_gcScanVol);
  M(m_placeholderVol, other.m_placeholderVol);
  M(m_runtimeCollects, other.m_runtimeCollects);
  M(m_manualCollects, other.m_manualCollects);
  M(m_autoCollects, other.m_autoCollects);
  M(m_runtimeSweeps, other.m_runtimeSweeps);
  M(m_manualSweeps, other.m_manualSweeps);
  M(m_autoSweeps, other.m_autoSweeps);
}

void ObstackDetail::AllocStats::countCollect(CollectMode mode) {
  switch (mode) {
    case CollectMode::Runtime:
      ++m_runtimeCollects;
      break;
    case CollectMode::Manual:
      ++m_manualCollects;
      break;
    case CollectMode::Auto:
      ++m_autoCollects;
      break;
  }
}

void ObstackDetail::AllocStats::countSweep(CollectMode mode) {
  switch (mode) {
    case CollectMode::Runtime:
      ++m_runtimeSweeps;
      break;
    case CollectMode::Manual:
      ++m_manualSweeps;
      break;
    case CollectMode::Auto:
      ++m_autoSweeps;
      break;
  }
}

void ObstackDetail::AllocStats::reportFinal() const {
  const auto totalVol = m_smallVol + m_largeVol;
  const auto maxChunkBytes = m_maxChunkCount * kChunkSize;
  const auto collects = m_runtimeCollects + m_manualCollects + m_autoCollects;
  const auto sweeps = m_runtimeSweeps + m_manualSweeps + m_autoSweeps;
  fprintf(stderr, "Obstack Peak Memory Usage Statistics\n");
  fprintf(stderr, "  total:      %lu\n", m_maxTotalSize);
  fprintf(stderr, "  chunks:     %lu (%lu)\n", m_maxChunkCount, maxChunkBytes);
  fprintf(stderr, "  largeObj:   %lu (%lu)\n", m_maxLargeCount, m_maxLargeSize);
  fprintf(stderr, "  iobj:       %lu\n", m_maxInternCount);
  fprintf(stderr, "Obstack Volume\n");
  fprintf(stderr, "  allocated:  %lu\n", totalVol);
  fprintf(stderr, "  |-large:    %lu\n", m_largeVol);
  fprintf(stderr, "  |-small:    %lu\n", m_smallVol);
  fprintf(stderr, "    |-places: %lu\n", m_placeholderVol);
  fprintf(stderr, "    |-frags:  %lu\n", m_fragmentVol);
  fprintf(stderr, "Collector Volume\n");
  fprintf(stderr, "  sweeps:    %lu\n", sweeps);
  fprintf(stderr, "  |-runtime: %lu\n", m_runtimeSweeps);
  fprintf(stderr, "  |-manual:  %lu\n", m_manualSweeps);
  fprintf(stderr, "  |-auto:    %lu\n", m_autoSweeps);
  fprintf(stderr, "  collects:  %lu\n", collects);
  fprintf(stderr, "  |-runtime: %lu\n", m_runtimeCollects);
  fprintf(stderr, "  |-manual:  %lu\n", m_manualCollects);
  fprintf(stderr, "  |-auto:    %lu\n", m_autoCollects);
  fprintf(stderr, "  visited:   %lu\n", m_gcVisitCount);
  fprintf(stderr, "  scanned:   %lu\n", m_gcScanVol);
  fprintf(stderr, "  copied:    %lu\n", m_gcVol);
  fprintf(stderr, "  shadowed:  %lu\n", m_shadowVol);
  fprintf(stderr, "  reclaimed: %lu\n", m_gcReclaimVol);
}

void ObstackDetail::AllocStats::report(const Obstack& obstack) const {
  auto PB = [](size_t n) { return n; };
  const auto usage = obstack.usage(obstack.m_detail->m_firstNote);
  fprintf(
      stderr, "Obstack Memory Usage: %lu (%lu peak)\n", usage, m_maxTotalSize);
}

RObjHandle::RObjHandle(RObjOrFakePtr robj, Process::Ptr owner)
    : m_robj(robj), m_next(this), m_prev(this), m_owner(std::move(owner)) {}

RObjHandle::~RObjHandle() {
  unlink();
}

void RObjHandle::unlink() {
  m_next->m_prev = m_prev;
  m_prev->m_next = m_next;
  m_prev = this;
  m_next = this;
}

RObjOrFakePtr RObjHandle::get() const {
  return m_robj;
}

bool RObjHandle::isOwnedByCurrentProcess() {
  // With a bit of hackery we could eliminate the lock here.
  std::lock_guard<std::mutex> lock{m_ownerMutex};
  return m_owner == Process::cur();
}

void RObjHandle::scheduleTask(std::unique_ptr<Task> task) {
  while (true) {
    // Fetch the current owner.
    Process::Ptr owner;
    {
      std::lock_guard<std::mutex> lock(m_ownerMutex);
      owner = m_owner;
    }

    // Attempt to post our task to it. There is a very unlikely race condition
    // here where the Process might be joinChild()ed after we fetched it above,
    // in which case we need to retry to ensure the task is not lost.
    //
    // We can't actually schedule the task while m_ownerMutex is locked,
    // because posting the task may run it and free the task, freeing
    // 'this' out from under us, so unlocking will be modifying freed memory.
    UnownedProcess unowned{std::move(owner)};
    if (LIKELY(unowned.scheduleTaskIfNotDead(task))) {
      break;
    }
  }
}

std::unique_ptr<RObjHandle> Obstack::makeHandle(RObjOrFakePtr robj) {
  // not using make_unique allows the RObjHandle constructor to be private
  auto handle = new RObjHandle(robj, Process::Ptr(Process::cur()));
  m_detail->prependHandle(*handle);
  return std::unique_ptr<RObjHandle>{handle};
}

bool Obstack::anyHandles() const {
  return m_detail->anyHandles();
}

bool Obstack::anyValidHandles() const {
  return m_detail->anyValidHandles();
}

template <class Fn>
void ObstackDetail::eachHandle(Fn fn) {
  for (auto sentinel = &m_handles, h = sentinel->m_next; h != sentinel;) {
    auto next = h->m_next; // in case h gets erased by fn.
    fn(*h);
    h = next;
  }
}

template <class T, class Fn>
T ObstackDetail::anyHandle(T falsy, Fn fn) const {
  for (const RObjHandle *sentinel = &m_handles, *h = sentinel->m_next;
       h != sentinel;) {
    auto next = h->m_next; // in case h gets erased by fn.
    if (auto v = fn(*h))
      return v;
    h = next;
  }
  return falsy;
}

bool ObstackDetail::anyHandles() const {
  return m_handles.m_next != &m_handles;
}

bool ObstackDetail::anyValidHandles() const {
  return anyHandle(false, [](const RObjHandle& h) { return h.get().isPtr(); });
}

bool ObstackDetail::empty(Obstack& obstack) const {
  assert(this == obstack.m_detail);
  assert(
      Pos::fromRaw(obstack.m_nextAlloc) > Pos::fromAddress(m_firstNote) ||
      (obstack.m_nextAlloc == m_firstNote.ptr && !anyValidHandles() &&
       m_iobjRefs.empty() && !m_currentIObj && !m_currentLargeObj));
  return obstack.m_nextAlloc == m_firstNote.ptr;
}

void ObstackDetail::prependHandle(RObjHandle& handle) {
  auto& sentinel = m_handles;
  handle.m_next = sentinel.m_next;
  handle.m_prev = &sentinel;
  sentinel.m_next->m_prev = &handle;
  sentinel.m_next = &handle;
}
} // namespace skip

using namespace skip;

#if OBSTACK_VERIFY_NOTE
// not inline, so the verifyNote logic gets run
SkipObstackPos SKIP_Obstack_note_inl() {
  return Obstack::cur().note();
}
#endif

///// manual collect

void SKIP_Obstack_collect0(SkipObstackPos note) {
  collectManual(Obstack::cur(), note);
}

RObjOrFakePtr SKIP_Obstack_collect1(SkipObstackPos note, RObjOrFakePtr root) {
  collectManual(
      Obstack::cur(), note, reinterpret_cast<RObjOrFakePtr*>(&root), 1);
  return root;
}

void SKIP_Obstack_collect(
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize) {
  collectManual(
      Obstack::cur(), note, reinterpret_cast<RObjOrFakePtr*>(roots), rootSize);
}

///// auto collect

void SKIP_Obstack_auto_collect0(SkipObstackPos note) {
  collectAuto(Obstack::cur(), note);
}

RObjOrFakePtr SKIP_Obstack_auto_collect1(
    SkipObstackPos note,
    RObjOrFakePtr root) {
  assert(tl_obstack.isNoteChunkFull(note));
  collectAuto(Obstack::cur(), note, reinterpret_cast<RObjOrFakePtr*>(&root), 1);
  return root;
}

void SKIP_Obstack_auto_collect(
    SkipObstackPos note,
    RObjOrFakePtr* roots,
    size_t rootSize) {
  collectAuto(
      Obstack::cur(), note, reinterpret_cast<RObjOrFakePtr*>(roots), rootSize);
}

///// Allocator api

void* SKIP_Obstack_alloc(size_t sz) {
  return Obstack::cur().alloc(sz);
}

void* SKIP_Obstack_calloc(size_t sz) {
  return Obstack::cur().calloc(sz);
}

void* SKIP_Obstack_alloc_pinned(size_t sz) {
  return Obstack::cur().allocPinned(sz);
}

void* SKIP_Obstack_calloc_pinned(size_t sz) {
  return memset(Obstack::cur().allocPinned(sz), 0, sz);
}

RObj* SKIP_Obstack_shallowClone(const RObj* obj) {
  auto p = reinterpret_cast<const RObj*>(obj);
  auto& type = p->type();
  const ObjectSize objSize(
      type.uninternedMetadataByteSize(), p->userByteSize());

  auto objCopy = static_cast<RObj*>(mem::add(
      Obstack::cur().alloc(objSize.totalSize()), objSize.metadataSize()));
  objSize.memcpy(objCopy, p);

  return reinterpret_cast<RObj*>(objCopy);
}

RObjOrFakePtr SKIP_Obstack_freeze(RObjOrFakePtr obj) {
  return Obstack::cur().freeze(obj);
}

// only called with skip_to_llvm --asan
void SKIP_Obstack_verifyStore(void* addr) {
#if OBSTACK_VERIFY_NOTE
  auto& obstack = Obstack::cur();
  obstack.m_detail->verifyStore(addr, obstack);
#else
  fatal("unimplemented");
#endif
}

SkipInt SKIP_Obstack_usage(SkipObstackPos note) {
  return Obstack::cur().usage(note);
}

void SKIP_Debug_printMemoryStatistics() {
  auto& obstack = Obstack::cur();
  obstack.m_detail->printMemoryStatistics(obstack);
}

void SKIP_Debug_printBoxedObjectSize(RObj* o) {
  Obstack::cur().m_detail->printObjectSize(reinterpret_cast<RObj*>(o));
}
