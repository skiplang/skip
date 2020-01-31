/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "skip/Obstack.h"
#include "skip/AllocProfiler.h"

#include <boost/noncopyable.hpp>
#include <boost/operators.hpp>

#include <utility>
#include <vector>
#include <queue>

namespace skip {

struct LargeObjHeader;
struct Chunk;

/*
 * ObstackDetail implements most of the Obstack functionality; the C++ public
 * interface is struct Obstack.
 */
struct ObstackDetail final : private skip::noncopyable {
  ObstackDetail();
  ~ObstackDetail();

 public:
  /*
   * Pos is a totally ordered representation of positions in the Obstack.
   * High order bits store the chunk generation (lower==older, starting at 0);
   * low order bits are the byte offset into the chunk. This allows relational
   * comparisons on Pos values even with arbitrary chunk addresses
   */
  struct Pos final : boost::totally_ordered<Pos> {
    Pos() = default;
    Pos(const Pos& o) = default;
    Pos& operator=(const Pos& o) {
      m_position = o.m_position;
      return *this;
    }
    Pos(Pos&& o) noexcept = default;
    Pos& operator=(Pos&& o) noexcept = default;

    bool operator<(const Pos& o) const {
      return m_position < o.m_position;
    }
    bool operator==(const Pos& o) const {
      return m_position == o.m_position;
    }
    intptr_t operator-(const Pos& o) const {
      return m_position - o.m_position;
    }
    Pos operator-(size_t i) const {
      assert(i <= m_position);
      return Pos(m_position - i);
    }

    size_t generation() const;
    static Pos fromRaw(const void*);
    static Pos fromAddress(const RObj* robj);
    static Pos fromAddress(SkipObstackPos p);
    size_t offsetInChunk() const;

   private:
    explicit Pos(uintptr_t position) : m_position(position) {}

    void* toAddress(Chunk* chunk) const;
    const void* toAddress(const Chunk* chunk) const;

   private:
    uintptr_t m_position;
    friend Obstack;
    friend ObstackDetail;
  };

  void printMemoryStatistics(Obstack&) const;
  void printObjectSize(const RObj* o) const;

  // Returns memory usage since the given note.
  size_t usage(const Obstack&, SkipObstackPos note) const;
  size_t totalUsage(const Obstack&) const;

  enum class CollectMode {
    Runtime, // triggered by runtime internally
    Manual, // triggered by explicit @gc or localGC() in skip source code
    Auto // triggered by --autogc enabled hooks
  };

  // specialized collect when there are no roots
  void
  collect(Obstack&, SkipObstackPos note, CollectMode = CollectMode::Runtime);

  // Garbage collect the range [note, nextAlloc) with the given roots.
  // Reachable objects after note may be moved. Reachable objects before note
  // will be scanned but not moved. Pointers in the root set (and in reachable
  // objects) will be updated. Returns a new minimum usage threshold for
  // the next automatic collection.
  size_t collect(
      Obstack&,
      SkipObstackPos note,
      RObjOrFakePtr* roots,
      size_t rootSize,
      CollectMode mode = CollectMode::Runtime);

  // Allocate pinned objects
  void* allocPinned(size_t sz, Obstack&) _MALLOC_ATTR(1);

  // Return an interned copy of the given object which will be released when the
  // current position is cleared.
  IObjOrFakePtr intern(RObjOrFakePtr obj, Obstack&);

  // Register an interned object to be decref'd when the current
  // position is cleared. This increfs if it decides to hold onto it.
  // The passed in object is returned.
  IObjOrFakePtr registerIObj(IObjOrFakePtr obj, Obstack&);

  // Register an IObj at the given position, if we aren't already tracking it.
  // Caller is responsible for allocating a placeholder if necessary.
  void stealIObj(IObj* obj, Pos pos);

  void setHeapObjectMappingInvalid();

  // Return a frozen copy of the given object (and all its transitive
  // references).
  RObjOrFakePtr freeze(RObjOrFakePtr obj, Obstack&);

  void verifyStore(void* addr, Obstack&);

  // Steal objects from a worker Obstack (source) to deep copy into this
  // Obstack. resultNote should be the same passed to the Obstack(resultNote)
  // constructor.
  void stealObjectsAndHandles(
      Process& destProcess,
      Obstack&,
      SkipObstackPos resultNote,
      Obstack& source);
  void mergeStats(Obstack& source);

  // Create a placeholder for a large object or iobj, if necessary.
  void allocPlaceholder(Pos note, Obstack&);

  void verifyInvariants(const Obstack&) const;

  // This is the alignment used when allocating on the Obstack.
  static constexpr size_t kAllocAlign = Obstack::kAllocAlign;

 private:
  struct RawChunk;
  struct Slab;
  struct ChunkMap;
  struct Collector;
  struct LocalCopier;

  friend Collector;
  friend LocalCopier;
  friend Slab;
  friend ChunkMap;
  friend Obstack;
  friend LargeObjHeader;
  friend struct TestPrivateAccess;

  // allocate a large or pinned block
  void* allocLarge(size_t sz, Obstack&) _MALLOC_ATTR(1);

  // allocate a small block, when the current chunk is full.
  void* allocOverflow(size_t sz, Obstack&) _MALLOC_ATTR(1);

  // Assert that note is in m_validNotes, and prune younger validNotes.
  void verifyNote(Pos note);

  struct SweepStats {
    size_t largeYoungSurvivors{0};
  };
  SweepStats sweep(Obstack&, SkipObstackPos note, void* oldNextAlloc);
  LargeObjHeader*
  sweepLargeObjects(Obstack&, Pos markPos, Pos note, SweepStats&);
  void sweepChunks(Chunk* collectChunk, void* oldNextAlloc);
  void sweepIObjs(Obstack&, Pos markPos, Pos note);

  // call fn(RObjHandle&) for the pointer within each handle
  template <class Fn>
  void eachHandle(Fn);

  // call fn(RObjHandle&) repeatedly until fn returns a truthy value
  template <class T, class Fn>
  T anyHandle(T falsy, Fn) const;

  // true if the handles list is non-empty
  bool anyHandles() const;

  // true if any handles are valid pointers
  bool anyValidHandles() const;

  // Prepend the given Handle, which must not be in a list, to m_handles.
  void prependHandle(RObjHandle& handle);

  // is this obstack completely empty? it could still have some
  // handles, if none of the handles wrap a valid pointer.
  bool empty(Obstack&) const;

  struct IObjRef : private skip::noncopyable {
    IObjRef(Pos pos, IObj* prev) : m_pos(std::move(pos)), m_prev(prev) {}
    IObjRef(IObjRef&& o) noexcept : m_pos(o.m_pos), m_prev(o.m_prev) {}
    IObjRef& operator=(IObjRef&& o) noexcept {
      m_pos = o.m_pos;
      m_prev = o.m_prev;
      return *this;
    }

    Pos m_pos;
    IObj* m_prev;
  };

  struct AllocStats {
    void modifyChunk(ssize_t deltaCount);
    void modifyLarge(ssize_t deltaCount, ssize_t deltaBytes);
    void modifyIntern(ssize_t deltaCount);

    // Track allocation volume (monotonic counters, ignoring frees)
    void allocSmall(size_t sz) {
      m_smallVol += sz;
    }
    void allocLarge(size_t sz) {
      m_largeVol += sz;
    }
    void allocFragment(size_t sz) {
      m_fragmentVol += sz;
    }
    void allocGc(size_t sz) {
      m_gcVol += sz;
      m_smallVol -= sz;
    }
    void allocShadow(size_t sz) {
      m_shadowVol += sz;
    }
    void allocPlaceholder(size_t sz) {
      m_placeholderVol += sz;
    }

    // GC activity (monotonic counters)
    void gcReclaim(size_t sz) {
      m_gcReclaimVol += sz;
    }
    void gcScan(size_t sz) {
      m_gcScanVol += sz;
    }
    void gcVisit(size_t n) {
      m_gcVisitCount += n;
    }

    void report(const Obstack&) const;
    void reportFinal() const;

    size_t getCurChunkCount() const {
      return m_curChunkCount;
    }
    size_t getCurLargeSize() const {
      return m_curLargeSize;
    }
    size_t getCurLargeCount() const {
      return m_curLargeCount;
    }
    size_t getCurInternCount() const {
      return m_curInternCount;
    }

    void resetLarge();

    // Merge other's stats into this instance.
    void merge(AllocStats& other);

    void countCollect(ObstackDetail::CollectMode);
    void countSweep(ObstackDetail::CollectMode);

   private:
    size_t m_curChunkCount = 0;
    size_t m_maxChunkCount = 0;
    size_t m_curLargeCount = 0;
    size_t m_maxLargeCount = 0;
    size_t m_curLargeSize = 0;
    size_t m_maxLargeSize = 0;
    size_t m_curInternCount = 0;
    size_t m_maxInternCount = 0;
    size_t m_maxTotalSize = 0; // max(chunks + large objects)

    // Allocator stats (all monotonic accumulators)
    uint64_t m_smallVol = 0; // Obstack::alloc(sz) for small objects
    uint64_t m_largeVol = 0; // Obstack::alloc(sz) large objects,
                             // not counting sizeof(LargeObjHeader)
    uint64_t m_placeholderVol = 0; // subset of smallVol for placeholders
    uint64_t m_fragmentVol = 0; // subset of smallVol for chunk-end fragments
    uint64_t m_gcVol = 0; // allocDuringGC(sz) (copied small objects)

    // Collector stats, all monotonic
    uint64_t m_shadowVol = 0; // bytes copied from shadow chunk after gc
                              // (gcVol includes copies to shadow chunk)
    uint64_t m_gcReclaimVol = 0; // total bytes reclaimed
    uint64_t m_gcVisitCount = 0; // objects marked by gc
    uint64_t m_gcScanVol = 0; // bytes scanned by gc, including roots

    // What kinds of collections did we do
    uint64_t m_runtimeCollects = 0;
    uint64_t m_manualCollects = 0;
    uint64_t m_autoCollects = 0;
    uint64_t m_runtimeSweeps = 0;
    uint64_t m_manualSweeps = 0;
    uint64_t m_autoSweeps = 0;

    friend LargeObjHeader;
  };

  struct ChunkAllocator {
    ~ChunkAllocator();
    ChunkAllocator();

    Chunk* newChunk(Chunk* prevChunk);
    Chunk* newChunk(Pos pin);
    void deleteChunk(Chunk*);
    void collectGarbage();
    void stealSlabs(ChunkAllocator& source);

   private:
    Slab* allocSlab();
    void freeSlab(Slab* ptr);
    RawChunk* newRawChunk();

   private:
    size_t m_allocatedSlabs = 0;
    size_t m_garbageLimit; // Collect garbage when freelist.size() exceeds this
    std::vector<RawChunk*> m_freelist;
  };

 public:
  size_t m_minUsage = 0;

 private:
  LargeObjHeader* m_currentLargeObj = nullptr; // The newest LargeObjHeader

  IObj* m_currentIObj = nullptr;

  // This map is used to ensure that we don't inc/dec more than
  // once for each iobj.
  skip::fast_map<IObj*, IObjRef> m_iobjRefs;

  // This records the notes from the start of the obstack up to and including
  // the note most recently passed to collect().
  // Only used when OBSTACK_VERIFY_NOTE is enabled.
  std::vector<Pos> m_validNotes;

  AllocStats m_stats;
  AllocProfiler m_allocProfiler;

  ChunkAllocator m_chunkAllocator;

  // oldest note in the chunk chain of just this Obstack
  SkipObstackPos m_firstNote;

  // anchor node for circular list of handles owned by this obstack.
  RObjHandle m_handles;
};
} // namespace skip
