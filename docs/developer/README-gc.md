# General docs for Skip GC config options

## Heap Organization

The skip heap is divided into two primary spaces: interned objects, which
are ref-counted, and the Obstack, a linear-allocated, compacted, heap.
Because skip only has interned globals, the root set for the Obstack only
contains pointers on the execution stack.

The Obstack allocator is a linear bump-pointer allocator, and assigns
a logical position (``Pos``) to each address. Positions start from 0 and
increase, with 0 considered oldest and higher positions considered younger.

The Obstack garbage collector is a stop-the-world mark-compact collector,
invoked either by compiler-inserted calls into the runtime, an explicit
``System.localGC()`` call, or upon exit from a function annotated with ``@gc``.
At each collection point, the GC only collects the young portion of the heap,
defined by a note position, which is a copy of the allocation cursor captured
at function entry before any allocations occur. A collection performed
at the base of the execution stack (e.g. main()) is effectively a whole-heap
collection.

For more information, see the comments and terminology at the top of
Obstack.cpp in tests/runtime/native/src.

## GC Policy (when to collect)

The Obstack runtime uses several mechanisms to manage when to trigger an actual
collection, some of which have tuning parameters, described below. The
settings only make sense with some understanding of the trigger mechanisms.

0. Manual collections are unconditional, unless SKIP_GC_MANUAL=0, when they
turn into to automatic collections.

1. Any collection where the allocation cursor (cursor) is on the same chunk
as the note is ignored. ObstackBase::kChunkSizeLog2 determines the chunk size.

2. Any collection with no roots is an unconditional sweep of young objects,
so has no further triggering once condition 1 is met.

Collections with 1 or more roots must identify the live portion of the
young heap, and are throttled. At any point, usage(note) is the size of
the young obstack space. Immediately after a collection, let survivors(note) =
usage(note), i.e. the number of young bytes known to be live.

3. A collection with roots is allowed if usage(note1) > minUsage, where
minUsage is defined as SKIP_GC_RATIO * survivor(note0). Each collection
computes a survivor size, and thus the threshold for the next collection.

At each collection point, usage(note) increases two ways: allocating memory,
or unwinding to an older note. Both are potentialy good reasons to collect,
if the eligible (young) portion of the Obstack is sufficiently large.

## Constants

- ObstackBase::kChunkSizeLog2 determines chunk size. Chunk size affects
  allocator fragmentation (small objects cannot span chunks), allocator
  slow-path frequency (when chunks overflow), collection frequency, and
  compaction cost. Compacting the note chunk requires double copies,
  whereas compacting fully young chunks is implemented by copying into
  fresh chunks during the marking phase.


## Environment Variables

- SKIP_GC_RATIO={mult} (default 3) After a collection, sets the next
  minimum Obstack usage as a function of the number of bytes the GC had
  to scan and copy, for collections involving 1 or more roots.
  Note that usage is calculated back to the note used for collection,
  so both unwinding the stack and allocating more memory
  can increase the size of the young portion of the heap eligible for
  collection. Usage includes small and large objstack objects, but not
  interned objects, same as Debug.getMemoryFrameUsage().

- SKIP_GC_VERBOSE={0..3} (default 0). Enable verbose gc spew when it runs.
  Higher values generate more spew - see Obstack.cpp for details.

- SKIP_GC_SQUAWK={mult} (default SKIP_GC_RATIO^2). At SKIP_GC_VERBOSE=1,
  this controls how noisy the verbose output is. The collector will report
  "abnormal" collections, using ad-hoc heuristics which include SKIP_GC_SQUAWK
  as a tuning paramter. Lower == more verbose.

- SKIP_GC_MANUAL={0,1} (default 1). Enables the @gc annotation and localGC()
  intrinsics. If disabled, these degrade to the automatic behavior.

- SKIP_MEMSTATS={0,1} (default 0) Enable reporting obstack statistics.
  -- 0 disabled
  -- 1 basic stats are printed when an Obstack is destroyed. Normally this
  is at the end of the program, but also reports tabulate worker stats when
  a worker finishes

- OBSTACK_VERIFY_NOTE={0,1} (default 0 if -DOBSTACK_VERIFY_NOTE given at
  runtime build time, else 0). If enabled by the Debug build and set in
  the environment, adds additional checks that only valid note values are
  passed to the gc. Off by default in Debug builds because of cost.

- OBSTACK_VERIFY_PARANOID={0,1} (default 1 with -DOBSTACK_VERIFY_PARANOID at
  runtime build time, else 0). Enables expensive Obstack invariant checks
  after various complex operations.

## skip_to_llvm flags

- --autogc compile automatic gc hooks into the target program. By default,
  only manual @gc and localGC() intrinsics are compiled.
