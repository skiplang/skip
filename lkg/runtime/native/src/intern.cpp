/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/intern.h"
#include "skip/intern-extc.h"

#include "skip/memoize.h"
#include "skip/objects.h"
#include "skip/stats.h"
#include "skip/map.h"
#include "skip/set.h"

#include <deque>
#include <iostream>

#if ENABLE_VALGRIND
#include <valgrind/memcheck.h>
#endif

namespace skip {

/*
 *
 * This file implements intern(), which clones and interns
 * an arbitrary graph of objects into a referenced-counted heap.
 *
 * Interning a graph of acyclic objects is trivial: recursively intern
 * referenced objects, replacing each reference with its canonical interned
 * pointer value. Then intern the raw bits for the object in the global
 * internTable. This always yields the right answer.
 *
 * We optimize the simplest case, interning a single acyclic object
 * (perhaps containing references to already-interned acyclic objects).
 * We look it up in the intern table without allocating any memory.
 * If it's not found then of course we must allocate a shallow clone and
 * insert that in the global internTable.
 *
 * It's interning cycles that is hard.
 *
 * Interning a cycle raises an obvious representation question: how do we
 * support reference-counted cycles without those reference counts causing a
 * memory leak?  This is the classic problem with reference-counted garbage
 * collection.  The trick, which only works because interned objects are
 * immutable, is that we identify cycles at interning time and mark each object
 * as a "cycle member" (see IObj::isCycleMember()) and delegate all of their
 * reference counting to the same object, a CycleHandle (see
 * IObj::refcountDelegate()). Intra-cycle references are not counted. When the
 * CycleHandle's reference count hits zero, that means no one is referencing any
 * member of the cycle, and the entire cycle is freed (i.e. the cycle lives and
 * dies as one).
 *
 * The graph of objects being interned may contain any combination of cyclic and
 * acyclic references, linked together in arbitrary ways. It's actually
 * surprisingly challenging to intern all cases correctly, but we do.
 *
 * The first step is to tease apart the graph into separately internable
 * subgraphs. Each subgraph is a "strongly connected component" (SCC), basically
 * a maximal group of objects where every object can "reach" every other object
 * by following references. In the common case of no cycles, each object forms
 * its own separate SCC.
 *
 * [Terminology note: whenever we say "cycle" we really mean "an SCC with cyclic
 * references", not the various sub-cycles that exist in a larger SCC.]
 *
 * [Terminology note: whenever we say objects are "equal", we really mean
 * "isomorphic". Object identity is not meaningful and all we care about is
 * local state in the object and what other objects are reachable from it.]
 *
 * To partition into SCCs we use Tarjan's Strongly Connected Components
 * algorithm
 * https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
 * This has the added benefit of topologically sorting the objects, so we
 * process children before the parents that point to them, which makes the
 * acyclic case easy.
 *
 * As a building block for the below algorithms, we have a deepCompare()
 * function which takes two objects and returns less than, equal, or
 * greater than, depending on the "local state" of the objects (class
 * type plus non-reference fields) and the local state of all objects
 * transitively reachable from them.  This imposes a total ordering on all graph
 * nodes. If this returns "equal" then the two objects are indistinguishable in
 * every way (there is no chain of pointers you can follow in the two objects to
 * find any difference in local state).
 *
 * To understand deepCompare()'s ordering, each object logically has a "sort
 * key" consisting of the (possibly infinite) "string" of local states reachable
 * from that object by doing a breadth-first walk which does not terminate when
 * it returns to a node already visited. The ordering on such strings, like any
 * strings, is transitive. It turns out that comparing two such nodes via BFS
 * walk does terminate, because we can do some short-circuiting without
 * affecting the semantically correct result -- there is never a need to place
 * any pair of pointers being compared on the BFS deque more than once, because
 * if there is any difference in state reachable from those two nodes, the copy
 * "earlier" in the BFS queue will find it first.
 *
 * There are two tricky interning cases:
 *
 * (A) The SCC points to an already-interned cycle, which is actually isomorphic
 *     to the SCC being interned, as in Examples 1 and 2 below. Surprisingly,
 *     interning a single-object SCC that does not point to itself can yield
 *     a cycle!
 *
 * (B) The SCC contains a cycle, either a single object pointing to itself or
 *     multiple objects.
 *
 * So the algorithm is as follows:
 *
 * (1) Check to see if (A) or (B) may be possible. A single-object SCC
 *     which only points to pre-interned acyclic objects (isDefinitelyAcyclic())
 *     cannot possibly hit either case, and so can be trivially interned by
 *     hashing its raw bytes, including the pointers to those interned objects.
 *
 * Otherwise:
 *
 * (2) Detect case (A): if any object in the SCC points to an already-interned
 *     cycle, maybe that object is equal to something in the SCC. To check, we
 *     find all predecessors of the already-interned object and deepCompare()
 *     them to the object being interned. If none are equal, we continue looking
 *     for more references to other cycles (we do not need to consider this
 *     cycle again.) But if we find a match, we have successfully interned this
 *     SCC, because if ANY object in the SCC is isomorphic to an interned
 *     object, all of the rest must be as well, and we can derive that mapping
 *     by recursively walking through the two isomorphic nodes' children in
 *     lockstep, mapping one to the other. This works even if there are
 *     duplicates in the SCC as in Example 4.
 *
 * (3) If step (2) didn't find anything, we need to look up the SCC in
 *     internTable. This requires some preprocessing to figure out exactly
 *     what we are going to look up:
 *
 *     (a) Create the localHashToNodes table by partitioning the objects in the
 *         SCC by their "local hashes", i.e. by hashing their raw byte contents
 *         *excluding* references which are not isDefinitelyAcyclic().
 *
 *     (b) Remove duplicate objects, as in Example 4, to put the SCC in
 *         canonical form. We find them by separately quicksorting the nodes in
 *         each partition of localHashToNodes, using deepCompare() as the
 *         comparator. Any object which is equal to the pivot at any step of the
 *         quicksort is discarded.
 *
 *     (c) Select a "cycle root". We *could* intern the graph many times,
 *         "starting at" every object in the graph, but it's smaller and cleaner
 *         to only intern it once, from the perspective of a single object,
 *         which we call the "root". The root is chosen in a way that anyone
 *         interning any object in any isomorphic SCC will also choose the same
 *         root, and hence look it up the same way (matching hash/equality).
 *         The root selection algorithm doesn't really matter as long as it's
 *         consistent; we happen to use the "first" object according to
 *         quicksort in the localHashToNodes partition with the smallest hash,
 *         which will be identical for any isomorphic SCC.
 *
 *     (d) Create a CycleHandle "wrapping" the cycle root and look it up in
 *         internTable. Its hash is formed by doing a DFS from the root and
 *         hashing in each SCC member it encounters, only visiting each child
 *         once. We also hash in information about the order in which each child
 *         is visited, in case we hash a degenerate graph which only has
 *         references and no distinguishing local hash state. internTable
 *         equality is done by recursing from the root using deepCompare().
 *
 *     (e) If we find the SCC in internTable, reuse it. Otherwise set up
 *         various bits of bookkeeping properly and insert it into the table.
 *
 * See also "DFA minimization", which is a related but non-incremental
 * algorithm.
 *
 * Here are some examples of tricky cases:
 *
 *
 * Example 1:
 *
 *          uninterned      #    already interned
 *          ------------    #    ----------------
 *                          #
 *                          #         +---+
 *                          #         |   |
 *               (7)---------------->(7)<-+
 *                          #
 *                          #
 *                          #
 *                          #
 *
 * Here we have an already-interned object with a single field "7" and a
 * reference to itself, and we want to intern a new object which also has a
 * field "7" that points to the already-interned object. These two objects are
 * equal (isomorphic), since each object has an "7" field and points to an
 * infinite chain of objects with "7" fields.
 *
 *
 *
 * Example 2:
 *
 *          uninterned      #    already interned
 *          ------------    #    ----------------
 *                          #         +------+
 *                          #         |      |
 *                          #         v      |
 *            +->(7)---------------->(8)<-+  |
 *            |   |         #         |   |  |
 *            |   |         #         |   |  |
 *            |   v         #         v   |  |
 *            +--(8)        #        (7)--+--+
 *
 * A more complex case of the problem in Example 1. The uninterned SCC
 * is equal to an already-interned SCC that it points to.
 *
 *
 *
 * Example 3:
 *
 *          uninterned      #    already interned
 *          ------------    #    ----------------
 *                          #
 *            +->(7)        #        (8)<-+
 *            |   |         #         |   |
 *            |   |         #         |   |
 *            |   v         #         v   |
 *            +--(8)        #        (7)--+
 *
 * These two cycles are equal, but we may have previously interned
 * the cycle starting at a different node. No matter which node we look
 * up ((7) or (8)), we need to find the corresponding node in the pre-interned
 * cycle rather than making a new cycle.
 *
 *
 *
 * Example 4:
 *
 *          uninterned      #    already interned
 *          ------------    #    ----------------
 *                          #
 *           (7)---+        #      +---+
 *            ^    |        #      |   |
 *            |    v        #     (7)<-+
 *            + --(7)       #
 *
 *
 * Here the graph being interned has internal redundancy that needs to be
 * removed: the two (7) values are equal to each other and need to be combined
 * before looking up the cycle in the intern table.
 *
 */

/**
 * Transient interning state for an RObj*, used for Tarjan's strongly
 * connected components algorithm and later steps.
 */
struct TarjanNode final {
  explicit TarjanNode(size_t index) : m_index(index), m_lowlink(index) {}

  /// The tentative interned object for this node, but perhaps not the final
  /// interned pointer yet.
  ///
  /// ??? This name is a bit misleading since it may not be interned yet.
  IObj* m_interned = nullptr;

  /// The "index" in Tarjan's strongly connected components algorithm,
  /// indicating the order in which DFS first visited this node.
  size_t m_index;

  /// The "lowlink" in Tarjan's strongly connected components algorithm,
  /// indicating the "first" node (earliest in DFS) reachable from this one,
  /// including itself.
  size_t m_lowlink;

  /// The "next" pointer for use in Tarjan's stack, and later linked lists.
  TarjanNode* m_next = nullptr;

  /// The "prev" node considered (for iterative implementaion).
  TarjanNode* m_prev = nullptr;

  /// Keep track of if we are in a cycle.
  bool m_inCycle = false;

  /// "visited" flag for use by recordInternMapping().
  bool m_visited = false;

  /// Does this object directly point to a fully interned cycle?
  bool m_pointsToInternedCycle = false;

  /// Used to hash a cycle.
  size_t m_dfsOrder = 0;

  /// Hash of local contents.
  size_t m_localHash;

  /// Populate with a passed in RObj.  This creates a "full" TarjanNode,
  /// which is only done after we know it needs to be made.
  void populate(const RObj& robj) {
    IObj& iobj = shallowCloneIntoIntern(robj);
    iobj.metadata().setFrozen();
    iobj.setRefcount(kBeingInternedRefcountSentinel);
    m_interned = &iobj;
    iobj.tarjanNode() = this;
  }
};

const bool kEnableInternStats = parseEnv("SKIP_INTERN_STATS", 0) != 0;

static ObjectStats s_internStats;

static void updateInternStats(const RObj& robj) {
  if (UNLIKELY(kEnableInternStats)) {
    s_internStats.accrue(robj);
  }
}

void dumpInternStats(bool sortByCount) {
  auto& out = std::cerr;

  out << "{ intern: ";
  s_internStats.dump(out, sortByCount);
  out << "}\n";
}

namespace {

constexpr DeepCmpResult cmp(uint64_t a, uint64_t b) {
  // This is weird because it needs to ensure that it doesn't return MIN_INT64.
  // If we return MIN_INT64 then cmp(a, b) would result in the same answer as
  // cmp(b, a) which is incorrect.
  return (a < b) - (a > b);
}

/**
 * Return true if an object is definitely acyclic, i.e. it's an interned
 * object already proven to not be part of any cycle.
 *
 * A reference to any isDefinitelyAcyclic() object is guaranteed to have a fixed
 * pointer value, both for the SCC being interned and for any already-interned
 * SCC). This makes the pointer suitable for bitwise hashing and comparison.
 *
 * Objects for which this returns false may interact with interning in
 * more complex ways, requiring more careful checking in the "slow path".
 *
 * 1) This returns false for an object that's in the process of being
 *    interned, which may be part of a cycle. The intra-SCC pointer
 *    value won't be identical to that used in an equal SCC already interned,
 *    so it would be wrong to hash or compare the pointer's raw bit pattern.
 *
 * 2) This returns false for an object in a pre-interned cycle, because
 *    it might be equal to the SCC being interned, as in Examples 1 and 2.
 */
static bool isDefinitelyAcyclic(IObj& iobj) {
  // The object must be a normal object, not using either
  // kCycleMemberRefcountSentinel or kBeingInternedRefcountSentinel.
  const Refcount rc = iobj.currentRefcount();
  assert(rc < kDeadRefcountSentinel);
  return rc <= kMaxRefcount;
}

/// Slightly slower version that works for any RObj, not just IObj.
static bool isDefinitelyAcyclic(const RObj& robj) {
  return robj.isInterned() && isDefinitelyAcyclic(static_cast<IObj&>(robj));
}

/**
 * Interns an object which is guaranteed to only refer to objects we
 * have proven are not isomorphic to some cycle that could contain robj.
 *
 * @param robj The object to be interned. It may or may not be an IObj.
 */
static IObj& internObjectWithKnownRefs(const RObj& robj) {
  // Lock the bucket where this object belongs.
  const size_t hash = robj.hash();
  auto& internTable = getInternTable();
  Bucket& bucket = internTable.lockHash(hash);

  // See if we already have an equal object in that bucket.
  if (IObj* existing = internTable.findAssumingLocked(bucket, robj, hash)) {
    // Success! This object already exists in the intern table.
    incref(existing);
    internTable.unlockBucket(bucket);

    if (IObj* iobj = robj.asInterned()) {
      // We allocated storage for an IObj we don't need after all, so free it.
      assert(iobj->currentRefcount() == kBeingInternedRefcountSentinel);
      freeInternObject(*iobj);
    }

    return *existing;
  }

  IObj* iobj = robj.asInterned();
  if (iobj != nullptr) {
    // Give it a refcount of 1 (immediately changing it to "isFullyInterned()").
    assert(iobj->currentRefcount() == kBeingInternedRefcountSentinel);
    iobj->setRefcount(1);
  } else {
    // Create a fresh IObj by cloning robj.
    iobj = &shallowCloneIntoIntern(robj);
    iobj->metadata().setFrozen();
  }

  // Incref all referenced objects.
  iobj->eachValidRef([](IObj* iref) { incref(iref); });

  // If they provided an initializer then call it before we release the intern
  // lock so we can guarantee that nobody else is given a handle to this object
  // before its initializer is run.
  if (auto onStateChange = iobj->type().getStateChangeHandler()) {
    (*onStateChange)(iobj, Type::StateChangeType::initialize);
  }

  // Record it in the global intern table, making it visible to everyone.
  internTable.insertAndUnlock(bucket, *iobj, hash);

  return *iobj;
}

/**
 * The "fast path" that attempts to intern robj, but only if it's "easy".
 *
 * @param robj The object to be interned. Must not already be
 *             allocated in interned memory.
 *
 * @returns an interned object, with an incremented refcount, if
 *          robj contains only isDefinitelyAcyclic() references.
 *          Otherwise (the "slow path"), returns nullptr.
 */
static IObj* simpleIntern(const RObj& robj) {
  assert(!robj.isInterned());
#if ENABLE_VALGRIND
  VALGRIND_CHECK_MEM_IS_DEFINED(&robj, robj.userByteSize());
#endif

#if FORCE_IOBJ_DELEGATE
  return nullptr;
#else
  const auto maybeCyclic = robj.anyValidRef(
      false, [](const RObj* ref) { return !isDefinitelyAcyclic(*ref); });
  if (maybeCyclic) {
    // This case is too complicated to handle here.
    return nullptr;
  }

  return &internObjectWithKnownRefs(robj);
#endif
}

/// See forceFakeLocalHashCollisionsForTesting().
static bool forceLocalHashToZero = false;

/**
 * Hash all of the non-reference bytes in this object, as well as
 * the raw pointer values for any reference to any isDefinitelyAcyclic() object.
 * These are all the bit patterns we definitely know won't change, regardless
 * of how the current SCC gets interned.
 */
static size_t computeLocalHash(IObj& iobj) {
  // Seed the hash with the VTable*.
  const auto& vtable = iobj.vtable();
  size_t hash = vtable.unfrozenBits();

  if (vtable.isArray()) {
    hash = hashCombine(hash, iobj.arraySize());
  }

  const auto rawStorage = reinterpret_cast<const char*>(&iobj);
  const char* prevEnd = rawStorage;

  // Hash all of the non-reference fields by hashing all the bytes "between"
  // references. We also treat any definitelyAcyclic() reference as hashable.
  iobj.eachValidRef([&](IObj* const& iref) {
    if (!isDefinitelyAcyclic(*iref)) {
      // Skip over this reference while hashing, by hashing everything
      // from the end of the previous ref to this one's start.
      const auto start = reinterpret_cast<const char*>(&iref);
      if (start != prevEnd) {
        hash = hashMemory(prevEnd, start - prevEnd, hash);
      }

      // Resume hashing after this reference.
      prevEnd = start + sizeof(IObj*);
    }
  });

  // Hash any leftover bytes up to the end of storage.
  if (size_t remaining = rawStorage + iobj.userByteSize() - prevEnd) {
    hash = hashMemory(prevEnd, remaining, hash);
  }

  return UNLIKELY(forceLocalHashToZero) ? 0 : hash;
}

/// Maps each computeLocalHash() to a linked list of TarjanNodes with that hash.
using HashToNodeListMap = skip::fast_map<size_t, TarjanNode*>;

/**
 * Partition nodes into linked lists by their computeLocalHash() values.
 * Two nodes in different buckets by definition cannot be equal.
 */
static HashToNodeListMap partitionByLocalContentsHash(TarjanNode* list) {
  HashToNodeListMap localHashToNodes;

  for (TarjanNode *n = list, *next; n != nullptr; n = next) {
    next = n->m_next;
    n->m_localHash = computeLocalHash(*n->m_interned);
    auto v = localHashToNodes.emplace(n->m_localHash, n);

    if (v.second) {
      // This is a new partition (we haven't seen this local hash before).
      n->m_next = nullptr;
    } else {
      // Existing partition. Prepend to the linked list of nodes with this hash.
      n->m_next = v.first->second;
      v.first->second = n;
    }
  }

  return localHashToNodes;
}

/**
 * If iobj (which is assumed to have a reference to cycleMember) is isomorphic
 * to some predecessor of cycleMember that's in the same SCC as cycleMember,
 * return that predecessor. Otherwise return nullptr.
 */
static IObj* findEqualPredecessor(IObj& iobj, IObj& cycleMember) {
  // The refcountDelegate() uniquely identifies members of its SCC.
  IObj& cycleHandle = cycleMember.refcountDelegate();
  assert(&cycleMember != &cycleHandle);

  // Objects already processed or pushed on the stack.
  skip::fast_set<IObj*> seen{&cycleMember};

  for (std::vector<IObj*> stack{&cycleMember}; !stack.empty();) {
    // Pop the next member of the cycle to try.
    IObj* const n = stack.back();
    stack.pop_back();

    bool compared = false;

    if (n->anyValidRef(false, [&](IObj* ref) {
          // Only consider objects in the same SCC.
          if (&ref->refcountDelegate() == &cycleHandle) {
            // If n points to cycleMember it is by definition a predecessor.
            if (ref == &cycleMember && !compared) {
              if (deepEqual(&iobj, n)) {
                return true;
              }

              // Don't bother comparing iobj and n a second time.
              compared = true;
            }

            // Recurse looking for other predecessors.
            if (seen.insert(ref).second) {
              stack.push_back(ref);
            }
          }
          return false;
        })) {
      return n;
    }
  }

  return nullptr;
}

/**
 * Record that 'dup', an uninterned object, is equal to 'canonical', an
 * already-interned object.
 *
 * This isomorphism also implies that each uninterned object that 'dup' points
 * to must intern to the corresponding object pointed to by 'canonical',
 * and so on recursively, so it creates those mappings as well.
 */
static void recordInternMapping(IObj& dup, IObj& canonical) {
  assert(!dup.isFullyInterned());
  assert(canonical.isFullyInterned());

  std::vector<IObj*> dead;

  std::vector<std::pair<IObj*, IObj*>> stack(1, {&dup, &canonical});
  dup.tarjanNode()->m_visited = true;

  while (!stack.empty()) {
    // Note that r1 interns to r2.
    IObj* const r1 = stack.back().first;
    IObj* const r2 = stack.back().second;
    stack.pop_back();

    assert(!r1->isFullyInterned());

    // 'r2' must be in the same cycle as 'canonical' to get here.
    assert(r2->isFullyInterned());
    assert(r2->isCycleMember());
    assert(&r2->refcountDelegate() == &canonical.refcountDelegate());

    assert(r1->vtable() == r2->vtable());

    // Recurse through both sets of references in lockstep, mapping
    // one to the other.
    const auto mem1 = reinterpret_cast<const char*>(r1);
    const auto mem2 = reinterpret_cast<const char*>(r2);

    r1->eachValidRef([&](IObj* const& n1) {
      if (!n1->isFullyInterned()) {
        TarjanNode* x = n1->tarjanNode();
        if (!x->m_visited) {
          x->m_visited = true;

          const ptrdiff_t offset = reinterpret_cast<const char*>(&n1) - mem1;
          IObj* const n2 = *reinterpret_cast<IObj* const*>(mem2 + offset);
          stack.emplace_back(n1, n2);
        }
      }
    });

    // Find the TarjanNode for this object and use the interned object.
    TarjanNode& node = *r1->tarjanNode();
    node.m_interned = r2;
    incref(r2);

    // Remember to free r1 when we are done recursing.
    dead.push_back(r1);
  }

  // Free all of the objects we made redundant.
  for (auto d : dead) {
    freeInternObject(*d);
  }
}

static bool findEqualNeighbor(TarjanNode& sccList) {
  skip::fast_set<IObj*> cyclesSeen;

  for (TarjanNode* n = &sccList; n != nullptr; n = n->m_next) {
    if (n->m_pointsToInternedCycle) {
      IObj& iobj = *n->m_interned;
      if (iobj.anyValidRef(false, [&](IObj* ref) {
            if (ref->isCycleMember() &&
                cyclesSeen.insert(&ref->refcountDelegate()).second) {
              if (IObj* eq = findEqualPredecessor(iobj, *ref)) {
                recordInternMapping(iobj, *eq);
                return true; // stop scanning iobj refs
              }
            }
            return false; // next ref
          })) {
        return true; // found equal neighbor
      }
    }
  }

  return false;
}

// C++11 doesn't seem to provide a standard pair hasher.
using HashIObjPair = boost::hash<std::pair<IObj*, IObj*>>;

/**
 * Which objects we are already in the process of comparing (or have
 * already compared).
 *
 * Since equality is symmetric, we canonicalize each pair by putting the
 * pointer at the lower address first.
 */
using IObjPairSet = skip::fast_set<std::pair<IObj*, IObj*>, HashIObjPair>;

/**
 * Helper function for deepCompare():
 *
 * If r1 and r2 are obviously equal, return 0.
 *
 * If r1 and r2 are obviously not equal, return a nonzero value indicating
 * how they compare to each other.
 *
 * Otherwise, push the pair on the stack for later deep comparison and
 * return 0, indicating they seem possibly equal.
 *
 * TODO: Technically r1 and r2 should be IObjOrFakePtr.
 */
static DeepCmpResult quickCompareOrDefer(
    IObjOrFakePtr r1,
    IObjOrFakePtr r2,
    IObjPairSet& seen,
    std::deque<std::pair<IObj*, IObj*>>& pending,
    bool needOrdering) {
  if (r1 != r2) {
    IObj *i1, *i2;
    if (!(i1 = r1.asPtr()) || !(i2 = r2.asPtr()) ||
        (!needOrdering && r1->isFullyInterned() && r2->isFullyInterned())) {
      // These objects are obviously not equal, so give up now.
      return cmp(r1.bits(), r2.bits());
    }

    // Insert into seen and, if not already there, push on the
    // stack for later recursive comparison.
    if (seen.emplace(std::min(i1, i2), std::max(i1, i2)).second) {
      pending.emplace_back(i1, i2);
    }
  }

  return 0;
}

/**
 * Shared implementation for deepCompare() and deepEqual(): this acts like
 * deepCompare() except that if needOrdering is true it can return
 * any arbitrary nonzero value if the graphs are not equal, without figuring
 * out the exact ordering.
 */
static DeepCmpResult _deepCompare(IObj* root1, IObj* root2, bool needOrdering) {
  // Deque of comparisons yet to be done, for BFS.
  //
  // NOTE: This *must* be a deque, not a stack, or comparison results
  // in the needOrdering==true case will not be transitive.
  std::deque<std::pair<IObj*, IObj*>> pending;

  // Which pairs of objects have ever been pushed onto 'pending'?
  //
  // This set prevents us from pushing the same comparison on the deque
  // more than once.
  //
  // Comparison works by optimistically assuming two objects are equal
  // and recursively trying to disprove that claim. If we end up back
  // at a comparison we are already trying, we just short circuit because
  // we are already assuming they are equal.
  //
  // Because we recurse in BFS order, pushing the same thing on the deque
  // a second time including (x,y) vs (y,x) can never affect the result,
  // because any difference the second copy would "expand to" would
  // already have been seen by the "earlier" pair expanding to it.
  IObjPairSet seen;

  // Seed the deque with root1, root2 (unless the answer is obvious).
  if (auto c = quickCompareOrDefer(root1, root2, seen, pending, needOrdering)) {
    return c;
  }

  while (!pending.empty()) {
    // Pop the next pair to compare (for a breadth-first walk).
    IObj* const r1 = pending.front().first;
    IObj* const r2 = pending.front().second;
    pending.pop_front();

    // NOTE: We know r1 != r2 here, and that neither is fake or null.

    // Compare VTable*s.
    if (r1->vtable() != r2->vtable()) {
      return cmp(r1->vtable().unfrozenBits(), r2->vtable().unfrozenBits());
    }

    // Compare sizes (only relevant for arrays/strings since VTables equal).
    const size_t size1 = r1->userByteSize();
    const size_t size2 = r2->userByteSize();
    if (size1 != size2) {
      return (DeepCmpResult)(size1 - size2);
    }

    //
    // Compare the contents of each object (both refs and raw nonref data).
    //

    // Raw memory for each object.
    const auto mem1 = reinterpret_cast<const char*>(r1);
    const auto mem2 = reinterpret_cast<const char*>(r2);

    // The byte offset immediately after the contents already compared.
    size_t prevEnd = 0;

    if (auto c = r1->anyRef(
            DeepCmpResult(0), [&](IObjOrFakePtr& n1) -> DeepCmpResult {
              // Compare the bytes "between" references.
              const size_t offset = reinterpret_cast<const char*>(&n1) - mem1;
              if (auto cmp = memcmp(
                      mem1 + prevEnd, mem2 + prevEnd, offset - prevEnd)) {
                return cmp;
              }
              prevEnd = offset + sizeof(n1);

              // Compare referenced objects by pushing on BFS queue.
              auto n2 = *reinterpret_cast<IObjOrFakePtr*>(mem2 + offset);
              if (auto cmp = quickCompareOrDefer(
                      n1, n2, seen, pending, needOrdering)) {
                return cmp;
              }
              return 0; // equal!
            })) {
      return c;
    }

    // Compare any bytes following the last reference.
    if (auto c = memcmp(mem1 + prevEnd, mem2 + prevEnd, size1 - prevEnd)) {
      return c;
    }
  }

  // The objects are equal!
  return 0;
}

/**
 * Sorts a list of TarjanNode objects using deepCompare().
 *
 * If it identifies equal objects, it discards all but one and updates
 * the 'canonical' fields of the redundant nodes to point to the one we keep.
 *
 * @returns the head of the sorted list, chaining that list to followingList.
 */
static TarjanNode* qsortAndDeduplicate(
    TarjanNode* list,
    TarjanNode* followingList) {
  if (list == nullptr) {
    return followingList;
  }

  // Use the first list entry as the quicksort pivot. Given that it comes
  // from a recursive walk there is no reason to believe the list is in
  // any particular order (e.g. already sorted) so this choice is as
  // good as any.
  TarjanNode* const pivot = list;
  list = list->m_next;
  pivot->m_next = nullptr;

  // Partition our objects: partitions[0] < pivot, partitions[1] > pivot.
  std::array<TarjanNode*, 2> partitions = {{nullptr, nullptr}};
  for (TarjanNode *n = list, *next; n != nullptr; n = next) {
    next = n->m_next;

    if (auto c = deepCompare(n->m_interned, pivot->m_interned)) {
      // c != 0, so they are not equal. Prepend to the list appropriate
      // for which side of 'pivot' it's on.
      const bool which = (c > 0);
      n->m_next = partitions[which];
      partitions[which] = n;
    } else {
      // This object is exactly equal to the pivot, so it is redundant,
      // as in Example 4.
      n->m_interned->tarjanNode() = pivot;
    }
  }

  // Recursively sort the two partitions, then concatenate them back
  // together with "pivot" in the middle to produce the final sorted list.
  pivot->m_next = qsortAndDeduplicate(partitions[1], followingList);
  return qsortAndDeduplicate(partitions[0], pivot);
}

/**
 * Hash the SCC starting at 'root', and update each references in each node's
 * 'interned' object to point to the 'interned' objects of the other nodes,
 * rather than to the original uninterned objects as was previously the case.
 */
static size_t canonicalizeRefsAndHash(TarjanNode& root) {
  size_t hash = root.m_localHash;

  // Track the order in which nodes are first visited by abusing the "mark"
  // field a little bit (< currentMark_ means "unvisited", else
  // n.mark - firstMark is the DFS visit order).
  size_t dfsOrder = 0;
  root.m_dfsOrder = ++dfsOrder;

  for (std::vector<TarjanNode*> stack{&root}; !stack.empty();) {
    TarjanNode& n = *stack.back();
    stack.pop_back();

    // "Snap" intra-SCC refs to point to the interned storage we allocated.
    n.m_interned->eachValidRef([&](IObj* const& ref) {
      assert(ref->isInterned());

      if (!ref->isFullyInterned()) {
        // Replace the reference with the canonical pointer.
        TarjanNode& child = *ref->tarjanNode();

        // const_cast is necessary because TarjanNode::m_interned is IObj*
        // rather than MutableIObj*
        const_cast<IObj*&>(ref) = child.m_interned;

        if (child.m_dfsOrder == 0) {
          // First time we have ever visited this child, so "recurse" on it.
          child.m_dfsOrder = ++dfsOrder;
          stack.push_back(&child);
        }

        hash = hashCombine(hash, child.m_localHash);

        // Mix the DFS order into the hash, just in case we have a pathological
        // graph where lots of objects have the same local hash. This gives
        // some clue as to the structure of the edges.
        hash = hashCombine(hash, dfsOrder);
      }
    });
  }

  return hash;
}

/**
 * This interns a nontrivial SCC which is known not to point to
 * an already-interned equal cycle (as in Examples 1 and 2) because
 * findEqualNeighbor() returned false.
 */
static void internComplexScc(TarjanNode& sccList) {
  // First partition by local hash.
  const auto localHashToNodes = partitionByLocalContentsHash(&sccList);

  // The "cycle root", used to intern a cycle from a canonical
  // starting point (to make Example 3 work). Anyone interning an
  // equal SCC starting at any node in this SCC will select
  // the same cycle root.
  TarjanNode* root = nullptr;

  // Linked list of all TarjanNodes, after removing the redundant ones.
  // This list is in no particular order.
  TarjanNode* head = nullptr;

  // qsort each partition, to remove duplicates and identify the cycle root.
  for (auto vp : localHashToNodes) {
    head = qsortAndDeduplicate(vp.second, head);

    // We elect the "first" node in the partition with the smallest
    // local hash to be the cycle root. Anyone interning an equal
    // SCC starting from any member of the SCC will produce the same root,
    // which is the invariant we need for interning to work.
    if (root == nullptr || vp.first < root->m_localHash) {
      root = head;
    }
  }

  assert(root != nullptr);

  const size_t hash = canonicalizeRefsAndHash(*root);

  CycleHandle& handle = CycleHandle::factory(hash, *root->m_interned);

  // Look up the CycleHandle in the intern table.
  auto& internTable = getInternTable();
  Bucket& bucket = internTable.lockHash(hash);

  if (IObj* existing = internTable.findAssumingLocked(bucket, handle, hash)) {
    // This cycle already exists, just reuse it.
    freeInternObject(handle);

    auto oldHandle = static_cast<CycleHandle*>(existing);
    recordInternMapping(*root->m_interned, *oldHandle->m_root);

    internTable.unlockBucket(bucket);

  } else {
    // We are keeping this cycle.

    // incref all referenced objects outside this cycle.
    for (TarjanNode* n = head; n != nullptr; n = n->m_next) {
      n->m_interned->eachValidRef([](IObj* ref) {
        if (ref->isFullyInterned()) {
          incref(ref);
        }
      });
    }

    // Mark the SCC members as being part of this cycle.
    Refcount sccSize = 0;
    for (TarjanNode* n = head; n != nullptr; n = n->m_next) {
      ++sccSize;

      // Set the magic refcount meaning "in a cycle".
      n->m_interned->setRefcount(kCycleMemberRefcountSentinel);

      // Set refcountDelegate().
      n->m_interned->next() = &handle;

      if (auto onStateChange = n->m_interned->type().getStateChangeHandler()) {
        (*onStateChange)(n->m_interned, Type::StateChangeType::initialize);
      }
    }

    // Each TarjanNode in the SCC contributes a refcount of 1.
    handle.setRefcount(sccSize);

    internTable.insertAndUnlock(bucket, handle, hash);
  }
}

/**
 * Interns robj, handling cyclic references.
 *
 * Algorithm adapted from strongconnect() in
 * https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
 */
IObj* partitionIntoSccsAndIntern(const RObj& root) {
  // Can't use skip::fast_map<> because we need the location of the values
  // to survive a rehash. TarjanNode::m_next and m_prev form useful lists.
  skip::node_map<const RObj*, TarjanNode> objToNode;

  // stack of pending refs to visit for the TarjanNodes in the scc stack.
  // Each recursion into a new node pushes refs in that node. They will be
  // popped in LIFO order, but order of refs within a node doesn't matter.
  std::vector<RObj**> refs;

  auto populateRefs = [&](TarjanNode* n) {
    refs.push_back(nullptr); // end-of-refs for n: signal to backtrack
    auto robj = const_cast<RObj*>(static_cast<const RObj*>(n->m_interned));
    robj->eachValidRef([&](RObj*& ref) { refs.push_back(&ref); });
  };

  auto rootTNodeIt = objToNode.emplace(&root, TarjanNode(0));
  TarjanNode* rootTNode = &(rootTNodeIt.first->second);
  rootTNode->populate(root);
  populateRefs(rootTNode);

  TarjanNode* currTNode = rootTNode;
  TarjanNode* stack = currTNode;

  while (true) {
    if (auto nextRef = refs.back()) {
      // Get next RObj.
      auto& nextRObj = **nextRef;

      auto nextIObj = nextRObj.asInterned();
      if (!nextIObj) {
        // Create TarjanNode.
        auto res = objToNode.emplace(&nextRObj, TarjanNode(objToNode.size()));
        bool freshlyInserted = res.second;
        TarjanNode* nextTNode = &res.first->second;

        if (freshlyInserted) {
          // We've never seen this node before.

          if (IObj* easyIObj = simpleIntern(nextRObj)) {
            // We can quickly intern this single acyclic node.
            nextTNode->m_interned = easyIObj;
          } else {
            // Create real TarjanNode.
            nextTNode->populate(nextRObj);
            populateRefs(nextTNode);
            nextTNode->m_prev = currTNode;

            // Push on to stack.
            nextTNode->m_next = stack;
            stack = nextTNode;

            // "Recurse" to this child and intern it.
            currTNode = nextTNode;
            continue; // goto while(true) above
          }
        } else {
          // We've seen this node before.

          // It has been fully interned if it isn't on the stack.
          // Invariant here is Tarjan's topological sort,
          // we are necessarily pointing to an already found
          // SCC, which are immediately interned.
          // We need to account for any merge and then check if it has
          // been fully interned to determine if it is on the stack.
          if (!nextTNode->m_interned->isFullyInterned()) {
            nextTNode = nextTNode->m_interned->tarjanNode();

            // It has not been interned because it is on the stack.
            if (!nextTNode->m_interned->isFullyInterned()) {
              currTNode->m_lowlink =
                  std::min(nextTNode->m_index, currTNode->m_lowlink);

              // We also know that we are in a cyclic SCC, so remember that.
              currTNode->m_inCycle = true;
            }
          }
        }
        nextIObj = nextTNode->m_interned;
      }

      // Replace the RObj reference with its interned equivalent.
      *nextRef = const_cast<MutableIObj*>(nextIObj);

      currTNode->m_pointsToInternedCycle |= nextIObj->isCycleMember();
    } else {
      // finished processing refs in currTNode
      refs.pop_back(); // pop the backtrack sentinel
      if (currTNode->m_lowlink == currTNode->m_index) {
        // We've found an SCC, immediately intern it.
        TarjanNode* sccList = stack;
        stack = currTNode->m_next;
        currTNode->m_next = nullptr;

        if (!findEqualNeighbor(*sccList)) {
          const bool oneNodeScc = (sccList->m_next == nullptr);
          if (!FORCE_IOBJ_DELEGATE && oneNodeScc && !sccList->m_inCycle) {
            // It's a simple acyclic node.
            currTNode->m_interned =
                &internObjectWithKnownRefs(*currTNode->m_interned);
          } else {
            internComplexScc(*sccList);
          }
        }

        assert(currTNode->m_interned->isFullyInterned());
      }

      assert(refs.empty() == !currTNode->m_prev);
      if (auto prev = currTNode->m_prev) {
        // Update our parent's reference to the interned result.
        // This could change later if they are both in the same SCC
        // and currTNode->m_interned is determined to be a duplicate.
        *refs.back() = const_cast<MutableIObj*>(currTNode->m_interned);

        prev->m_pointsToInternedCycle |= currTNode->m_interned->isCycleMember();

        // Backtrack.
        prev->m_lowlink = std::min(prev->m_lowlink, currTNode->m_lowlink);
        currTNode = prev;
      } else {
        // We're done.
        break;
      }
    }

    // Advance to the next reference.
    refs.pop_back();
  }

  if (!rootTNode->m_interned->isFullyInterned()) {
    rootTNode = rootTNode->m_interned->tarjanNode();
  }

  // We've got the output IObj.
  auto ret = rootTNode->m_interned;
  incref(ret);

  // Drop the extra references temporarily held by the TarjanNode objects.
  for (auto p : objToNode) {
    updateInternStats(*p.first);

    if (IObj* iobj = p.second.m_interned) {
      if (!iobj->isFullyInterned()) {
        // This was a duplicate that got discarded.
        freeInternObject(*iobj);
      } else {
        // The TarjanNode is no longer keeping this alive.
        decref(iobj);
      }
    }
  }

  assert(ret->isFullyInterned());

  return ret;
}
} // anonymous namespace

DeepCmpResult deepCompare(IObj* root1, IObj* root2) {
  return _deepCompare(root1, root2, true);
}

bool deepEqual(IObj* root1, IObj* root2) {
  return _deepCompare(root1, root2, false) == 0;
}

bool forceFakeLocalHashCollisionsForTests(bool forceCollisions) {
  std::swap(forceCollisions, forceLocalHashToZero);
  return forceCollisions;
}

void freeInternObject(IObj& obj) {
#ifndef NDEBUG
  AtomicRefcount& refcount = obj.refcount();
#endif

  // Make sure to call the finalizer BEFORE we change the refcount.
#ifndef NDEBUG
  auto refBefore = refcount.load(std::memory_order_relaxed);
#endif
  if (auto onStateChange = obj.type().getStateChangeHandler()) {
    (*onStateChange)(&obj, Type::StateChangeType::finalize);
    assert(refcount.load(std::memory_order_relaxed) == refBefore);
  }

  // Do a quick check for double-frees.
#ifndef NDEBUG
  assert(refBefore != kDeadRefcountSentinel);
  refcount.store(kDeadRefcountSentinel, std::memory_order_relaxed);
#endif

  Arena::free(
      const_cast<void*>(mem::sub(&obj, obj.type().internedMetadataByteSize())));
}

IObj* intern(const RObj* robj) noexcept {
  if (IObj* ret = robj->asInterned()) {
    // The object is already interned, so just incref and return it.
    incref(ret);
    return ret;
  } else if (IObj* easy = simpleIntern(*robj)) {
    // Optimization: avoid overhead of creating an Interner etc. below.
    updateInternStats(*robj);
    return easy;
  } else {
    // Handle the slow path of interning, where we could conceivably
    // have a cycle.
    return partitionIntoSccsAndIntern(*robj);
  }
}

StringPtr intern(String s) noexcept {
  if (auto longstring = s.asLongString()) {
    auto iobj = intern(longstring);
    return StringPtr(iobj->cast<const LongString>(), false);
  } else {
    return StringPtr(s, false);
  }
}

namespace {

void* rawShallowCloneObjectIntoIntern(
    size_t internedMetadataByteSize,
    size_t userByteSize,
    const RObj* userData,
    size_t numMetadataBytesToCopy) {
  assert(
      internedMetadataByteSize % sizeof(void*) == 0 &&
      userByteSize % sizeof(void*) == 0);

  // We can't have a userByteSize of 0 - if we did then when we asked the memory
  // subsystem if the first non-metadata address is interned we'd actually be
  // asking about memory which is off the allocated object.
  const size_t numBytes =
      internedMetadataByteSize + std::max<size_t>(userByteSize, 8);
  void* const raw = Arena::alloc(numBytes, Arena::Kind::iobj);

  // Copy in the VTable* and user data.
  memcpy(
      mem::add(raw, internedMetadataByteSize - numMetadataBytesToCopy),
      mem::sub(userData, numMetadataBytesToCopy),
      userByteSize + numMetadataBytesToCopy);

  return raw;
}
} // namespace

IObj& shallowCloneIntoIntern(const RObj& obj) {
  auto& type = obj.type();
  assert(type.internedMetadataByteSize() >= sizeof(IObjMetadata));
  const auto raw = mem::add(
      rawShallowCloneObjectIntoIntern(
          type.internedMetadataByteSize(),
          obj.userByteSize(),
          &obj,
          type.uninternedMetadataByteSize()),
      type.internedMetadataByteSize());
  IObj& iobj = *static_cast<IObj*>(raw);

  if (type.kind() == Type::Kind::invocation) {
    new (mem::sub(raw, Invocation::kMetadataSize)) Invocation(obj.vtable());
  } else {
    // Initialize the next pointer and refcount.
    iobj.next() = nullptr;
    new (&iobj.refcount()) AtomicRefcount(1);
  }

  return iobj;
}
} // namespace skip

skip::IObjOrFakePtr SKIP_intern(const skip::RObjOrFakePtr p) {
  if (auto ptr = p.asPtr()) {
    auto i = intern(ptr);
    return {i};
  } else {
    return skip::IObjOrFakePtr(p.bits());
  }
}
