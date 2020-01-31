/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/memoize.h"

#include "skip/Async.h"
#include "skip/Async-extc.h"
#include "skip/Exception.h"
#include "skip/external.h"
#include "skip/intern.h"
#include "skip/LockManager.h"
#include "skip/objects.h"
#include "skip/Obstack.h"
#include "skip/Process.h"
#include "skip/Reactive-extc.h"
#include "skip/Regex.h"
#include "skip/String.h"
#include "skip/System-extc.h"
#include "skip/System.h"
#include "skip/util.h"

#include <boost/io/ios_state.hpp>
#include <boost/version.hpp>
#include <shared_mutex>

#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <cstring>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <stdarg.h>

/*
 TODO:
- Ensure we cannot subscribe to inactive nonrefreshable Revisions.
- What thread pool to use?
- TxnId -> TxnID if we use HHVM naming.
- Right now I think we are not getting the theoretical deadlock case
  right, using multiple non-backdating placeholders (?) If we were willing
  to swallow that pill, we might as well not have placeholders at all.
*/

namespace skip {

// boost::intrusive_ptr::detach() is only from v1.56+
// boost::reset(T*, bool) is only from v1.56+
#if BOOST_VERSION >= 105600
template <typename T>
T* boost_detach(boost::intrusive_ptr<T>& p) {
  return p.detach();
}
template <typename T>
void boost_reset(boost::intrusive_ptr<T>& p, T* rhs, bool add_ref) {
  p.reset(rhs, add_ref);
}
#else
template <typename T>
T* boost_detach(boost::intrusive_ptr<T>& p) {
  T* raw = p.get();
  intrusive_ptr_add_ref(raw);
  p.reset();
  return raw;
}
template <typename T>
void boost_reset(boost::intrusive_ptr<T>& p, T* rhs, bool add_ref) {
  boost::intrusive_ptr<T>(rhs, add_ref).swap(p);
}
#endif

// These are protected by cleanupsMutex(), but safe to read without the lock if
// only a conservative value is needed (it can only increase).
static std::atomic<TxnId> s_oldestVisibleTxn{2};
static std::atomic<TxnId> s_newestVisibleTxn{2};

/// The oldest txn that any running task can possibly see (inclusive).
TxnId oldestVisibleTxn() {
  return s_oldestVisibleTxn.load(std::memory_order_relaxed);
}

/// The newest txn that any running task can possibly see (inclusive).
TxnId newestVisibleTxn() {
  return s_newestVisibleTxn.load(std::memory_order_relaxed);
}

static void registerCleanup(Invocation& inv, TxnId txn);
static void runCleanupsSoon();

/*

- You can only lock something where you can guarantee the refcount won't hit
  zero while the lock is held. Since decref doesn't actually do anything until
  all of this thread's locks are released, that tends not be a problem. But
  if some other thread could decref(), then a reference should be taken.

Locking order:

- Always lock Invocations before anything else.
- Only one Invocation at a time can be locked.
- Always lock Revision parents before Revision children
  (i.e. trace owner before the thing the trace points to).
- If locking multiple Revisions in the same Invocation, lock them
  in linked list order.
- Locking a Context (not its embedded m_placeholder) happens
  only after locking everything above.
- s_lruMutex and s_cleanupListsMutex locked last, and not both.

*/

// TODO: Try to enforce rules above, e.g. # of Invocations locked at once.

// TODO: Should holding a lock increment the refcount on the locked thing?
// Refcount hitting zero on a locked thing is not a good situation.
// Should we record that here?

// Decref this object once no locks are held.
static void safeDecref(IObj& obj) {
  auto& delegate = obj.refcountDelegate();
  if (!decrefToNonZero(delegate.refcount())) {
    lockManager().safeDecref(delegate);
  }
}

// Decref this object once no locks are held.
static void safeDecref(Revision& rev) {
  if (!decrefToNonZero(rev.m_refcount)) {
    lockManager().safeDecref(rev);
  }
}

// Invoke aa lambda on a list of Callers, but only in the Process
// that owns each Caller.
//
// Because we can't pass the same lambda to multiple callers, this
// takes as argument a function that generates a lambda.
//
// Note that we can't simply run this lambda ourselves, because that
// might deadlock -- running a lambda on one list entry might block waiting
// for another lambda in the list owned by a different Process.
template <typename GenFunc>
void invokeOnList(Caller* list, GenFunc&& genFunc) {
  Caller* runNow = nullptr;
  for (Caller *c = list, *next; c; c = next) {
    next = c->m_next;
    if (c->m_owningProcess) {
      c->m_owningProcess.schedule(genFunc(c));
    } else {
      c->m_next = runNow;
      runNow = c;
    }
  }

  // Notify any callers so fresh they have no Process (which must mean
  // they were created by this Process).
  for (Caller* next; runNow; runNow = next) {
    next = runNow->m_next;
    genFunc(runNow)();
  }
}

/**
 * An object that manages refreshing a Revision, i.e. trying to extend
 * its lifespan.
 *
 * TODO: It would be slightly more efficient to combine Refresher
 * and RefreshCaller into one class, but maybe a bit more confusing.
 *
 * TODO: Locking not clear.
 */
struct Refresher final : LeakChecker<Refresher> {
  Refresher(Caller& firstCaller, Revision& lockedRefreshee);

  // Advances the state machine refreshing m_refreshee.
  void continueRefresh_lck(EdgeIndex nextIndex, RevisionLock refresheeLock);

  // Called when some input fails to refresh.
  void inputRefreshFailed_lck();

  void runInvocation_lck(RevisionLock refresheeLock);

  // The Revision we hope to be refreshing.
  Revision::Ptr m_refreshee;

  CallerList m_callers;

  TxnId m_end = kNeverTxnId;

  const TxnId m_queryTxn;

  const TxnId m_latestVisibleWhenStarted;

  // Strong pointer to the invocation, if known, else nullptr.
  // We record this because it can get forgotten by m_refreshee while
  // we are running, and asyncEvaluate() relies on us being able to
  // provide an answer even if m_refreshee's trace and invocation disappear
  // while we are trying to refresh it.
  Invocation::Ptr m_invocation;

  EdgeIndex m_index = 0;
};

namespace {

using FreelistEdge = DownEdge;

struct RefreshCaller final : Caller, LeakChecker<RefreshCaller> {
  RefreshCaller(
      Refresher& refresher,
      Revision::Ptr refreshee,
      EdgeIndex traceIndex)
      : Caller(refresher.m_queryTxn),
        m_refresher(refresher),
        m_refreshee(std::move(refreshee)),
        m_traceIndex(traceIndex) {
    m_owningProcess = UnownedProcess(Process::cur());
  }

  void retry() override {
    auto lock = lockify(*m_refreshee);
    m_refreshee->asyncRefresh_lck(*this, std::move(lock));
  }

  void addDependency(Revision& lockedInput) override {
    // "Refresh" requires extending the lifespan of the existing
    // Revision. If we computed a different value, then of course the
    // refresh failed. Even if we computed the same value, but with a
    // noncontiguous lifespan, the bookkeeping is too hard to consider
    // that a "refresh".
    //
    // So a convenient way to check if we get the same value, and it's
    // consecutive, is just to see that the Revision we are refreshing
    // is the same one holding our desired vnalue.
    if (&lockedInput != m_refreshee.get()) {
      refreshFailed();
      DEBUG_TRACE("addDependency says refresh failed for " << m_refreshee);
    } else {
      DEBUG_TRACE(
          "addDependency says refresh succeeded for "
          << m_refreshee << " (newestVisible = " << newestVisibleTxn() << ")");
    }
  }

  void finish() override {
    Refresher& r = m_refresher;
    auto lock = lockify(r.m_refreshee.get());

    const EdgeIndex traceIndex = m_traceIndex;
    const bool refreshFailed = m_refreshFailed;

    DEBUG_TRACE(
        "Finishing refresh for " << m_refreshee << "; refreshFailed = "
                                 << (refreshFailed ? "true" : "false")
                                 << "; queryTxn = " << m_queryTxn);

    delete this;

    if (refreshFailed) {
      r.inputRefreshFailed_lck();
    }

    // Retry at the same index. This time we know more.
    r.continueRefresh_lck(traceIndex, std::move(lock));
  }

 private:
  // Only finish() should delete this object.
  ~RefreshCaller() override = default;

  Refresher& m_refresher;
  Revision::Ptr m_refreshee;
  const EdgeIndex m_traceIndex;
};
} // anonymous namespace

/**
 * Doubly-linked list of Invocations.
 */
struct InvocationList final : private skip::noncopyable {
  explicit InvocationList(bool isTopLevel = false)
      : m_sentinel(sentinelVTable()) {
    // The sentinel node points to itself. This way linking and unlinking
    // doubly-linked list entries never need to do null checks.
    m_sentinel.m_lruPrev = &m_sentinel;
    m_sentinel.m_lruNext = &m_sentinel;

    if (isTopLevel) {
#if SKIP_PARANOID
      // Don't count as a leak the sentinel Invocation we just created.
      LeakChecker<Invocation>::adjustCounter(-1);
#endif
    }
  }

  static Type& sentinelType() {
    static auto singleton =
        Type::invocationFactory(typeid(InvocationList).name(), 0, {});
    return *singleton;
  }

  static const VTableRef sentinelVTable() {
    static auto singleton = RuntimeVTable::factory(sentinelType());
    static VTableRef vtable{singleton->vtable()};
    return vtable;
  }

  bool empty_lck() {
    assertLocked(*this);
    return m_sentinel.m_lruPrev.ptr() == &m_sentinel;
  }

  Invocation::Ptr peekNewest() {
    auto lock = lockify(*this);
    if (empty_lck()) {
      return Invocation::Ptr();
    } else {
      return Invocation::Ptr(m_sentinel.m_lruNext.ptr());
    }
  }

  Invocation::Ptr peekOldest() {
    auto lock = lockify(*this);
    if (empty_lck()) {
      return Invocation::Ptr();
    } else {
      return Invocation::Ptr(m_sentinel.m_lruPrev.ptr());
    }
  }

  void erase(Invocation& inv, bool needDecref) {
    // This needs to be locked so we can change its m_owningList.
    assertLocked(inv);
    assert(inv.inList_lck());

    {
      auto lock = lockify(*this);
      erase_lck(inv);
    }

    if (needDecref) {
      inv.decref();
    }
  }

  void insert(Invocation& inv) {
    auto lock = lockify(*this);
    insert_lck(inv);
  }

  void insertIfUnlocked(Invocation& inv) {
    if (auto lock = tryLockify(*this)) {
      insert_lck(inv);
    }
  }

  std::mutex& mutex() const;

 private:
  void erase_lck(Invocation& inv) {
    assertLocked(*this);

    auto prev = inv.m_lruPrev.ptr();
    auto next = inv.m_lruNext.ptr();

    // TODO: If we ever *really* cared about speed here, it would better to
    // prefetch prev and next *before* taking the lock, at least if we knew
    // the memory could never become unmapped. Otherwise we are taking
    // cache/TLB misses while holding the lock.

    next->m_lruPrev = prev;
    prev->m_lruNext = next;

    inv.setOwningList_lck(OwningList::kNone);

    // These are not strictly needed; m_owningList is what matters.
    inv.m_lruPrev = nullptr;
    inv.m_lruNext = nullptr;
  }

  void insert_lck(Invocation& inv) {
    assertLocked(*this);

    if (inv.m_owningList == OwningList::kLru) {
      // Splice out of the list.
      auto prev = inv.m_lruPrev.ptr();
      auto next = inv.m_lruNext.ptr();
      next->m_lruPrev = prev;
      prev->m_lruNext = next;
    } else {
      assert(inv.m_owningList == OwningList::kNone);
      inv.incref();
    }

    // Insert at the head of the list, indicating it's the most recent.
    auto oldHead = m_sentinel.m_lruNext.ptr();
    inv.m_lruPrev = &m_sentinel;
    inv.m_lruNext = oldHead;
    oldHead->m_lruPrev = &inv;
    m_sentinel.m_lruNext = &inv;
  }

  // To avoid nullptr checks for prev/next, we always have this sentinel in
  // the list. Initially it points to itself as both prev and next.
  Invocation m_sentinel;
};

/// Protects the m_lruPrev and m_lruNext for all Invocations in an LRU list.
/// It does not protect these fields in Invocations in a cleanup list or
/// in no list.
alignas(kCacheLineSize) static std::mutex s_lruMutex;

static InvocationList s_lruList{true};

std::mutex& InvocationList::mutex() const {
  return s_lruMutex;
}

bool discardLeastRecentlyUsedInvocation() {
  // TODO: This is a bit slower than necessary due to lock ordering issues.
  // We end up taking and releasing the s_lruList mutex twice. This is
  // because we need to have an invocation locked to change its m_owningList,
  // but we don't know which one to lock until we already have s_lruList
  // locked, and the lock ordering rules do not allow locking an invocation
  // while s_lruMutex is held.

  while (auto oldest = s_lruList.peekOldest()) {
    auto lock = lockify(*oldest);

    // Handle an unlikely race where the object got removed from LRU
    // somehow before we locked it. Just grab another.
    if (LIKELY(oldest->m_owningList == OwningList::kLru)) {
      s_lruList.erase(*oldest, true);

      // Discard whatever state we can to reclaim memory.
      oldest->detachRevisions_lck();

      // Success! We removed something.
      return true;
    }
  }

  return false;
}

Invocation::Ptr mostRecentlyUsedInvocation() {
  return s_lruList.peekNewest();
}

/*
   Linked-list maintenance algorithm for Invocation Revisions:
   An Invocation corresponds to a (function, arguments) pair - an invocation.
   It caches values that this call is known to return for different TxnIds.

   An Invocation maintains a doubly linked list of Revisions
   (Invocation::m_headValue and Invocation::m_tailValue) indicating cached
   values for different transaction ranges. The list obeys several invariants:

   - Each entry specifies a [begin, end) TxnId lifespan that does not overlap
   any other entry's lifespan.

   - The list is sorted by "end", with larger values appearing earlier.

   - If two Revisions in the list "touch", i.e. their lifespans are
   consecutive, they must not have the same cached value.

   - Only the most recent non-placeholder Revision (see below) in the list
   maintains enough information to be "refreshed" (i.e. extend its lifespan).
   Although not semantically required, older entries discard that state simply
   to reclaim memory more quickly.

   Revisions are initially added to the Invocation::m_headValue list
   holding a "Context", rather than a memoized value, indicating computation
   currently in progress. Such Revisions are called "placeholders".
   To avoid redundant computation, any attempt to compute a value in a TxnId
   range occupied by a placeholder simply registers a callback with the
   placeholder to be invoked once its value is known.

   The placeholder's lifespan starts out with "begin" equal to the original
   TxnId being queried and its "end" equal to the "begin" of the previous
   (i.e. newer) linked list entry. If there is no newer entry, the "end"
   is kNeverTxnId (infinity).

   We do not "backdate" the "begin" TxnId to the "end" of the next linked
   list entry only to avoid an obscure deadlock with simultaneous evaluation
   of conditionally mutually recursive functions for different TxnIds.
   Consider this example:

   def a():        def b():
   if x():         if not x():
   b()             a()

   where x() is a function whose value changes over time due to some external
   dependency.

   Now imagine that one thread evaluates a() at TxnId 101, where x() is true,
   and a second thread evaluates b() at TxnId 100, where x() is false. Each
   thread will insert a placeholder for evaluating its function. Now suppose
   we "backdate" the begin values for those placeholders to some earlier TxnId
   (say, TxnId 1 because there is nothing else in the m_headValue list).
   As evaluation proceeds, a() will encounter the b() placeholder, register
   a callback with it waiting for it to finish, and return. b() will encounter
   the a() placeholder and add a callback, and now the system is deadlocked.

   Instead, by creating the placeholders starting at the original query TxnId,
   one of them (the one at the oldest TxnId) is guaranteed to always make
   progress. In this example, a() may indeed hit b()'s placeholder and get
   suspended. But b(), running at TxnId 100, will ignore the placeholder
   starting at 101 and successfully evaluate a().

   Evaluating an Invocation produces not only its return value, but also
   the lifespan (TxnId range) where the function is known to return that value.
   The lifespan is just the intersection of the lifespans of all the values
   the computed result depends on. It also produces a Trace describing the
   "inputs" to that function.

   When the placeholder's computation is complete, the placeholder is removed
   and the computed value is recorded in the Revision list. The
   placeholder's lifespan plays no role in the insertion; the exact lifespan
   is instead maintained in the Context while computing, and could be a
   superset or subset of the placeholder's lifespan, or only partially
   overlap it. The precise details of how it is recorded depend on the
   lifespan and what Revision entries already exist in the list.

   In addition to maintaining the invariants listed above, we also do not
   want to lose any information contained in the computed lifespan. Therefore
   the final m_headValue list will contain a single entry spanning at least
   the newly computed lifespan, and possibly even more.

   The easy case is where no existing Revision "touches" the newly-computed
   lifespan (i.e. is exactly adjacent to it with an equal value, or overlaps
   it). In this case we just insert a Revision with the computed lifespan
   and value and return.

   The "touch" cases are the harder ones. Three are three kinds of touching:

   - Placeholder. It is possible to have multiple placeholders in the list
   at the same time, and one of them will finish computing first. Its
   final lifespan may overlap those of other placeholders, either partially
   or completely. Any placeholder which is *completely* overlapped is simply
   removed from the list and discarded. If the overlap is *partial*, the
   overlapping range is removed from its lifespan, leaving it adjacent to
   the newly-computed Revision and still in the m_headValue list. If the
   computed value ends up in the middle of a placeholder's lifespan, the
   placeholder's lifespan is truncated to only include its earlier part
   (the one that encompasses the original query TxnId that created it),
   rather than splitting it into two placeholders or keeping the later part.

   - Unequal value. An unequal value with an adjacent lifespan is ignored.
   But if it has an overlapping lifespan that is a serious internal error -- it
   means that a function is somehow able to return different values for the
   same TxnId. If we want to be robust to such errors, rather than blowing up,
   we simply leave the overlapping Revision alone, and install zero or
   more new Revision objects "around it" as appropriate to fully cover
   the newly-computed lifespan.

   - Equal value. Whether adjacent or overlapping, an equal Revision must
   get "merged" with the newly computed value to maintain the list invariants.
   If the old Revision's lifespan subsumes the newly-computed range,
   then of course nothing needs to be done.

   If the lifespan touches one or more equal Revisions, we will extend
   the lifespan(s) of those objects rather than allocating a new list
   entry. We do this because any existing Revision object may already be
   referenced in the dependency graph, and making it more capable is strictly
   more useful.

   If more than one equal Revision "touches" the new one, we "keep" the
   newest one in the list, extending its lifepan as appropriate to "absorb"
   the lifespans of all touching, equal Revisions. If the newest touching
   Revision is also the "latest" one in the list, and if either its "end"
   is less than the new lifespan's or it has no Trace, then the existing
   Revision's Trace is replaced with the newly computed Trace. Otherwise,
   it is left alone.

   Any additional (i.e. older) touching values have their lifespan set to
   the same range but are then discarded from the list altogether to preserve
   the list invariants. This means they will probably be freed immediately,
   but it is possible for something else to hold a reference to one.

   The "latest" non-placeholder in the list is special, because it may remember
   information about how to refresh itself: a Trace, so it can see any of its
   inputs now produce different values, and an (Invocation, value) pair, so it
   can re-execute the code and see if it still produces the same value thot it
   used to. Additionally, if the latest value is "active" (has an end of
   kNeverTxnId), it may have subscribers watching to see if its lifespan is
   ever truncated.

   When a placeholder finishes computing and inserts a new value, it may become
   the new "latest" in the list. If so, it records the additional "refresh"
   information listed above and discards it from the previous latest. Note that
   simply inserting a placeholder at the head of a list does not render the
   old value "non-latest", because the eventual computed value may simply
   extend the lifespan of the current "latest" value, leaving it still
   "latest".

*/

const uint8_t Invocation::kMetadataSize =
    offsetof(Invocation, m_metadata) + sizeof(Invocation::m_metadata);

Invocation::Invocation(const VTableRef vtable)
    // WARNING: Be careful about setting fields here - we're usually raw copied
    // and then placement new'd with the expectation that the data won't be
    // munged.
    : m_metadata(1, vtable) {
  m_mutex.init();
  m_lruPrev = nullptr;
  m_lruNext = nullptr;
  m_headValue = nullptr;
  m_tailValue = nullptr;
  m_metadata.m_next.m_obj = nullptr;
}

Invocation::~Invocation() {
  // Everything should have been detached before we got here; otherwise
  // the strong pointers from the Revision list should have kept us alive.
  assert(m_headValue.ptr() == nullptr);
}

void Invocation::verifyInvariants_lck() const {
#if SKIP_PARANOID
  assertLocked(*this);

  // Verify Revision list invariants.
  bool seenNonPlaceholder = false;
  Revision* prevRev = nullptr;

  for (auto rev = m_headValue.ptr(); rev; rev = rev->m_next.ptr()) {
    assert(rev->currentRefcount() > 0);
    assert(rev->m_prev.ptr() == prevRev);
    assert(rev->isAttached_lck());

    if (prevRev != nullptr) {
      // Only the head of the Revision list is allowed to have a trace.
      assert(!rev->hasTrace_lck());
    }

    rev->verifyInvariants_lck();

    // Every entry in the list should have a defined value or be
    // a placeholder.
    auto& revValue ATTR_UNUSED = rev->value_lck();
    assert(revValue.type() != MemoValue::Type::kUndef);

    if (prevRev != nullptr) {
      if (rev->end_lck() == prevRev->begin_lck()) {
        // Two adjacent Revisions should not have the same value; they
        // should have been combined into one larger Revision.
        assert(revValue != prevRev->value_lck());
      } else {
        // The list must remain sorted with no overlaps.
        assert(rev->end_lck() < prevRev->begin_lck());
      }
    } else {
      // TODO should be true for all revs?
      // TODO should be non-null and == this?
      if (auto inv = rev->owner_lck()) {
        assert(inv == this);
      }
    }

    if (!rev->isPlaceholder()) {
      // Every Invocation with more than one Revision must be in
      // a cleanup list, to at least clean up the oldest one.
      assert(!seenNonPlaceholder || m_owningList == OwningList::kCleanup);
      seenNonPlaceholder = true;
    }

    prevRev = rev;
  }

  assert(m_tailValue.ptr() == prevRev);

  // TODO: Verify LRU list invariants, e.g.:
  //
  // - if not in LRU or cleanup, there should be no revision list.
#endif
}

void Invocation::verifyInvariants() const {
  auto lock = lockify(*this);
  verifyInvariants_lck();
}

Refcount Invocation::currentRefcount() const {
  return m_metadata.m_refcount.load(std::memory_order_relaxed);
}

void Invocation::detachRevisions_lck() {
  assertLocked(*this);

  while (auto head = m_headValue.ptr()) {
    detach_lck(*head);
  }
}

void Invocation::cleanup() {
  // TODO: If we ever see lock contention here, we can modify the
  // caller to take the lock, and do so in two passes: first a try_lock,
  // where objects that fail their try_lock are skipped, then a second
  // pass taking a normal lock where the try_lock values are processed.
  auto lock = lockify(*this);

  DEBUG_TRACE("Invocation::cleanup(" << this << ")");

  // Now that we hold the lock, mark this as no longer in the cleanup list.
  assert(m_owningList == OwningList::kCleanup);
  setOwningList_lck(OwningList::kNone);

  const TxnId oldestVisible = oldestVisibleTxn();

  // If tail is nullptr, meaning there are no cached values at all,
  // do nothing, not even move into LRU list.

  while (auto tail = m_tailValue.ptr()) {
    if (tail->canRefresh() && tail->m_prev == nullptr) {
      // We always keep the latest Revision, hoping to refresh it
      // later, even if its lifespan has expired.
      break;
    }

    const TxnId tailEnd = tail->end_lck();
    if (tailEnd > oldestVisible) {
      DEBUG_TRACE(
          "Re-registering " << this << " for cleanup due to tail end "
                            << tailEnd << " exceeding oldestVisible "
                            << oldestVisible);

      // tail is still visible.
      registerCleanup(*this, tailEnd);
      return;
    }

    detach_lck(*tail);
  }

  if (m_tailValue != nullptr) {
    DEBUG_TRACE("Moving " << this << " from cleanup to LRU");

    // NOTE: We do not bother to do this if there are no Revisions in the
    // list. TODO: The caller could do this much more efficiently, inserting
    // every single object whose cleanups ran into LRU all at once.
    moveToLruHead_lck();
  }
}

void Invocation::detach_lck(Revision& rev, bool permanent) {
  assertLocked(*this);

  if (rev.isAttached_lck()) {
    auto prev = rev.m_prev.ptr();
    auto next = rev.m_next.ptr();
    rev.m_prev = nullptr;
    rev.m_next = nullptr;

    if (prev != nullptr) {
      prev->m_next = next;
    } else {
      assert(m_headValue.ptr() == &rev);
      m_headValue = next;
    }

    if (next != nullptr) {
      next->m_prev = prev;
    } else {
      assert(m_tailValue.ptr() == &rev);
      m_tailValue = prev;
    }

    if (permanent) {
      rev.resetMemoizedValue_lck();
      rev.m_ownerAndFlags.detach();
    }

    rev.decref();
  }
}

void Invocation::insertBefore_lck(Revision& rev, Revision* before) {
  rev.m_next = before;

  Revision* prev;
  if (before != nullptr) {
    prev = before->m_prev.ptr();
    before->m_prev = &rev;

    // Only the head of the list is allowed to have a trace.
    before->clearTrace_lck();
  } else {
    // No successor, so insert at the very end of the list.
    prev = m_tailValue.ptr();
    m_tailValue = &rev;
  }

  rev.m_prev = prev;
  if (prev != nullptr) {
    prev->m_next = &rev;

    // Only the head of the list is allowed to have a trace.
    rev.clearTrace_lck();
  } else {
    m_headValue = &rev;
  }
}

IObj* Invocation::asIObj() const {
  auto raw = reinterpret_cast<const char*>(this);
  return reinterpret_cast<IObj*>(raw + kMetadataSize);
}

Invocation& Invocation::fromIObj(IObj& iobj) {
  // Since most of Invocation is in the metadata it's technically mutable.
  return *const_cast<Invocation*>(
      reinterpret_cast<const Invocation*>(mem::sub(&iobj, kMetadataSize)));
}

void Invocation::incref() {
  skip::incref(asIObj());
}

void Invocation::decref() {
  safeDecref(*asIObj());
}

skip::SpinLock& Invocation::mutex() const {
  return reinterpret_cast<skip::SpinLock&>(m_mutex);
}

bool Invocation::inList_lck() const {
  return m_owningList != OwningList::kNone;
}

Revision::Ptr Invocation::insertRevision_lck(
    Revision::Ptr insert,
    bool preferExisting) {
  assertLocked(*this);

  DEBUG_TRACE(
      "Inserting revision " << RevDetails(*insert) << " (preferExisting="
                            << (preferExisting ? "true" : "false") << ") into "
                            << InvDetails(*this));

  // "insert" should think it is attached, but not be in the list yet.
  assert(insert->isAttached_lck());
  assert(insert->m_prev == nullptr && insert->m_next == nullptr);
  assert(insert->owner_lck() == this);

  TxnId begin = insert->begin_lck();
  const TxnId end = insert->end_lck();

  // Did we replace "insert" with something already in the Revision list?
  bool inserted = false;

  // Scan until we find the first possible overlap.
  Revision* rev;
  for (rev = m_headValue.ptr(); rev != nullptr; rev = rev->m_next.ptr()) {
    TxnId rb = rev->begin_lck();
    const bool eq = (rev->value_lck() == insert->value_lck());

    if (rb < end || (rb == end && eq)) {
      break;
    }
  }

  for (Revision* next; rev != nullptr; rev = next) {
    next = rev->m_next.ptr();

    const TxnId re = rev->end_lck();
    const bool eq = rev->value_lck() == insert->value_lck();
    if (re < begin || (re == begin && !eq)) {
      // We overshot.
      break;
    }

    const TxnId rb = rev->begin_lck();

    if (eq) {
      begin = std::min(begin, rb);
      insert->setBegin_lck(begin);

      // We have two Revisions with the same value whose lifespans overlap
      // or touch. We can only keep one of them.
      if (!inserted && !rev->refresher_lck() &&
          (preferExisting || re > end ||
           // Break ties by keeping the one with the most
           (re == end &&
            rev->currentRefcount() >= insert->currentRefcount()))) {
        bool restart = false;

        // Keep rev, the existing Revision, and throw away the new one.
        if (end > re && rev->canRefresh()) {
          DEBUG_TRACE(
              "Keeping existing Revision " << RevDetails(*rev)
                                           << "; stealing trace from "
                                           << RevDetails(*insert));

          // 'insert' has a better lifespan, but we'd rather keeep 'rev',
          // so transfer the trace over to 'rev'.
          rev->stealTrace_lck(*insert);

          // Keep "begin" information we know to be true.
          rev->setBegin_lck(std::min(begin, rev->begin_lck()));

          // It's possible by the time we stole the trace, someone else
          // refreshed some input and now it goes farther into the future.
          // The easiest thing is just to start over.
          const TxnId newEnd = rev->end_lck();
          restart =
              (newEnd > end && rev->m_prev != nullptr &&
               newEnd >= rev->m_prev->begin_lck());

          DEBUG_TRACE("After steal revision is " << RevDetails(*rev));
        } else {
          insert->clearTrace_lck();
          rev->setBegin_lck(begin);
        }

        // NOTE: "insert" is not really in the linked list, even though
        // it claims to be attached (since were maybe going to insert it),
        // so calling detach_lck would be wrong (mess up
        // m_headValue/m_tailValue).
        insert->resetMemoizedValue_lck();
        insert->m_ownerAndFlags.detach();

        insert.reset(rev);

        if (restart) {
          detach_lck(*insert, false);
          return insertRevision_lck(std::move(insert), false);
        } else if (insert->m_prev != nullptr) {
          insert->clearTrace_lck();
        }

        inserted = true;
      } else {
        // Either we already picked a different Revision as the winner,
        // or this Revision looked worse than "insert". Either way, throw
        // away "rev".

        // Recover the trace memory, as we are unlikely to ever use it.
        rev->clearTrace_lck();

        detach_lck(*rev);
      }
    } else {
      assert(rev->isPlaceholder());

      if (rb < begin) {
        // Only keep beginning of placeholder. This case could be
        // "insert" overlapping rev's entire end, or because it appears in
        // the middle of "rev"'s lifespan. Either way, we'll just keep the first
        // piece of "rev", which is where its original queryTxn was.
        rev->setEnd_lck(begin, __LINE__);
        break;
      } else if (re > end) {
        // Only keep end of placeholder.
        rev->setBegin_lck(end);
      } else {
        // Placeholder completely subsumed, so drop it.
        detach_lck(*rev);
      }
    }
  }

  if (!inserted) {
    // Insert right before "rev".
    insert->incref();
    insertBefore_lck(*insert, rev);
  }

  DEBUG_TRACE("Done inserting, result is " << InvDetails(*this));

  return insert;
}

namespace {

// We don't assume a non-MVCC data source will produce the same value
// for the same TxnId, and we don't know for what TxnIds any computed
// value is actually valid, but we try to model it as consistently as we can,
// as a "best effort".
//
// Normally, when a non-MVCC data source produces a value we just assign
// it the same lifespan that its placeholder had. It has no other dependencies
// on which to base a decision.
//
// However, there are certain exotic situations where that placeholder
// may be missing.  In those cases we choose some lifespan that contains
// the original queryTxn but does not overlap any other Revisions.
Revision*
assignNonMvccLifespan(Invocation& inv, Revision& result, Context& ctx) {
  const auto queryTxn = ctx.m_queryTxn;

  // Look to see if we already have a Revision at that location.
  for (Revision* rev = inv.m_headValue.ptr(); rev; rev = rev->m_next.ptr()) {
    if (rev->begin_lck() <= queryTxn) {
      if (rev->end_lck() > queryTxn) {
        // There's already a Revision there...
        if (!rev->isPlaceholder()) {
          // It's not a placeholder - use its value.
          return rev;
        } else {
          // It's a placeholder - use the placeholder's lifespan
          result.setBegin_lck(rev->begin_lck());
          result.setEnd_lck(rev->end_lck(), __LINE__);
          return &result;
        }
      }
      // Everything else is older.
      break;
    }
  }

  // There's nothing there - so just use the minimum required.
  result.setBegin_lck(queryTxn);
  result.setEnd_lck(queryTxn + 1, __LINE__);
  return nullptr;
}
} // anonymous namespace

std::pair<Revision::Ptr, RevisionLock> Invocation::replacePlaceholder(
    Context& ctx,
    MemoValue&& value) {
  auto lock = lockify(*this);

  // Create the tentative result Revision (but we may not keep it).
  Revision::Ptr result{
      new Revision(0, kNeverTxnId, nullptr, nullptr, std::move(value), this),
      false};

  // Imbue it with a Trace, which computes its current lifespan.
  auto traceVec = ctx.linearizeTrace();
  if (!traceVec.empty()) {
    result->createTrace_lck(traceVec.data(), traceVec.size());
  }

  if (m_isNonMvccAware) {
    assert(traceVec.empty());
    result = assignNonMvccLifespan(*this, *result, ctx);
  }

  // Remove the placeholder from the list, if it's still present
  // (possibly some earlier replacePlaceholder call could have discarded it.)
  //
  // NOTE: We cannot use isAttached_lck here because if it's detached it
  // won't be locked (but if it is attached it will be implicitly).
  if (ctx.m_placeholder.m_ownerAndFlags.isAttached()) {
    detach_lck(ctx.m_placeholder);
  }

  result = insertRevision_lck(std::move(result), true);

  // Ensure this is in the cleanup list (it may be already).
  auto tailEnd = m_tailValue->end_lck();
  if (tailEnd != kNeverTxnId) {
    registerCleanup(*this, tailEnd);
  }

  return {result, result->convertLock(std::move(lock))};
}

void Invocation::asyncEvaluate(Caller& caller) {
  asyncEvaluateWithCustomEval(
      caller, [this]() { return callMemoizeThunk(asIObj()); });
}

template <typename FN>
void Invocation::asyncEvaluateWithCustomEval(Caller& caller, FN eval) {
  Context* ctx = nullptr;

  {
    auto lock = lockify(*this);

    moveToLruHead_lck();

    const TxnId queryTxn = caller.m_queryTxn;
    assert(queryTxn > 0 && queryTxn < kNeverTxnId);

    //
    // Scan the list to find a possible match.
    //
    Revision* prev = nullptr;
    Revision* rev;
    for (rev = m_headValue.ptr(); rev; prev = rev, rev = rev->m_next.ptr()) {
      if (rev->begin_lck() <= queryTxn) {
        break;
      }
    }

    if (rev != nullptr) {
      // We found an entry that's old enough but is it too old?

      if (rev->end_lck() > queryTxn) {
        // Success! We found the Revision encompassing queryTxn.

        if (Context* alreadyRunning = rev->valueAsContext()) {
          // Computation is already underway, just wait for it to finish.
          caller.prepareForDeferredResult();
          alreadyRunning->addCaller_lck(caller);
        } else {
          // Add dependencies while rev is still locked.
          caller.addDependency(*rev);

          // Make sure we aren't holding a lock while running the callback.
          lock.unlock();

          caller.finish();
        }

        return;
      }

      if (rev->hasTrace_lck()) {
        // Try to refresh rev's trace. If this fails the trace will
        // be removed and it will try this method again.

        // TODO: Detect impending deadlock here and just compute the value
        // without creating a placeholder. That's actually tricky since
        // computing the value may insert a Revision before the one being
        // refreshed, which is not allowed. So maybe we need to just
        // force-fail the trace refresh to avoid the deadlock?
        // Maybe wake up the other waiter(s) and only the one at the
        // lowest address is allowed to make progress or something...

        caller.prepareForDeferredResult();

        // Create a Process just for running refresh.
        auto memoProcess = Process::make(ctx);
        memoProcess->m_parent = UnownedProcess(Process::cur());

        {
          // Keep rev alive while we hold a lock to it.
          Revision::Ptr revPtr{rev};
          auto revLock = rev->convertLock(std::move(lock));

          // Refresh in the subprocess we just created.
          ProcessContextSwitcher processGuard{memoProcess};
          Obstack::cur().registerIObj(asIObj());
          rev->asyncRefresh_lck(caller, std::move(revLock));
        }

        memoProcess->runReadyTasksThenDisown();

        return;
      }
    }

    // Failed to find it.

    // Insert a placeholder before we start computing, to prevent any
    // redundant computation, and to detect infinite recursion.

    // TODO: With different deadlock detection we can backdate "begin"
    // for the placeholder.

    const TxnId begin = queryTxn;
    const TxnId end = prev ? prev->begin_lck() : kNeverTxnId;

    ctx = new Context(*this, caller, begin, end);

    insertBefore_lck(ctx->m_placeholder, rev);
  }

  //
  // Run the user's code to compute the answer.
  //

  caller.prepareForDeferredResult();

  auto invocationIObj = asIObj();
  MemoValue result;
  bool ready = false;

  {
    auto memoProcess = Process::make(ctx);
    memoProcess->m_parent = UnownedProcess(Process::cur());

    {
      ProcessContextSwitcher processGuard{memoProcess};

      Obstack::cur().registerIObj(invocationIObj);

      // If the thunk returns nullptr, it means it assumes responsibility
      // for calling ctx->evaluateDone() itself.
      if (auto awaitable = eval()) {
        switch (awaitable->m_continuation.sbits()) {
          case kAwaitableValueMarker:
            SKIP_awaitableToMemoValue(&result, awaitable);
            ready = true;
            break;
          case kAwaitableExceptionMarker:
            result = MemoValue(
                intern(awaitable->m_exception.unsafeAsPtr()),
                MemoValue::Type::kException,
                false);
            ready = true;
            break;
          default:
            // Tell it that we are waiting on it.
            awaitable->m_exception.setSBits(kContextIsAwaitingThis);
        }
      }
    }

    memoProcess->runReadyTasksThenDisown();
  }

  if (ready) {
    ctx->evaluateDone(std::move(result));
  }
}

void Invocation::setOwningList_lck(OwningList list) {
  assertLocked(*this);
  m_owningList = list;
}

struct GenericCaller : Caller {
  GenericCaller(TxnId queryTxn, Context* callingContext)
      : Caller(queryTxn), m_callingContext(callingContext) {}

  void addDependency(Revision& lockedInput) override {
    m_value = lockedInput.value_lck();
    if (m_value.type() == MemoValue::Type::kUndef) {
      // This is a pretty weird case. This caller tried to evaluate an
      // Invocation, but by the time it got the answer back it had become
      // detached and forgotten its value, perhaps because the Invocation
      // fell off the bottom of LRU. In this case we will just trigger
      // a retry() in finish().
      refreshFailed();
    } else {
      if (auto ctx = m_callingContext) {
        ctx->addDependency(lockedInput);
      }

      if (m_invalidationWatcher) {
        m_invalidationWatcher->m_revision->createInvalidationWatcherTrace(
            lockedInput);
      }
    }
  }

 protected:
  MemoValue m_value;
  Context* m_callingContext = nullptr;
  InvalidationWatcher::Ptr m_invalidationWatcher;
};

struct FutureCaller final : GenericCaller, LeakChecker<FutureCaller> {
  FutureCaller(
      Invocation& invocation,
      MemoTask::Ptr memoTask,
      bool subscribeToInvalidations,
      bool preserveException)
      : GenericCaller(memoTask->m_queryTxn, nullptr),
        m_memoTask(std::move(memoTask)),
        m_invocation(&invocation),
        m_preserveException(preserveException) {
    if (subscribeToInvalidations) {
      m_invalidationWatcher = InvalidationWatcher::make();
    }
  }

  FutureCaller(
      Invocation& invocation,
      Context& callingContext,
      bool subscribeToInvalidations,
      bool preserveException)
      : GenericCaller(callingContext.m_queryTxn, &callingContext),
        m_invocation(&invocation),
        m_preserveException(preserveException) {
    if (subscribeToInvalidations) {
      m_invalidationWatcher = InvalidationWatcher::make();
    }
  }

  void addDependency(Revision& lockedInput) override {
    m_value = lockedInput.value_lck();
    if (m_value.type() == MemoValue::Type::kUndef) {
      // This is a pretty weird case. This caller tried to evaluate an
      // Invocation, but by the time it got the answer back it had become
      // detached and forgotten its value, perhaps because the Invocation
      // fell off the bottom of LRU. In this case we will just trigger
      // a retry() in finish().
      refreshFailed();
    } else {
      if (auto ctx = m_callingContext) {
        ctx->addDependency(lockedInput);
      }

      if (m_invalidationWatcher) {
        m_invalidationWatcher->m_revision->createInvalidationWatcherTrace(
            lockedInput);
      }
    }
  }

  void retry() override {
    m_invocation->asyncEvaluate(*this);
  }

  void finish() override {
    // ??? Yuck this is convoluted.
    if (m_refreshFailed) {
      m_refreshFailed = false;
      retry();
      return;
    }

    auto promise = std::move(m_promise);
    auto value = std::move(m_value);
    auto watcher = std::move(m_invalidationWatcher);
    auto preserveException = m_preserveException;

    delete this;

    if (!preserveException && value.type() == MemoValue::Type::kException) {
      String::CStrBuffer buf;
      auto msg =
          SKIP_getExceptionMessage(const_cast<MutableIObj*>(value.asIObj()));
      promise.set_exception(
          make_exception_ptr(std::runtime_error(msg.c_str(buf))));
    } else {
      AsyncEvaluateResult result;
      result.m_value = std::move(value);

      if (watcher && watcher->isSubscribed()) {
        result.m_watcher = std::move(watcher);
      }

      promise.set_value(std::move(result));
    }
  }

  std::future<AsyncEvaluateResult> getFuture() {
    return m_promise.get_future();
  }

 private:
  // Only finish() should be calling this.
  ~FutureCaller() override = default;

  MemoTask::Ptr m_memoTask;
  Invocation::Ptr m_invocation;
  const bool m_preserveException;

  std::promise<AsyncEvaluateResult> m_promise;
};

std::future<AsyncEvaluateResult> Invocation::asyncEvaluate(
    bool preserveException,
    bool subscribeToInvalidations,
    MemoTask::Ptr memoTask) {
  FutureCaller* caller;

  if (auto ctx = Context::current()) {
    // Don't specify the memoTask if there is a calling context.
    assert(!memoTask);
    caller = new FutureCaller(
        *this, *ctx, subscribeToInvalidations, preserveException);
  } else {
    if (!memoTask) {
      memoTask = createMemoTask();
    }

    caller = new FutureCaller(
        *this,
        std::move(memoTask),
        subscribeToInvalidations,
        preserveException);
  }

  auto future = caller->getFuture();

  asyncEvaluate(*caller);

  return future;
}

std::future<AsyncEvaluateResult> Invocation::asyncEvaluateAndSubscribe() {
  return asyncEvaluate(false, true, 0);
}

std::future<AsyncEvaluateResult> Invocation::asyncEvaluate(
    MemoTask::Ptr memoTask) {
  return asyncEvaluate(false, false, std::move(memoTask));
}

namespace {
// Each Cell points to a dummy Invocation used to holds its Revision list.
// This type describes the layout of those dummy objects.
Type& static_cellInvocationType() {
  static auto singleton = Type::avoidInternTable(Type::invocationFactory(
      "CellInvocation", sizeof(Invocation) - Invocation::kMetadataSize, {}));
  return *singleton;
}

bool isCell(Invocation& inv) {
  return inv.asIObj()->type() == static_cellInvocationType();
}
} // namespace

void Invocation::moveToLruHead_lck() {
  if (UNLIKELY(isCell(*this))) {
    // We never record these special kinds of Invocations in LRU,
    // as they are not discardable.
    return;
  }

  switch (m_owningList) {
    case OwningList::kCleanup:
      // Always leave objects in the cleanup list.
      break;
    case OwningList::kLru:
      // The value is already in the LRU list.
      //
      // For scalability, only move to LRU head if the LRU lock isn't taken.
      // The idea is that "hot" items will eventually succeed in getting
      // moved to the head, and for "cold" items we don't much care.
      //
      // This triples the speed of some benchmarks on Mac.
      s_lruList.insertIfUnlocked(*this);
      break;
    case OwningList::kNone:
      // We must always insert into the LRU list, even if the lock is
      // contended, so we can locate and free the memory later if the
      // cache gets full.
      s_lruList.insert(*this);
      setOwningList_lck(OwningList::kLru);
      break;
  }
}

/**
 * A linked list of Invocations whose cleanup() method should be called
 * once oldestVisibleTxn() hits a specific value. s_cleanupLists maintains
 * a mapping from TxnId to the CleanupList that handles that oldestVisibleTxn()
 * reaching that txn.
 *
 * Pushing something onto this list only requires the read lock
 */
struct CleanupList final : private skip::noncopyable {
  CleanupList() : m_numActiveMemoTasks(0), m_head(nullptr) {}

  // Atomically pushes a locked Invocation onto the cleanup list.
  //
  // This object does not have a lock, and does not need one, but you must
  // have s_cleanupListsMutex read- or write-locked to prevent this
  // list from being destroyed while pushing a value.
  //
  // Extracting the list requires having s_cleanupListsMutex write-locked.
  void push(Invocation::Ptr invPtr) {
    // Steal the reference from the caller.
    auto inv = boost_detach(invPtr);

    assertLocked(*inv);
    assert(!inv->inList_lck());

    inv->setOwningList_lck(OwningList::kCleanup);

    auto head = m_head.load(std::memory_order_relaxed);
    do {
      inv->m_lruNext = head;
    } while (UNLIKELY(!m_head.compare_exchange_weak(
        head, inv, std::memory_order_release, std::memory_order_relaxed)));

    if (head == nullptr) {
      m_tail = inv;
    }
  }

  /**
   * Steal and concatenate this list to the given one. This can only
   * be called with s_cleanupListsMutex write-locked, so we know no
   * one can call "push".
   */
  void clearAndConcatTo_lck(Invocation*& head, Invocation*& tail) {
    // Steal this object's list.
    if (auto myHead = m_head.load(std::memory_order_relaxed)) {
      m_head.store(nullptr, std::memory_order_relaxed);
      auto myTail = m_tail;
      m_tail = nullptr;

      // Concatenate it to the caller's linked list.
      if (tail == nullptr) {
        head = myHead;
      } else {
        tail->m_lruNext = myHead;
      }
      tail = myTail;
    }
  }

  void taskFinished() {
    if (--m_numActiveMemoTasks == 0) {
      // We do not know if we can run cleanups for this list yet --
      // that's only legal if no older tasks are still running. So call
      // runCleanupsSoon() and let it figure that out.
      runCleanupsSoon();
    }
  }

  // Number of tasks running at this cleanup's TxnId (which is implicit
  // in its key in s_cleanupLists).
  AtomicRefcount m_numActiveMemoTasks;

  // Singly-linked, atomically prepended list of everything in this cleanup.
  // The only operations are "prepend" and "steal entire list".
  std::atomic<Invocation*> m_head;
  Invocation* m_tail{nullptr};
};

Refresher::Refresher(Caller& firstCaller, Revision& lockedRefreshee)
    : m_refreshee(&lockedRefreshee),
      m_callers(firstCaller),
      m_queryTxn(firstCaller.m_queryTxn),
      m_latestVisibleWhenStarted(newestVisibleTxn()),
      m_invocation(m_refreshee->owner_lck()) {
  assert(firstCaller.m_queryTxn <= m_latestVisibleWhenStarted);

  DEBUG_TRACE("Beginning refresh for " << m_refreshee);
}

// Advances the state machine refreshing m_refreshee.
void Refresher::continueRefresh_lck(
    EdgeIndex nextIndex,
    RevisionLock refresheeLock) {
  assertLocked(*m_refreshee);

  DEBUG_TRACE("continueRefresh for " << RevDetails(m_refreshee, true));

  // Plan A: Try refreshing all inputs. If they all refresh then ew do
  // not need to run m_refreshee's Invocation.
  if (m_refreshee->hasTrace_lck()) {
    Trace& trace = m_refreshee->m_trace;

    // Only the head of the revision list or a detached Revision
    // can have a trace.
    assert(m_refreshee->m_prev == nullptr);

    // Consider only the bits we haven't examined yet.
    // Older ones may "turn on" behind us due to invalidations,
    // but that is OK (see "trace.inactive() != 0" check below).
    auto mask = trace.inactive() & ~((1ull << nextIndex) - 1);

    for (auto n = mask; n != 0; n &= n - 1) {
      auto index = skip::findFirstSet(n) - 1;
      auto child = trace[index].target();

      auto childLock = lockify(*child);

      // See if the child is actually up to date enough for our purposes.
      const auto childEnd = child->end_lck();

      if (childEnd <= m_queryTxn) {
        // The hard case -- recursively refresh this child.

        // Prevent child from being freed once we drop refresheeLock.
        Revision::Ptr childPtr(child);

        // Unlock this object while we recurse. This may be unnecessary,
        // but the recursion may cause complex things to happen and
        // we don't want to risk either deadlock or scalability issues.
        refresheeLock.unlock();

        // TODO: Launch multiple children in parallel if we know they
        // have no control dependencies.

        auto r = new RefreshCaller(*this, childPtr, index);
        child->asyncRefresh_lck(*r, std::move(childLock));

        return;
      } else if (childEnd == kNeverTxnId) {
        // It's actually active, so clear the flag for next time.
        trace.setActive(index, *m_refreshee, __LINE__);
      } else {
        m_end = std::min(m_end, childEnd);
      }
    }

    assert(m_end > m_queryTxn);

    if (trace.inactive() != 0) {
      // It's possible something got invalidated while we were refreshing.
      // We might mistakenly believe this trace has an "end" of kNeverTxnId,
      // because every input we refreshed said so, but while we were doing
      // that some input we already processed got set to "inactive".
      //
      // When that happens, we conservatively assume that the invalidated
      // inputs were invalidated at m_latestVisibleWhenStarted + 1, which
      // is the first txn where that could have happened.
      DEBUG_TRACE(
          "Input " << trace[skip::findFirstSet(trace.inactive()) - 1].target()
                   << " for " << this << " still inactive so reducing end from "
                   << m_end << " to " << (m_latestVisibleWhenStarted + 1));
      m_end = std::min(m_end, m_latestVisibleWhenStarted + 1);
    }

    {
      auto result = std::move(m_refreshee);

      DEBUG_TRACE(
          "Refresh succeeded for "
          << RevDetails(result) << " from " << result->end_lck() << " to "
          << m_end << "; inactive = 0x" << std::hex << trace.inactive()
          << std::dec << "; inputs are: " << trace);

      result->m_refresher = nullptr;
      result->setEnd_lck(m_end, __LINE__);

      m_callers.notifyCallers_lck(*result, std::move(refresheeLock));
    }

    delete this;
  } else {
    // Plan B: Run the user's code.
    runInvocation_lck(std::move(refresheeLock));
  }
}

// Called when some input fails to refresh.
void Refresher::inputRefreshFailed_lck() {
  // Kill the trace so no one ever tries to refresh it again.
  // Note that we may end up creating another Trace after re-running the
  // invocation.
  m_refreshee->clearTrace_lck();
}

void Refresher::runInvocation_lck(RevisionLock refresheeLock) {
  assert(!m_refreshee->hasTrace_lck());

  DEBUG_TRACE("Running invocation to refresh " << RevDetails(m_refreshee));

  m_refreshee->m_refresher = nullptr;

  auto callers = m_callers.stealList_lck();

  auto inv = std::move(m_invocation);

  refresheeLock.unlock();

  delete this;

  // We don't want to call any callbacks with locks still held.
  assert(!lockManager().deferWork());

  // We don't want to call any callbacks with locks still held.
  // TODO: If we are trying to refresh some old Revision, e.g.
  // m_refreshee->end_lck() == 100, and we want to refresh at txn 300, and
  // there is another Revision with a different, non-placeholder value
  // with begin == 200, , we can just instantly fail here without bothering
  // to run the Invocation, since we know there is no way the computed value
  // could possibly be contiguous with m_refreshee with that incompatible
  // value sitting there at 200.

  if (inv) {
    // Find the oldest queryTxn.
    TxnId oldest = kNeverTxnId;
    for (auto c = callers; c != nullptr; c = c->m_next) {
      oldest = std::min(oldest, c->m_queryTxn);
    }

    // Move the oldest one to the head of the list, so it gets run first.
    // It will create one placeholder that (probably) engulfs all callers.
    Caller** prevp = &callers;
    for (auto c = callers; c != nullptr; prevp = &c->m_next, c = *prevp) {
      if (c->m_queryTxn == oldest) {
        *prevp = c->m_next;
        c->m_next = callers;
        callers = c;
        break;
      }
    }

    // Run the Invocation for all the callers.
    invokeOnList(callers, [inv](Caller* c) {
      return [inv, c]() { inv->asyncEvaluate(*c); };
    });
  } else {
    // Tell the callers the bad news.
    invokeOnList(callers, [inv](Caller* c) {
      return [inv, c]() {
        c->refreshFailed();
        c->finish();
      };
    });
  }
}

void TraceArray::operator delete(void* ptr) {
  // This pairs with TraceArray::make() which calls allocAligned().
  ::free(ptr);
}

TraceArray* TraceArray::make(size_t size) {
  assert(size > 1 && size <= kMaxTraceSize);

  size_t elementSize = sizeof(((TraceArray*)nullptr)->m_inputs[0]);
  void* mem = allocAligned(
      sizeof(TraceArray) + size * elementSize, alignof(TraceArray));

  return new (mem) TraceArray((EdgeIndex)size);
}

TraceArray::~TraceArray() {
  for (size_t i = 0, end = m_size; i < end; ++i) {
    DownEdge input = m_inputs[i];
    if (auto rev = input.target()) {
      rev->unsubscribe(input);
      rev->decref();
    }
  }
}

static_assert(sizeof(Trace) <= 6, "Expected a single tagged pointer");

Trace::Trace(size_t size) {
  if (size != 1) {
    m_rep.assign(size ? TraceArray::make(size) : nullptr, kTraceArrayTag);
  } else {
    // This is not in a good state yet.
    m_rep.assign(nullptr, (size_t)kShortNoEdgeIndex << 1);
  }
}

Trace::Trace(Trace&& other) noexcept : Trace() {
  *this = std::move(other);
}

Trace& Trace::operator=(Trace&& other) noexcept {
  std::swap(m_rep, other.m_rep);
  return *this;
}

Trace::~Trace() {
  clear();
}

void Trace::verifyInvariants(const Revision& owner) const {
  assertLocked(owner);

  if (empty()) {
    return;
  }

  TxnId minEnd = kNeverTxnId;
  TxnId maxBegin = 0;

  uint64_t actuallyInactive = 0;

  for (size_t i = 0, numInputs = size(); i < numInputs; ++i) {
    auto e = (*this)[i];
    assert(e.isDownEdge());

    auto input = e.target();
    auto lock = lockify(input);

    auto reverse ATTR_UNUSED = e.dereference();
    assert(!reverse.isDownEdge());
    assert(reverse.target() == &owner);
    assert(reverse.index() == i);

    // This reference should count as a refcount.
    assert(input->currentRefcount() > 0);

    // Permanently active objects should never appear in a trace.
    // TODO: If we ever let something retroactively become permanently
    // active (like, the earlier lifespan estimate was pessimistic), then
    // we can remove this.
    assert(!input->isPure_lck());

    const TxnId inputEnd = input->end_lck();
    minEnd = std::min(minEnd, inputEnd);
    maxBegin = std::min(maxBegin, input->begin_lck());

    const TxnId ownerEnd = owner.end_lck();

    if (inputEnd < kNeverTxnId) {
      // Normally we would expect every input to have an end() >= ownerEnd.
      // However, an invalidate propagating "up" the graph may have reduced
      // an input's end() from kNeverTxnId to a finite value, but not told
      // owner about it yet. Due to lock ordering issues, the invalidate
      // must temporarily not hold a lock on either owner or input, which
      // could be why we have both of them locked right here and now.
      //
      // When this race happens we know that the thread committing a transaction
      // is waiting to propagate the invalidate() from input to owner, which
      // will be taking place at newestVisibleTxn() + 1, the txn in the
      // process of being committed.

      TxnId newest = newestVisibleTxn();

      if (!(inputEnd >= ownerEnd ||
            (ownerEnd == kNeverTxnId && inputEnd == newest + 1))) {
        DEBUG_TRACE(
            "@@@ about to lose for parent="
            << RevDetails(owner) << "; input=" << input << ": inputEnd="
            << inputEnd << ", ownerEnd=" << ownerEnd << ", newest=" << newest
            << ", #invals=" << lockManager().invalidationStack().size());
      }

      assert(
          inputEnd >= ownerEnd ||
          (ownerEnd == kNeverTxnId && inputEnd == newest + 1));

      actuallyInactive |= 1ull << i;

      // If an input in inactive and cannot ever be refreshed, then
      // this trace should not even exist. It's pointless. We should
      // have done a "permanent invalidate".

      if (g_oneThreadActive) {
        // But we cannot safely test this with threads because there is valid
        // race race where the input is in the process of telling us about
        // a permanent invalidate but is waiting for owner's lock.

        // TODO: Reenable
        //         assert(input->canRefresh());
      }
    }
  }

  if (g_oneThreadActive) {
    // All the inactive inputs must be marked as such. Because updating is lazy,
    // it is OK to have "false positives" where an active input is incorrectly
    // believed to be inactive.
    assert((actuallyInactive & ~inactive()) == 0);

    // Our lifespan is allowed to be a bit pessimistic, as an input may
    // refresh in a way that we don't know about. This is especially true when
    // an input's begin() refreshes to an earlier value.

    assert(minEnd >= owner.end_lck());
    assert(maxBegin <= owner.begin_lck());
  } else {
    // ??? TODO: Is there a way we can safely assert something here? The problem
    // is that an input might be in the process of invalidating this object
    // but unable to do so due the lock on 'owner', leaving it momentarily
    // inconsistent (but in a way that is handled).
  }
}

EdgeIndex Trace::size() const {
  auto vp = m_rep.unpack();
  if (vp.m_tag == kTraceArrayTag) {
    if (auto a = static_cast<TraceArray*>(vp.m_ptr)) {
      return a->m_size;
    } else {
      return 0;
    }
  } else {
    return 1;
  }
}

const DownEdge Trace::inlineEdge() const {
  auto vp = m_rep.unpack();

  // The low bit holds the "inactive" flag. Shift it out to get the index.
  EdgeIndex index = vp.m_tag >> 1;

  if (index == kShortInlineSubscriptionIndex) {
    return DownEdge(*static_cast<Revision*>(vp.m_ptr));
  } else {
    if (index == kShortNoEdgeIndex) {
      index = DownEdge::kNoEdgeIndex;
    }

    return DownEdge(*static_cast<SubArray*>(vp.m_ptr), index);
  }
}

const DownEdge Trace::operator[](EdgeIndex index) const {
  auto vp = m_rep.unpack();

  if (vp.m_tag != kTraceArrayTag) {
    return inlineEdge();
  } else if (auto a = static_cast<TraceArray*>(vp.m_ptr)) {
    assert(index < a->m_size);
    return a->m_inputs[index];
  } else {
    return DownEdge();
  }
}

bool Trace::empty() const {
  auto vp = m_rep.unpack();
  return vp.m_tag == kTraceArrayTag && vp.m_ptr == nullptr;
}

void Trace::clear() {
  auto vp = m_rep.unpack();
  if (vp.m_ptr != nullptr) {
    if (vp.m_tag == kTraceArrayTag) {
      delete static_cast<TraceArray*>(vp.m_ptr);
    } else {
      auto input = inlineEdge();
      auto rev = input.target();
      rev->unsubscribe(input);
      rev->decref();
    }
  }

  m_rep.assign(nullptr, kTraceArrayTag);
}

uint64_t Trace::inactive() const {
  if (auto a = array()) {
    return a->m_inactive;
  } else {
    return m_rep.tag() & 1;
  }
}

void Trace::setInactive(
    EdgeIndex index,
    Revision& owner ATTR_UNUSED,
    int line ATTR_UNUSED) {
  assertLocked(owner);
  assert(index < size());

#if ENABLE_DEBUG_TRACE
  const auto oldInactive = inactive();
#endif

  if (auto a = array()) {
    a->m_inactive |= 1ull << index;
  } else {
    auto vp = m_rep.unpack();
    m_rep.assign(vp.m_ptr, vp.m_tag | 1);
  }

  DEBUG_TRACE(
      "Setting inactive from 0x"
      << std::hex << oldInactive << " to 0x" << inactive() << std::dec
      << " for " << RevDetails(owner, false) << " because of line " << line);
}

void Trace::setActive(
    EdgeIndex index,
    Revision& owner ATTR_UNUSED,
    int line ATTR_UNUSED) {
  assertLocked(owner);
  assert(index < size());

#if ENABLE_DEBUG_TRACE
  const auto oldInactive = inactive();
#endif

  if (auto a = array()) {
    a->m_inactive &= ~(1ull << index);
  } else {
    auto vp = m_rep.unpack();
    m_rep.assign(vp.m_ptr, vp.m_tag & ~1);
  }

  DEBUG_TRACE(
      "Clearing inactive from 0x"
      << std::hex << oldInactive << " to 0x" << inactive() << std::dec
      << " for " << RevDetails(owner, false) << " because of line " << line);
}

void Trace::assign(EdgeIndex index, DownEdge downEdge) {
  if (auto a = array()) {
    assert(index < a->m_size);
    a->m_inputs[index] = downEdge;
  } else {
    assert(index == 0);

    // Convert the special index values to our more compact form.
    auto downIndex = downEdge.index();

    constexpr EdgeIndex delta = DownEdge::kNoEdgeIndex - kShortNoEdgeIndex;
    if (downIndex >= DownEdge::kNoEdgeIndex) {
      downIndex -= delta;
    }
    assert(downIndex <= kShortInlineSubscriptionIndex);

    // Make sure the above code works for kInlineSubscriptionIndex as well.
    static_assert(kShortNoEdgeIndex < kShortInlineSubscriptionIndex, "");
    static_assert(
        kShortInlineSubscriptionIndex + delta ==
            DownEdge::kInlineSubscriptionIndex,
        "");

    // Left-shift tag by one for inactive() flag.
    m_rep.assign(downEdge.rawPointer(), (size_t)downIndex << 1);
  }
}

TraceArray* Trace::array() const {
  auto vp = m_rep.unpack();
  if (vp.m_tag == kTraceArrayTag) {
    return static_cast<TraceArray*>(vp.m_ptr);
  } else {
    return nullptr;
  }
}

SubArray::SubArray(Revision& owner, SubArray* nextArray)
    : m_owner(owner), m_next(nextArray) {
  // Chain together array entries into a freelist.
  FreelistEdge next;
  for (size_t i = kArraySize; i-- != 0;) {
    m_subs[i] = next;
    next = DownEdge(*this, i);
  }
}

SubscriptionSet::SubscriptionSet() : m_subs() {}

SubscriptionSet::~SubscriptionSet() {
  clear();
}

void SubscriptionSet::clear() {
  for (SubArray *a = m_subs.asSubArray(), *next; a != nullptr; a = next) {
    next = a->m_next;
    delete a;
  }
  m_subs.reset();
}

bool SubscriptionSet::obviouslyEmpty() const {
  return m_subs.isNull();
}

UpEdge SubscriptionSet::inlineSubscriber() {
  Edge e;
  if (SubArray* a = m_subs.asSubArray()) {
    e = a->m_subs[kIndexForInlineSubscriptionMovedToSubArray];
  } else {
    e = m_subs;
  }
  return UpEdge(e);
}

void SubscriptionSet::unsubscribe(DownEdge edge) {
  auto index = edge.index();
  SubArray* array;

  if (index == DownEdge::kInlineSubscriptionIndex) {
    // This edge claims to be stored directly in our "inline" slot (m_subs),
    // rather than in an external SubArray. If that's still true, it's
    // easy to unsubscribe. But if we ever got a second subscription we
    // had to move it to a SubArray.
    array = m_subs.asSubArray();

    if (array == nullptr) {
      // It is still inline, no subscriptions to update.
      m_subs.reset();
      return;
    }

    // It should be in the second slot in the first SubArray.
    index = kIndexForInlineSubscriptionMovedToSubArray;
  } else {
    array = edge.asSubArray();
  }

  if (array != nullptr) {
    assert(index < SubArray::size());
    auto& fHead = freelistHead();
    array->m_subs[index] = fHead;
    fHead = DownEdge(*array, index);
  }
}

void SubscriptionSet::subscribe(Revision& owner, UpEdge subscriber) {
  if (obviouslyEmpty()) {
    // Zero subscriptions so far, so just use m_subs as a special inline slot.
    m_subs = subscriber;
    subscriber.assign(DownEdge(owner));
    return;
  }

  if (m_subs.asRevision() != nullptr) {
    // Migrate the entry from the special inline slot to an array, to make
    // room for m_subs to become a pointer to a linked list of arrays.
    auto array = new SubArray(owner, nullptr);
    array->m_subs[kIndexForInlineSubscriptionMovedToSubArray] = m_subs;

    // The first entry in the first list is reserved for the freelist head,
    // and the second is for kIndexForInlineSubscriptionMovedToSubArray,
    // so the freelist starts with the third entry.
    array->m_subs[0] = FreelistEdge(*array, 2);

    m_subs = DownEdge(*array, 0);
  }

  auto& fHead = freelistHead();
  if (fHead.isNull()) {
    // Freelist is empty. Allocate another SubArray and splice it in right
    // after the head of the list, which is special because its first slot
    // holds the freelist.
    auto headArray = m_subs.asSubArray();
    auto newArray = new SubArray(owner, headArray->m_next);
    headArray->m_next = newArray;
    fHead = FreelistEdge(*newArray, 0);
  }

  // Make subscriber point to the newly-allocated edge slot and vice versa.
  auto freeArray = fHead.asSubArray();
  assert(freeArray != nullptr);
  auto freeIndex = fHead.index();
  fHead = freeArray->m_subs[freeIndex];
  freeArray->m_subs[freeIndex] = subscriber;
  subscriber.assign(DownEdge(*freeArray, freeIndex));
}

int SubscriptionSet::compareSizes(const SubscriptionSet& x) const {
  if (this == &x) {
    return 0;
  }

  for (auto it1 = begin(), it2 = x.begin(), end1 = end(), end2 = x.end();;
       ++it1, ++it2) {
    bool done1 = (it1 == end1);
    bool done2 = (it2 == end2);
    if (done1 || done2) {
      return done2 - done1;
    }
  }
}

void SubscriptionSet::verifyInvariants(const Revision& owner) const {
  assertLocked(owner);

  if (auto head = m_subs.asSubArray()) {
    // The first slot in the first SubArray is the freelist head.
    assert(head->m_subs[0].isSubArray());

    // SubArray -> bit mask of which slots are free.
    skip::fast_map<SubArray*, uint16_t> freeSlots;
    static_assert(SubArray::size() <= sizeof(uint16_t) * 8, "");

    // Total number of freelist entries encountered.
    size_t expectedFreelistSize = 0;

    // Check invariants for the linked list and record where the free slots are.
    for (auto p = head; p != nullptr; p = p->m_next) {
      assert(&p->m_owner == &owner);

      assert(reinterpret_cast<uintptr_t>(p) % kSubArrayAlign == 0);

      uint16_t* mask = nullptr;

      for (EdgeIndex i = (p == head) ? 1 : 0; i < SubArray::size(); ++i) {
        DownEdge thisSlot(*p, i);

        if (p->m_subs[i].isDownEdge()) {
          // Update a bit mask of all of the free slots.
          if (mask == nullptr) {
            mask = &freeSlots.insert({p, 0}).first->second;
          }
          *mask |= 1u << i;
          ++expectedFreelistSize;
        } else {
          UpEdge subscriber(p->m_subs[i]);
          DownEdge reverse ATTR_UNUSED = subscriber.dereference();
          assert(reverse.target() == &owner);

          // Either the reverse edge should point back at us, or it can
          // still legally be pointing to the inline slot if the slot
          // we are looking at is the one where the inline slot gets moved to.
          assert(
              reverse == thisSlot ||
              (p == head && reverse.asRevision() == &owner &&
               reverse.index() == DownEdge::kInlineSubscriptionIndex &&
               i == kIndexForInlineSubscriptionMovedToSubArray));
        }
      }
    }

    // Walk the freelist and make sure it's exactly the free slots we saw.
    size_t freelistSize = 0;
    for (DownEdge d(freelistHead()); !d.isNull(); ++freelistSize) {
      auto nextArray = d.asSubArray();
      assert(nextArray != nullptr);

      auto nextIndex = d.index();
      assert(nextIndex < SubArray::size());

      auto it = freeSlots.find(nextArray);
      assert(it != freeSlots.end());

      // Make sure it's free.
      auto mask = 1u << nextIndex;
      assert(it->second & mask);

      // Clear the free bit to catch infinite loops.
      it->second -= mask;

      d = DownEdge(nextArray->m_subs[nextIndex]);
    }

    assert(freelistSize == expectedFreelistSize);
  } else if (!m_subs.isNull()) {
    // Make sure this is an UpEdge whose target points back at us.
    // This check is not thread safe!
    UpEdge subscriber(m_subs);
    DownEdge reverse ATTR_UNUSED = subscriber.dereference();
    assert(reverse.target() == &owner);
    assert(reverse.index() == DownEdge::kInlineSubscriptionIndex);
  }
}

SubscriptionSet::iterator::iterator() = default;

SubscriptionSet::iterator::iterator(Edge pos) : m_pos(pos) {}

const UpEdge& SubscriptionSet::iterator::dereference() const {
  const Edge* e;

  if (auto a = m_pos.asSubArray()) {
    e = &a->m_subs[m_pos.index()];
  } else {
    e = &m_pos;
  }

  return *static_cast<const UpEdge*>(e);
}

bool SubscriptionSet::iterator::equal(const iterator& other) const {
  return m_pos == other.m_pos;
}

void SubscriptionSet::iterator::increment() {
  if (auto a = m_pos.asSubArray()) {
    for (EdgeIndex i = m_pos.index() + 1;; i = 0) {
      // Find the next live slot in the current array, if any.
      for (; i < SubArray::size(); ++i) {
        if (!a->m_subs[i].isSubArray()) {
          // Found a non-freelist entry.
          m_pos = DownEdge(*a, i);
          return;
        }
      }

      // Start over on the next array.
      a = a->m_next;

      if (a == nullptr) {
        break;
      }
    }
  }

  // We hit the end, make it the same thing that end() returns.
  *this = iterator();
}

SubscriptionSet::iterator SubscriptionSet::begin() const {
  if (obviouslyEmpty()) {
    // Empty set.
    return end();
  } else if (auto a = m_subs.asSubArray()) {
    // The first slot in the SubArray just contains the freelist head,
    // so advance beyond it to the first real subscription (if any).
    return ++iterator(Edge(a, 0));
  } else {
    // The "inline edge" case.
    return iterator(m_subs);
  }
}

SubscriptionSet::iterator SubscriptionSet::end() const {
  return iterator();
}

Edge& SubscriptionSet::freelistHead() {
  // The first edge in the first SubArray holds the freelist head.
  auto array = m_subs.asSubArray();
  assert(array != nullptr);
  return array->m_subs[0];
}

const Edge SubscriptionSet::freelistHead() const {
  return const_cast<SubscriptionSet*>(this)->freelistHead();
}

void SubscriptionSet::invalidateSubscribers_lck(Revision& owner) {
  assertLocked(owner);
  auto& stack = lockManager().invalidationStack();

  for (auto upEdge : *this) {
    // Push a strong (incref'd) edge on the invalidation stack.
    //
    // There is a race where the subscriber's refcount might already have hit
    // zero but it hasn't unsubscribed yet (perhaps because it is waiting
    // on the Revision lock this thread holds right now). Lock ordering rules
    // prevent us from locking it.
    //
    // We use increfFromNonZero to just ignore such subscribers. They will
    // be dead soon enough and don't need to be told about this invalidation.
    if (increfFromNonZero(upEdge.target()->m_refcount)) {
      DEBUG_TRACE(
          "Invalidating from child " << RevDetails(owner) << " to parent "
                                     << upEdge.target()
                                     << ": index=" << (int)upEdge.index());
      stack.push_back(upEdge);
    } else {
      DEBUG_TRACE("Racing " << upEdge.target() << ":" << (int)upEdge.index());
    }
  }
}

CallerList::CallerList() {
  m_callers = nullptr;
}

CallerList::CallerList(CallerList&& other) noexcept {
  *this = std::move(other);
}

CallerList& CallerList::operator=(CallerList&& other) noexcept {
  m_callers = other.m_callers;
  other.m_callers = nullptr;
  return *this;
}

CallerList::CallerList(Caller& first) {
  first.m_next = nullptr;
  m_callers = &first;
}

std::ostream& operator<<(std::ostream& out, CallerList& caller) {
  out << "CallerList[";
  const char* sep = "";
  for (Caller* c = caller.m_callers.ptr(); c; c = c->m_next) {
    out << sep << "Caller " << (void*)c << " @ txn=" << c->m_queryTxn;
    sep = ", ";
  }
  out << "]";
  return out;
}

Caller* CallerList::stealList_lck() {
  Caller* c = m_callers.ptr();
  m_callers = nullptr;
  return c;
}

void CallerList::addCaller_lck(Caller& caller) {
  // TODO: Detect infinite recursion here.
  caller.m_next = m_callers.ptr();
  m_callers = &caller;
}

void CallerList::notifyCallers_lck(Revision& rev, RevisionLock lock) {
  // Steal the list while we still have the lock.
  Caller* c = stealList_lck();

  Caller* retry = nullptr;
  Caller* done = nullptr;

  const TxnId revBegin = rev.begin_lck();
  const TxnId revEnd = rev.end_lck();

  // Now provide all waiting callers with the computed value.
  for (Caller* next; c != nullptr; c = next) {
    next = c->m_next;

    if (LIKELY(c->m_queryTxn >= revBegin && c->m_queryTxn < revEnd)) {
      // Add a dependency while we still hold lock.
      c->addDependency(rev);

      // Prepend to list of callbacks to notify.
      c->m_next = done;
      done = c;
    } else {
      // The lifespan did not turn out to intersect the desired queryTxn,
      // because the placeholder's lifespan was too optimistic. We
      // need to re-run it.
      c->m_next = retry;
      retry = c;

      DEBUG_TRACE(
          "Need to retry because " << c->m_queryTxn << " not in range ["
                                   << revBegin << ", " << revEnd << ")");
    }
  }

  lock.unlock();

  // We don't want to call any callbacks with locks still held.
  assert(!lockManager().deferWork());

  invokeOnList(done, [](Caller* q) { return [q]() { q->finish(); }; });
  invokeOnList(retry, [](Caller* q) { return [q]() { q->retry(); }; });
}

Edge::Edge() : Edge(static_cast<Revision*>(nullptr), kNoEdgeIndex) {}

Edge::Edge(SubArray* array, EdgeIndex index) {
  assert(index <= kInlineSubscriptionIndex);
  m_pointerAndIndex.assign(array, index | kSubArrayFlag);
}

Edge::Edge(Revision* rev, EdgeIndex index) {
  assert(index <= kInlineSubscriptionIndex);

  m_pointerAndIndex.assign(rev, index);
}

void Edge::reset() {
  *this = Edge();
}

Revision* Edge::target() const {
  if (auto array = asSubArray()) {
    return &array->m_owner;
  } else {
    return static_cast<Revision*>(m_pointerAndIndex.ptr());
  }
}

void* Edge::rawPointer() const {
  return m_pointerAndIndex.ptr();
}

EdgeIndex Edge::index() const {
  return m_pointerAndIndex.tag() & ((1u << kAlignBits) - 1);
}

bool Edge::isSubArray() const {
  return (m_pointerAndIndex.tag() & kSubArrayFlag) != 0;
}

SubArray* Edge::asSubArray() const {
  if (isSubArray()) {
    return reinterpret_cast<SubArray*>(m_pointerAndIndex.ptr());
  } else {
    return nullptr;
  }
}

Revision* Edge::asRevision() const {
  if (isSubArray()) {
    return nullptr;
  } else {
    return reinterpret_cast<Revision*>(m_pointerAndIndex.ptr());
  }
}

bool Edge::isNull() const {
  return m_pointerAndIndex == nullptr;
}

bool Edge::isDownEdge() const {
  // This returns true if either (1) kSubArrayFlag is set or (2)
  // index() == kInlineSubscriptionIndex, which are the two cases
  // for DownEdges.
  return m_pointerAndIndex.tag() >= kInlineSubscriptionIndex;
}

bool Edge::operator==(const Edge& other) const {
  return m_pointerAndIndex.bits() == other.m_pointerAndIndex.bits();
}

bool Edge::operator<(const Edge& other) const {
  return m_pointerAndIndex.bits() < other.m_pointerAndIndex.bits();
}

DownEdge::DownEdge() : Edge((SubArray*)nullptr, kNoEdgeIndex) {}

DownEdge::DownEdge(SubArray& array, EdgeIndex index) : Edge(&array, index) {}

DownEdge::DownEdge(const Revision& rev)
    : Edge(const_cast<Revision*>(&rev), kInlineSubscriptionIndex) {}

DownEdge::DownEdge(Edge edge) {
  assert(edge.isDownEdge());
  static_cast<Edge&>(*this) = edge;
}

UpEdge DownEdge::dereference() const {
  auto i = index();
  if (auto a = asSubArray()) {
    return UpEdge(a->m_subs[i]);
  } else {
    assert(i == DownEdge::kInlineSubscriptionIndex);
    return asRevision()->m_subs.inlineSubscriber();
  }
}

DownEdge UpEdge::dereference() const {
  return asRevision()->m_trace[index()];
}

UpEdge::UpEdge(Revision& rev, EdgeIndex index) : Edge(&rev, index) {}

// Checked downcast from Edge to UpEdge.
UpEdge::UpEdge(Edge edge) {
  assert(!edge.isDownEdge());
  static_cast<Edge&>(*this) = edge;
}

void UpEdge::assign(DownEdge d) {
  asRevision()->m_trace.assign(index(), d);
}

MemoValue::MemoValue(Type type) : m_type(type) {
  // Initialize all bits so that operator== can blindly compare them,
  // even for union members that don't use all the bits.
  memset(&m_value, 0, sizeof(m_value));
}

MemoValue::MemoValue(Context& ctx) : MemoValue(Type::kContext) {
  m_value.m_context = &ctx;
}

MemoValue::MemoValue(InvalidationWatcher& watcher)
    : MemoValue(Type::kInvalidationWatcher) {
  m_value.m_invalidationWatcher = &watcher;
}

MemoValue::MemoValue(IObj* iobj, Type type, bool incref) : MemoValue(type) {
  assert(
      type == Type::kIObj || type == Type::kException ||
      type == Type::kLongString);
  m_value.m_IObj = iobj;

  if (iobj != nullptr && incref) {
    skip::incref(iobj);
  }
}

MemoValue::MemoValue(intptr_t n, Type /*type*/) : MemoValue(Type::kFakePtr) {
  m_value.m_int64 = n;
}

MemoValue::MemoValue(long n) : MemoValue(Type::kInt64) {
  m_value.m_int64 = n;
}

MemoValue::MemoValue(long long n) : MemoValue(Type::kInt64) {
  m_value.m_int64 = n;
}

MemoValue::MemoValue(int n) : MemoValue(static_cast<int64_t>(n)) {}

MemoValue::MemoValue(double n) : MemoValue(Type::kDouble) {
  m_value.m_double = n;
}

MemoValue::MemoValue(const StringPtr& s) {
  if (auto longString = s->asLongString()) {
    m_value.m_IObj = &longString->cast<IObj>();
    m_type = Type::kLongString;
    incref(m_value.m_IObj);
  } else {
    m_value.m_int64 = s->sbits();
    m_type = Type::kShortString;
  }
}

MemoValue::MemoValue(StringPtr&& s) noexcept {
  if (auto longString = s->asLongString()) {
    m_value.m_IObj = &longString->cast<IObj>();
    m_type = Type::kLongString;
    s.release();
  } else {
    m_value.m_int64 = s->sbits();
    m_type = Type::kShortString;
  }
}

MemoValue::MemoValue(MemoValue&& v) noexcept : MemoValue() {
  *this = std::move(v);
}

MemoValue::MemoValue(const MemoValue& v) : MemoValue() {
  *this = v;
}

MemoValue& MemoValue::operator=(const MemoValue& v) {
  if (this != &v) {
    reset();
    m_type = v.m_type;
    m_value = v.m_value;

    if (auto iobj = asIObj()) {
      incref(iobj);
    }
  }

  return *this;
}

MemoValue::~MemoValue() {
  if (auto iobj = asIObj()) {
    safeDecref(*iobj);
  }
}

MemoValue& MemoValue::operator=(MemoValue&& v) noexcept {
  swap(v);
  return *this;
}

void MemoValue::reset() {
  *this = MemoValue();
}

void MemoValue::swap(MemoValue& v) noexcept {
  // We cannot use std::swap here because the field is packed.
  Value tmp = m_value;
  m_value = v.m_value;
  v.m_value = tmp;
  std::swap(m_type, v.m_type);
}

bool MemoValue::operator==(const MemoValue& v) const {
  // NOTE: We intentially use bitwise equality so that e.g. 0.0 != -0.0 and
  // NaN == NaN (if same bit pattern). This is different than the C++ ==
  // operator on doubles.
  return (m_type == v.m_type && !memcmp(&m_value, &v.m_value, sizeof(m_value)));
}

bool MemoValue::operator!=(const MemoValue& v) const {
  return !(*this == v);
}

bool MemoValue::isSkipValue() const {
  return m_type > Type::kContext;
}

Context* MemoValue::asContext() const {
  return (m_type == Type::kContext) ? m_value.m_context : nullptr;
}

IObj* MemoValue::asIObj() const {
  if (m_type == Type::kIObj || m_type == Type::kException ||
      m_type == Type::kLongString) {
    return m_value.m_IObj;
  } else {
    return nullptr;
  }
}

IObjOrFakePtr MemoValue::asIObjOrFakePtr() const {
  if (m_type == Type::kIObj || m_type == Type::kException ||
      m_type == Type::kLongString || m_type == Type::kNull) {
    return {m_value.m_IObj};
  } else if (m_type == Type::kFakePtr || m_type == Type::kShortString) {
    return IObjOrFakePtr((intptr_t)m_value.m_int64);
  } else {
    fatal("Unhandled type for asIObjOrFakePtr()");
  }
}

IObj* MemoValue::detachIObj() {
  auto iobj = asIObj();
  if (iobj != nullptr) {
    m_type = Type::kUndef;
    memset(&m_value, 0, sizeof(m_value));
  }
  return iobj;
}

InvalidationWatcher::Ptr MemoValue::detachInvalidationWatcher() {
  assert(m_type == Type::kInvalidationWatcher);
  InvalidationWatcher::Ptr watcher{m_value.m_invalidationWatcher, false};
  m_type = Type::kUndef;
  memset(&m_value, 0, sizeof(m_value));
  return watcher;
}

int64_t MemoValue::asInt64() const {
  assert(m_type == Type::kInt64);
  return m_value.m_int64;
}

double MemoValue::asDouble() const {
  assert(m_type == Type::kDouble);
  return m_value.m_double;
}

StringPtr MemoValue::asString() const {
  assert(isString());
  if (m_type == Type::kShortString) {
    return StringPtr(String::fromSBits(m_value.m_int64));
  } else {
    return StringPtr(String(m_value.m_IObj->cast<const LongString>()));
  }
}

MemoValue::Type MemoValue::type() const {
  return m_type;
}

OwnerAndFlags::OwnerAndFlags(Invocation* owner) {
  if (owner == nullptr) {
    m_bits.store(kCanRefreshFlag, std::memory_order_relaxed);
  } else {
    // This holds a strong pointer back to the owning Invocation.
    // Yes, that creates a circular refcount, but there is no memory leak
    // because any Invocation in the LRU or Cleanup list is kept alive
    // anyway, and when one falls off the end of LRU we purge its Revision
    // list, breaking the cycle.
    owner->incref();

    // Start out pointing to the Invocation with a count of 1.
    auto ownerBits = reinterpret_cast<uintptr_t>(owner);
    ownerBits <<= kFirstOwnerBitIndex;
    m_bits.store(
        ownerBits | kFirstCountBit | kIsAttachedFlag | kCanRefreshFlag,
        std::memory_order_relaxed);
  }
}

OwnerAndFlags::~OwnerAndFlags() {
  auto bits ATTR_UNUSED = currentBits();
  assert(extractOwner(bits) == nullptr && extractCount(bits) == 0);
}

InvocationPtr OwnerAndFlags::owner() const {
  for (auto bits = currentBits(); (bits & kIsAttachedFlag) != 0;) {
    assert(extractCount(bits) != 0);

    // This temporarily increfs and decrefs but we know there will
    // be no net effect.
    auto f = const_cast<OwnerAndFlags*>(this);

    // incref the owner pointer, to give us permission to dereference it,
    // but only if this object has not already become detached.
    auto newBits = bits + kFirstCountBit;
    if (f->m_bits.compare_exchange_weak(
            bits, newBits, std::memory_order_relaxed)) {
      InvocationPtr ret{extractOwner(newBits)};
      f->decrefRef(newBits);
      return ret;
    }
  }

  return InvocationPtr();
}

InvocationPtr OwnerAndFlags::owner_lck() const {
  auto bits = currentBits();
  // NOTE: Even if extractOwner() is non-null, we must return nullptr
  // if kAttachedFlag is not set, as logically there is no owner.
  if ((bits & kIsAttachedFlag) != 0) {
    return extractOwner(bits);
  } else {
    return InvocationPtr();
  }
}

bool OwnerAndFlags::canRefresh() const {
  return (currentBits() & kCanRefreshFlag) != 0;
}

void OwnerAndFlags::clearCanRefresh() {
  m_bits.fetch_and(~kCanRefreshFlag, std::memory_order_relaxed);
}

bool OwnerAndFlags::isAttached() const {
  return (currentBits() & kIsAttachedFlag) != 0;
}

void OwnerAndFlags::clearIsAttached() {
  m_bits.fetch_and(~kIsAttachedFlag, std::memory_order_relaxed);
}

void OwnerAndFlags::detach(bool canRefresh) {
  auto flagsToClear = kIsAttachedFlag | (canRefresh ? 0 : kCanRefreshFlag);
  decrefRef(currentBits(), flagsToClear);
}

void OwnerAndFlags::lock() {
  // If we still know the owner, lock it (and therefore its entire
  // Revision list). If we don't, just lock locally.
  //
  // Note that we may still use the owner's lock even after we are detached,
  // until the final "count" keeping the owner reference goes away.
  auto bits = currentBits();

  while (auto owner = extractOwner(bits)) {
    // Incref the owner pointer itself, so we know we can deref it safely.
    if (m_bits.compare_exchange_weak(
            bits, bits + kFirstCountBit, std::memory_order_relaxed)) {
      // Now that we know the owner pointer cannot become invalid, lock it.
      owner->mutex().lock();
      return;
    }
  }

  // No Invocation to delegate to, so use the local mutex instead.
  m_mutex.lock();
}

void OwnerAndFlags::unlock() {
  auto bits = currentBits();

  if (auto owner = extractOwner(bits)) {
    // No one should ever have locked the local lock.
    assert((bits & kLockMask) == 0);

    owner->mutex().unlock();

    decrefRef(bits);
  } else {
    // No Invocation to delegate to, so use the local mutex instead.
    m_mutex.unlock();
  }
}

uintptr_t OwnerAndFlags::currentBits() const {
  return m_bits.load(std::memory_order_relaxed);
}

/// Drop one reference to the m_owner field.
void OwnerAndFlags::decrefRef(uintptr_t bits, uintptr_t extraFlagsToClear) {
  while (true) {
    // No one else can clear the Invocation until we drop our reference.
    assert(extractOwner(bits) != nullptr);

    auto count = extractCount(bits);

    if (LIKELY(count > 1)) {
      // We are not the last reference, so merely decrement.
      if (m_bits.compare_exchange_weak(
              bits,
              (bits - kFirstCountBit) & ~extraFlagsToClear,
              std::memory_order_relaxed)) {
        break;
      }
    } else {
      assert(count != 0);

      auto newBits = (bits & (kFirstCountBit - 1)) & ~extraFlagsToClear;

      // We are dropping the owner pointer, so we had better not be attached.
      assert((newBits & kIsAttachedFlag) == 0);

      // Clear count and owner, but leave the other fields alone
      // (except as requested by extraFlagsToClear).
      if (m_bits.compare_exchange_weak(
              bits, newBits, std::memory_order_relaxed)) {
        // Whoever finally nulls out the owner is required to decref it.
        extractOwner(bits)->decref();
        break;
      }
    }
  }
}

size_t OwnerAndFlags::extractCount(uintptr_t bits) {
  constexpr auto numCountBits = kFirstOwnerBitIndex - kFirstCountBitIndex;
  constexpr auto countMask = (uintptr_t{1} << numCountBits) - 1;
  return (bits >> kFirstCountBitIndex) & countMask;
}

/// Extract the "m_owner" bitfield.
Invocation* OwnerAndFlags::extractOwner(uintptr_t bits) {
  return reinterpret_cast<Invocation*>(bits >> kFirstOwnerBitIndex);
}

const void* OwnerAndFlags::lockManagerKey() const {
  // This is only called while the owner ref is locked, so this is safe.
  const void* p = extractOwner(currentBits());
  return p ? p : this;
}

RevisionLock OwnerAndFlags::convertLock(Revision& rev, InvocationLock&& lock) {
  // Increment the refcount, just as if we had locked this Revision originally.
  auto bits ATTR_UNUSED =
      m_bits.fetch_add(kFirstCountBit, std::memory_order_relaxed);
  assert(extractOwner(bits) != nullptr);
  return RevisionLock{rev, std::move(lock)};
}

Revision::Revision(
    TxnId begin,
    TxnId end,
    Revision* prev,
    Revision* next,
    MemoValue value,
    Invocation* owner,
    Refcount refcount)
    : m_ownerAndFlags(owner), m_refcount(refcount), m_value(std::move(value)) {
  m_end = end;
  m_begin = begin;
  m_prev = prev;
  m_next = next;
  m_refresher = nullptr;
}

Revision::~Revision() {
  assert(!isPlaceholder());
}

void Revision::verifyInvariants() const {
  auto lock = lockify(*this);
  verifyInvariants_lck();
}

void Revision::verifyInvariants_lck() const {
  assertLocked(*this);

  assert(reinterpret_cast<uintptr_t>(this) % kRevisionAlign == 0);

  if (isAttached_lck()) {
    assert(owner_lck() != nullptr);
  } else {
    assert(m_prev.ptr() == nullptr);
    assert(m_next.ptr() == nullptr);
    // TODO might be too strict - mid-detach situation
    assert(owner_lck() == nullptr);
  }

  // A begin() of zero is reserved for "permanently active" values.
  if (begin_lck() == 0) {
    assert(end_lck() == kNeverTxnId);
    assert(!hasTrace_lck());
    assert(m_subs.obviouslyEmpty() || m_subs.begin() == m_subs.end());
  } else {
    assert(end_lck() > begin_lck());
  }

  m_subs.verifyInvariants(*this);
  m_trace.verifyInvariants(*this);
}

void Revision::invalidate(EdgeIndex index) {
  auto lock = lockify(*this);

  if (UNLIKELY(m_value.type() == MemoValue::Type::kInvalidationWatcher)) {
    // Special the case InvalidationWatcher::m_revision sentinel.
    if (InvalidationWatcher::Ptr watcher = detachInvalidationWatcher_lck()) {
      lockManager().invalidationWatchersToNotify().push_back(
          std::move(watcher));
    }
    return;
  }

  if (index >= m_trace.size()) {
    // Apparently this was a racy notification and we have either thrown
    // away our trace or replaced it with a different one. Either way,
    // we don't care about the notification.
    DEBUG_TRACE(
        "invalidate(" << RevDetails(this) << ") with bogus index ("
                      << (int)index << " >= " << (int)m_trace.size() << ')');
    return;
  }

  // NOTE: There is a possible race where this is an old invalidation
  // for a Trace that we already discarded and recreated differently during
  // a refresh. But that's fine, there is absolutely no harm in double-checking
  // an input to see if it looks OK.

  DownEdge d = m_trace[index];
  Revision* input = d.target();
  auto inputLock = lockify(input);

  auto newest = newestVisibleTxn();

  // Revisit: is it possible for canRefresh() to be false here,
  // since we're guaranteed to have a trace?
  // TODO if this turns out to be always true, modify guard below
  const bool couldRefreshBeforeInvalidating = canRefresh();

  const TxnId inputEnd = input->end_lck();
  const TxnId myEnd = end_lck();

  const bool truncated = (inputEnd < myEnd);

  DEBUG_TRACE(
      "invalidate(index=" << (int)index << " for " << RevDetails(this, false)
                          << ") from " << myEnd << " to " << inputEnd);

  if (truncated) {
    if (myEnd != kNeverTxnId) {
      std::cerr << "Internal error: truncating from " << myEnd << " to "
                << inputEnd << "; newest=" << newest << std::endl;
    }

    assert(myEnd == kNeverTxnId); // must have been infinite
    setEnd_lck(inputEnd, __LINE__); // truncate
  } else if (inputEnd == kNeverTxnId) {
    // It must have been refreshed before we got here (the race described
    // above), so "never mind" (don't invalidate).
    return;
  }

  const bool inputCanRefresh = input->canRefresh();
  inputLock.unlock();

  if (!inputCanRefresh) {
    // Once some inactive input can no longer refresh, then our trace
    // cannot be refreshed, so we might as well discard it.
    clearTrace_lck();
  } else {
    m_trace.setInactive(index, *this, __LINE__);
  }

  // Recursively invalidate subscribers, but only if not implicitly already
  // done above by a transition of canRefresh in recomputeCanRefresh_lck().
  // TODO remove "== couldRefreshBeforeInvalidating" (probably)
  if (truncated && canRefresh() == couldRefreshBeforeInvalidating) {
    m_subs.invalidateSubscribers_lck(*this);
  }

  DEBUG_TRACE("invalidate done for " << RevDetails(this, false));
}

bool Revision::canRefresh() const {
  return m_ownerAndFlags.canRefresh();
}

void Revision::recomputeCanRefresh_lck(bool allowRefresh) {
  assertLocked(*this);

  // Once we ever decide we can't refresh, we never can again.
  //
  // There are two ways to refresh:
  //
  // 1) refresh every entry of the incoming trace (preferred). If all the
  //    inputs to a function are the same, then the function returns the
  //    same value.
  //
  // 2) evaluate the invocation again and see if the value is unchanged.
  //    This requires knowing both the invocation and the old value.
  //    Knowing the invocation implies knowing the value.
  if (canRefresh() &&
      (!allowRefresh ||
       (end_lck() < kNeverTxnId && m_trace.empty() &&
        value_lck().type() == MemoValue::Type::kUndef))) {
    m_ownerAndFlags.clearCanRefresh();
    m_subs.invalidateSubscribers_lck(*this);
  }
}

void Revision::preventRefresh_lck() {
  clearTrace_lck();
  recomputeCanRefresh_lck(false);
}

void Revision::decref() {
  safeDecref(*this);
}

void Revision::incref() {
  m_refcount.fetch_add(1, std::memory_order_relaxed);
}

void Revision::subscribe_lck(UpEdge subscriber) {
  assertLocked(*this);

  Revision* parent = subscriber.target();
  assertLocked(*parent);

  m_subs.subscribe(*this, subscriber);

  auto myEnd = end_lck();
  if (myEnd != kNeverTxnId) {
    parent->m_trace.setInactive(subscriber.index(), *this, __LINE__);
    parent->setEnd_lck(std::min(parent->end_lck(), myEnd), __LINE__);
  }
}

void Revision::unsubscribe(DownEdge edge) {
  auto lock = lockify(*this);
  m_subs.unsubscribe(edge);
}

void Revision::asyncRefresh_lck(Caller& caller, RevisionLock thisLock) {
  assertLocked(*this);

  assert(caller.m_queryTxn >= begin_lck());

  if (caller.m_queryTxn < end_lck()) {
    // This lifespan is already acceptable, just tell our caller now.
    CallerList(caller).notifyCallers_lck(*this, std::move(thisLock));
  } else if (Refresher* existingRefresher = refresher_lck()) {
    existingRefresher->m_callers.addCaller_lck(caller);
  } else {
    auto newRefresher = new Refresher(caller, *this);
    m_refresher = newRefresher;
    newRefresher->continueRefresh_lck(0, std::move(thisLock));
  }
}

// Private decref called by ThreadLocalLockManager when no locks are held.
void Revision::decrefAssumingNoLocksHeld() {
  auto refcountPlusOne = m_refcount.fetch_sub(1, std::memory_order_acq_rel);

  assert(refcountPlusOne > 0);

  if (refcountPlusOne == 1) {
    {
      auto lock = lockify(*this);

      // These should have been cleared already.
      assert(!isAttached_lck());
      assert(m_prev == nullptr);
      assert(m_next == nullptr);
      assert(m_refresher == nullptr);

      // There must be no subscriptions to this, or the refcount
      // would not be zero.
      assert(m_subs.begin() == m_subs.end());

      // It is especially important to clear the trace now, to unsubscribe
      // anyone that might be about to invalidate() this object.
      // Otherwise that invalidate() might follow a dangling reference.
      clearTrace_lck();

      // We could do these in the destructor, but feels tidier to do now.
      m_subs.clear();
      resetMemoizedValue_lck();
    }

    // Verify no crazy resurrection happened.
    assert(currentRefcount() == 0);

    delete this;
  }
}

InvalidationWatcher::Ptr Revision::detachInvalidationWatcher_lck() {
  if (m_value.type() == MemoValue::Type::kInvalidationWatcher) {
    clearTrace_lck();
    return m_value.detachInvalidationWatcher();
  } else {
    return InvalidationWatcher::Ptr();
  }
}

void Revision::createInvalidationWatcherTrace(Revision& lockedInput) {
  assertLocked(lockedInput);

  // This lock would normally be considered out-of-order, since the child
  // is already locked and now we are locking the parent. However, we know
  // that the parent is the special Revision in an InvalidationWatcher,
  // not yet connected to the graph, so there is no problem.
  auto lock = lockify(*this);

  if (m_value.type() == MemoValue::Type::kInvalidationWatcher) {
    // We have not yet been detached.
    if (!lockedInput.isPure_lck()) {
      // The value could someday be invalidated, so go ahead and subscribe.
      lockedInput.incref();
      m_trace = Trace(1);
      lockedInput.subscribe_lck(UpEdge(*this, 0));
    } else {
      // We can never be invalidated, so drop our reference to the watcher.
      detachInvalidationWatcher_lck();
    }
  }
}

void Revision::createTrace_lck(Revision::Ptr* inputs, size_t size) {
  assertLocked(*this);

  assert(size > 0);

  bool canRefreshTrace = true;
  TxnId begin = 1;
  TxnId end = kNeverTxnId;

  // Allocate a new Trace object.
  const size_t traceSize = std::min<size_t>(size, kMaxTraceSize);
  assert(m_trace.empty());
  m_trace = Trace(traceSize);

  // We need to recursively create a tree with fake Revisions
  // to hold this trace.
  for (size_t index = 0;; ++index) {
    // We have this many slots remaining in 'trace'.
    const size_t toplevel = traceSize - index;

    if (size <= toplevel) {
      // Everything else just fits directly in 'trace', no more children.
      assert(size == toplevel);

      for (size_t start = index; index < traceSize; ++index) {
        auto inputPtr = std::move(inputs[index - start]);
        auto inputLock = lockify(*inputPtr);

        inputPtr->subscribe_lck(UpEdge(*this, (EdgeIndex)index));

        // m_trace now owns the refcount, so drop it here.
        auto input = boost_detach(inputPtr);

        // Now that we are subscribed, get the current begin/end.
        // If the end becomes finite we will be invalidated.
        begin = std::max(begin, input->begin_lck());
        end = std::min(end, input->end_lck());
        canRefreshTrace &= input->canRefresh();
      }

      break;
    }

    // Figure out the max number of leaves any child could need.
    size_t maxChild = 1;
    for (size_t n = toplevel; n < size; n *= kMaxTraceSize) {
      maxChild *= kMaxTraceSize;
    }

    // After the child consumes some of the inputs, we'll have this many
    // slots we can use "for free" in 'trace'. We don't want the child
    // to consume those or 'trace' won't be fully populated.
    size_t numToplevelAfterChild = toplevel - 1;
    size_t childSize = std::min(maxChild, size - numToplevelAfterChild);

    // Make a dummy Revision just to handle 'trace' fanout.
    auto child = new Revision(1, kNeverTxnId, nullptr, nullptr);

    auto lock = lockify(child);
    child->createTrace_lck(inputs, childSize);
    child->subscribe_lck(UpEdge(*this, (EdgeIndex)index));

    begin = std::max(begin, child->begin_lck());
    end = std::min(end, child->end_lck());
    canRefreshTrace &= child->canRefresh();

    inputs += childSize;
    size -= childSize;
  }

  setBegin_lck(begin);
  setEnd_lck(end, __LINE__);

  if (!canRefreshTrace) {
    DEBUG_TRACE("Abandoning non-refreshable trace for " << RevDetails(this));

    assert(end < kNeverTxnId);
    m_trace.clear();
  }

  DEBUG_TRACE("Created trace for " << RevDetails(this) << ": " << m_trace);

  recomputeCanRefresh_lck();
}

void Revision::stealTrace_lck(Revision& other) {
  assertLocked(*this);

  DEBUG_TRACE("Stealing trace from " << other << " to add to " << this);

  // Once we say we can't refresh, we never can again, so we really
  // shouldn't be taking on another trace.
  assert(canRefresh());

  if (this == &other) {
    return;
  }

  clearTrace_lck();

  if (!other.hasTrace_lck()) {
    setBegin_lck(other.begin_lck());
    setEnd_lck(other.end_lck(), __LINE__);
  } else {
    // Record what Revisions the trace points to.
    const size_t size = other.m_trace.size();
    std::vector<Revision::Ptr> traceVec(size);
    for (size_t i = 0; i < size; ++i) {
      traceVec[i].reset(other.m_trace[i].target());
    }

    // Throw away the trace. This is important to do now, because the
    // case of one subscriber is optimized, and we don't want to unnecessarily
    // cause inputs to temporarily have two subscribers and lose that
    // optimization.
    other.clearTrace_lck();

    createTrace_lck(traceVec.data(), traceVec.size());
  }
}

OwnerAndFlags& Revision::mutex() const {
  return const_cast<OwnerAndFlags&>(m_ownerAndFlags);
}

RevisionLock Revision::convertLock(InvocationLock&& lock) {
  return m_ownerAndFlags.convertLock(*this, std::move(lock));
}

void Revision::resetMemoizedValue_lck() {
  assertLocked(*this);
  if (!isPlaceholder()) {
    m_value.reset();
    recomputeCanRefresh_lck();
  }
}

/// Pure values have no external dependencies and so can never become
/// invalid. They do not get added to the dependency graph.
bool Revision::isPure_lck() const {
  return (begin_lck() == 0);
}

void Revision::clearTrace_lck() {
  assertLocked(*this);
  DEBUG_TRACE(
      "Clearing trace for " << RevDetails(this, false) << "; size was "
                            << (int)m_trace.size());

  m_trace.clear();
  recomputeCanRefresh_lck();
}

bool Revision::hasTrace_lck() const {
  assertLocked(*this);
  return !m_trace.empty();
}

Refresher* Revision::refresher_lck() const {
  assertLocked(*this);
  return m_refresher.ptr();
}

/// Readable by either the holder of mutex().
Invocation::Ptr Revision::owner_lck() const {
  assertLocked(*this);
  return m_ownerAndFlags.owner_lck();
}

bool Revision::isPlaceholder() const {
  // Even though m_value can change, this is safe to examine with a lock
  // because we assume reads of m_value.m_type (a byte) are atomic,
  // and this value never transitions to or from kContext.
  return m_value.type() == MemoValue::Type::kContext;
}

Context* Revision::valueAsContext() const {
  // We do not need a lock here, since we treat the implied isPlaceholder()
  // check as atomic, and this return value never changes for a given Revision.
  return m_value.asContext();
}

TxnId Revision::end_lck() const {
  assertLocked(*this);
  return m_end;
}

void Revision::setEnd_lck(TxnId newEnd, int line ATTR_UNUSED) {
  assert(newEnd <= kNeverTxnId);

  assertLocked(*this);

  DEBUG_TRACE(
      "Setting end from " << end_lck() << " to " << newEnd << " for "
                          << RevDetails(this, false) << " because of line "
                          << line << " (newestVisible = " << newestVisibleTxn()
                          << ")");

  m_end = newEnd;
  recomputeCanRefresh_lck();
}

TxnId Revision::begin_lck() const {
  assertLocked(*this);
  return m_begin;
}

void Revision::setBegin_lck(TxnId begin) {
  assertLocked(*this);
  m_begin = begin;
}

const MemoValue& Revision::value_lck() const {
  assertLocked(*this);
  return m_value;
}

void Revision::setValue_lck(const MemoValue& value) {
  assertLocked(*this);
  m_value = value;
}

void Revision::setValue_lck(MemoValue&& value) {
  assertLocked(*this);
  m_value = std::move(value);
}

// TODO: Do we really need the lock here?
bool Revision::isAttached_lck() const {
  assertLocked(*this);
  return m_ownerAndFlags.isAttached();
}

Refcount Revision::currentRefcount() const {
  return m_refcount.load(std::memory_order_relaxed);
}

static std::shared_mutex s_cleanupListsMutex;

// Protected by cleanupsMutex().
static std::map<TxnId, CleanupList> s_cleanupLists;

/**
 * Returns a CleanupList for the requested transaction, as well as a
 * read-lock on the set of all CleanupLists that is currently preventing
 * it from being processed or freed. As soon as the caller releases this
 * lock, the CleanupList could get freed unless it has a m_numActiveMemoTasks
 * counter greater than zero.
 *
 * If txn is zero, this replaces it with the newest TxnId visible to
 * reader. In any case the txn actually selected in returned as the second
 * tuple member.
 */
static std::tuple<CleanupList*, TxnId, std::unique_lock<std::shared_mutex>>
findOrCreateLockedCleanupList(TxnId txn) {
  std::unique_lock lock(s_cleanupListsMutex);

  // If the user asked for TxnId zero, it means they want the latest one.
  // We can only properly compute that here after taking the lock.
  TxnId queryTxn = txn ? txn : newestVisibleTxn();

  // First, try to find the cleanup list using only a read lock.
  CleanupList* cl;
  auto it = s_cleanupLists.find(queryTxn);
  if (LIKELY(it != s_cleanupLists.end())) {
    cl = &it->second;
  } else {
    // TODO: figure out if we still need this.
    queryTxn = txn ? txn : newestVisibleTxn();

    // Create this if it doesn't exist, or reuse it if it does.
    cl = &s_cleanupLists[queryTxn];
  }

  return std::make_tuple(cl, queryTxn, std::move(lock));
}

static void registerCleanup(Invocation& inv, TxnId txn) {
  assertLocked(inv);

  DEBUG_TRACE(
      "registerCleanup(" << inv << ", " << txn
                         << "); list = " << (int)inv.m_owningList);

  assert(txn != 0 && txn != kNeverTxnId);

  Invocation::Ptr invPtr;

  switch (inv.m_owningList) {
    case OwningList::kCleanup:
      // While 'inv' is in a cleanup list, it never moves to a different one,
      // even if technically it should because the other CleanupList will
      // be processed sooner.
      //
      // The reason is that supporting random access removal would require
      // more locking and doubly-linked list manipulation, rather than just
      // the atomic stack push we do now.
      //
      // The theory is that 'inv' being in the optimal CleanupList doesn't
      // much matter, because as long as it is in some cleanup list, its
      // memory will get reclaimed eventually.
      return;
    case OwningList::kLru:
      // Transfer existing refcount from LRU list to cleanup.
      s_lruList.erase(inv, false);
      boost_reset(invPtr, &inv, false);
      break;
    case OwningList::kNone:
      invPtr.reset(&inv);
      break;
  }

  // NOTE: If a caller somehow registers a cleanup for a txn list that's older
  // than should be possible, that's OK, we'll just create the really old
  // cleanup list they asked for and it will get cleaned up the next time
  // runCleanupsSoon() is called.

  std::get<0>(findOrCreateLockedCleanupList(txn))->push(std::move(invPtr));
}

/** Helper function run runCleanupsSoon. Do not call directly. */
static void runReadyCleanups() {
  DEBUG_TRACE("Entering runReadyCleanups()");

  Invocation* head = nullptr;
  Invocation* tail = nullptr;

  // Quickly delete any old CleanupLists and chain their members into head+tail.
  for (std::unique_lock lock(s_cleanupListsMutex);;) {
    auto it = s_cleanupLists.begin();

    auto newestVisible = newestVisibleTxn();
    if (it == s_cleanupLists.end() || it->first > newestVisible) {
      s_oldestVisibleTxn.store(newestVisible, std::memory_order_relaxed);
      break;
    }

    TxnId cleanupTxn = it->first;
    CleanupList& cl = it->second;

    if (cl.m_numActiveMemoTasks.load(std::memory_order_acquire) > 0) {
      // MemoTasks are still running at this TxnId, so it is still "visible".
      s_oldestVisibleTxn.store(cleanupTxn, std::memory_order_relaxed);
      break;
    }

    DEBUG_TRACE(
        "Concatenating to cleanups list for txn " << cleanupTxn << ": " << cl);

    cl.clearAndConcatTo_lck(head, tail);
    s_cleanupLists.erase(it);
  }

  // Only run these once we have released the lock above.
  for (Invocation *inv = head, *next; inv != nullptr; inv = next) {
    next = inv->m_lruNext.ptr();
    inv->cleanup();
    inv->decref();

    // TODO: We could collect all the Invocations that want to enter
    // the LRU list here in a big list, and then drop them all at once
    // with only a single LRU lock grab. It's a bit tricky though since
    // we won't be holding the individual object locks at that point.
    // It's legal to modify m_lruPrev and m_lruNext, but m_owningList is
    // technically a bit of a problem. But moving it from kCleanup to kLru
    // should actually be OK, if we only do it after taking s_lruMutex.
  }

  DEBUG_TRACE("Leaving runReadyCleanups()");
}

/**
 * Run any cleanups that are ready to go. Run them either now or "soon"
 * (i.e. they are not guaranteed to have run when this returns.
 */
static void runCleanupsSoon() {
  // Upper bound on the number of CleanupLists that might be ready to
  // have their cleanup() method called.
  alignas(kCacheLineSize) static std::atomic<uint32_t> s_cleanupsReady{0};

  // We only allow one thread to run cleanups at a time, although that's
  // not strictly necessary. Only the thread that increments from zero
  // to nonzero runs the cleanups, and keeps doing running them until
  // no other thread increments the count while it was running cleanups.
  if (s_cleanupsReady.fetch_add(1, std::memory_order_release) == 0) {
    // TODO: Consider posting running this loop ta a worker thread, at least
    // if it runs more than once. Don't want to starve the caller. The good
    // news is that the caller of runCleanupsSoon() just finished a task so
    // hopefully doesn't have anything urgent to do.
    for (uint32_t ready = 1; ready != 0; ready = (s_cleanupsReady -= ready)) {
      runReadyCleanups();
    }
  }
}

void assertNoCleanups() {
  bool foundNonEmptyCleanupList = false;

  {
    std::shared_lock lock(s_cleanupListsMutex);

    for (auto& vp : s_cleanupLists) {
      std::cerr << "Found non-empty cleanup list for txn " << vp.first << '\n';
      std::cerr << "newest visible: " << newestVisibleTxn() << '\n';
      std::cerr << "oldest visible: " << oldestVisibleTxn() << std::endl;
      foundNonEmptyCleanupList = true;
    }
  }

  if (foundNonEmptyCleanupList) {
    runReadyCleanups();

    std::shared_lock lock(s_cleanupListsMutex);
    std::cerr << "Re-ran cleanups, found " << s_cleanupLists.size() << '\n';
  }

  assert(!foundNonEmptyCleanupList);
}

Context::Guard::Guard(Context* newContext)
    : m_old(Context::setCurrent(newContext)) {}

Context::Guard::~Guard() {
  Context::setCurrent(m_old);
}

Context::Context(Invocation& owner, Caller& firstCaller, TxnId begin, TxnId end)
    : m_queryTxn(begin),
      m_owner(&owner),
      m_callers(firstCaller),
      m_placeholder(begin, end, nullptr, nullptr, MemoValue(*this), &owner, 2) {
  // NOTE: The placeholder starts with a refcount of 2, 1 for the Context
  // and 1 for its imminent presence in the Invocation Revision list.

  m_mutex.init();

  // Guarantee the list starts out terminated.
  firstCaller.m_next = nullptr;
}

Context::Context(TxnId begin)
    : m_queryTxn(begin),
      m_owner(),
      m_callers(),
      m_placeholder(begin, kNeverTxnId, nullptr, nullptr, MemoValue(*this)) {
  m_mutex.init();
}

Context::~Context() {
  // No one should ever be taking a reference to a placeholder Revision.
  assert(m_placeholder.currentRefcount() == 1);
  assert(m_placeholder.m_value == MemoValue(*this));
  assert(m_placeholder.m_prev == nullptr);
  assert(m_placeholder.m_next == nullptr);

  // Make us no longer a placeholder so our nested Revision's constructor
  // is happy being destroyed -- we don't want its destructor called
  // independently from this outer, containing object's.
  m_placeholder.m_value.reset();

  discardCalls();
}

Context* Context::current() {
  return Process::cur()->m_ctx;
}

Context* Context::setCurrent(Context* ctx) {
  std::swap(ctx, Process::cur()->m_ctx);
  return ctx;
}

void Context::evaluateDone(MemoValue&& v) {
  auto result = m_owner->replacePlaceholder(*this, std::move(v));

  // Grab the current list of callers before destroying this Context.
  CallerList callers(std::move(m_callers));

  delete this;

  // Tell everyone the answer.
  callers.notifyCallers_lck(*result.first, std::move(result.second));
}

void Context::discardCalls() {
  CallSet calls;
  calls.swap(m_calls);
  for (auto vp : calls) {
    vp.first->decref();
  }
}

void Context::addCaller_lck(Caller& caller) {
  // TODO: Detect infinite recursion here.
  m_callers.addCaller_lck(caller);
}

// ??? lockedInput does not *really* need to be locked...
void Context::addDependency(Revision& lockedInput) {
  assertLocked(lockedInput);

  if (!lockedInput.isPure_lck()) {
    try {
      // No need for the 'lockify' overhead here.
      m_mutex.lock();

      bool freshlyInserted =
          m_calls.emplace(&lockedInput, m_calls.size()).second;
      if (freshlyInserted) {
        lockedInput.incref();
      }
      m_mutex.unlock();
    } catch (const std::exception& ex) {
      m_mutex.unlock();
      throw(ex);
    }
  }
}

// Turn the scrambled m_calls unordered_map into a linear array listing the
// inputs in the order in which they were first visited.
//
// This transfers the reference counts from m_calls to the returned array,
// so the caller becomes responsible for decrefing.
std::vector<Revision::Ptr> Context::linearizeTrace() {
  std::vector<Revision::Ptr> v(m_calls.size());

  for (auto kv : m_calls) {
    assert(kv.second < v.size());
    assert(!v[kv.second]);
    v[kv.second] = Revision::Ptr(kv.first, false);
  }

  // We stole the refcounts, so we need to purge m_calls.
  CallSet().swap(m_calls);
  return v;
}

SpinLock& Context::mutex() const {
  return m_mutex;
}

MemoTask::MemoTask(TxnId queryTxn, CleanupList& cleanupList)
    : m_queryTxn(queryTxn), m_cleanupList(cleanupList) {}

MemoTask::~MemoTask() {
  m_cleanupList.taskFinished();
}

std::vector<MemoTask::Ptr> createMemoTasks(size_t count) {
  std::vector<MemoTask::Ptr> tasks(count);

  if (LIKELY(count != 0)) {
    CleanupList* cl;
    TxnId queryTxn;

    {
      // NOTE: This tuple has a read lock keeping the CleanupList alive.
      // We introduce this scope to release it before we start creating tasks.
      const auto& cleanupInfo = findOrCreateLockedCleanupList(0);

      // Grab whatever txn findOrCreateLockedCleanupList chose.
      queryTxn = std::get<1>(cleanupInfo);

      // Incref the CleanupList so it won't get freed once cleanupInfo
      // goes away (which drops the read lock).
      cl = std::get<0>(cleanupInfo);
      cl->m_numActiveMemoTasks.fetch_add(count, std::memory_order_relaxed);
    }

    for (auto& t : tasks) {
      t = std::make_shared<MemoTask>(queryTxn, *cl);
    }
  }

  return tasks;
}

MemoTask::Ptr createMemoTask() {
  return std::move(createMemoTasks(1)[0]);
}

static std::mutex s_txnMutex;

Transaction::Transaction() = default;

Transaction::~Transaction() {
  if (!std::uncaught_exception()) {
    commit();
  }
}

Transaction::Transaction(Transaction&& other) noexcept : Transaction() {
  *this = std::move(other);
}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
  m_commits = std::move(other.m_commits);
  other.m_commits.clear();
  return *this;
}

void Transaction::assignMemoValue(Cell& cell, MemoValue&& value) {
  m_commits.emplace_back(cell.invocation(), std::move(value));
}

void Transaction::assignMemoValue(Invocation& inv, MemoValue&& value) {
  m_commits.emplace_back(&inv, std::move(value));
}

void Transaction::abort() {
  m_commits.clear();
}

std::unique_lock<std::mutex> Transaction::commitWithoutUnlock(
    std::vector<InvalidationWatcher::Ptr>& invalidationWatchersToNotify) {
  skip::fast_set<Invocation*> noOpAssigned;
  bool changed = false;

  std::unique_lock<std::mutex> commitLock{s_txnMutex};
  const TxnId begin = newestVisibleTxn() + 1;

  for (auto it = m_commits.rbegin(); it != m_commits.rend(); ++it) {
    auto inv = it->first.get();

    if (noOpAssigned.find(inv) != noOpAssigned.end()) {
      // Already assigned with an unchanged value, which should "win".
      continue;
    }

    auto lock = lockify(*inv);

    // Truncate the head so it no longer extends past begin.
    auto head = inv->m_headValue.ptr();
    if (head != nullptr) {
      assert(head->end_lck() <= begin || head->end_lck() == kNeverTxnId);
      assert(!head->isPure_lck());
    }

#if ENABLE_DEBUG_TRACE
    if (head) {
      DEBUG_TRACE(
          "Invalidating " << inv << " at txn " << begin << "; subscribers are "
                          << head->m_subs);
    } else {
      DEBUG_TRACE("Invalidating " << inv << " at txn " << begin);
    }
#endif

    auto& value = it->second;

    // You can only change a value which extends to kNeverTxnId.  Once a
    // Revision has a real end then it's never allowed to change again.
    if (head && head->end_lck() == kNeverTxnId) {
      if (head->value_lck() == value) {
        // Ignore assigning the same value.
        noOpAssigned.insert(inv);
        continue;

      } else if (head->begin_lck() == begin) {
        // Already assigned in this txn.
        continue;

      } else {
        // Not assigned yet in this txn.
        changed = true;
        head->setEnd_lck(begin, __LINE__);
        head->preventRefresh_lck();
        head->m_subs.invalidateSubscribers_lck(*head);
      }
    }

    // If the commit provided a value then insert the value into the
    // Invocation
    if (value.isSkipValue()) {
      // Prepend a new value to the list.
      auto rev = new Revision(begin, kNeverTxnId, nullptr, head, value, inv);
      inv->insertBefore_lck(*rev, head);

#if ENABLE_DEBUG_TRACE
      {
        DEBUG_TRACE(
            "Assigning " << rev->value_lck() << " to " << inv << " (now "
                         << RevDetails(rev) << ") at txn " << begin);
      }
#endif

      changed = true;
    }

    // Ensure this is in the cleanup list (it may be already).
    registerCleanup(*inv, begin);
  }

  if (changed) {
    std::unique_lock lock(s_cleanupListsMutex);

    // Steal the vector of watchers to notify, so we can notify them below.
    invalidationWatchersToNotify.swap(
        lockManager().invalidationWatchersToNotify());

    assert(newestVisibleTxn() + 1 == begin);
    s_newestVisibleTxn = begin;

    if (s_cleanupLists.empty()) {
      s_oldestVisibleTxn = begin;
    }
  }

  m_commits.clear();

  return commitLock;
}

void Transaction::commit() {
  std::vector<InvalidationWatcher::Ptr> invalidationWatchersToNotify;

  { auto commitLock = commitWithoutUnlock(invalidationWatchersToNotify); }

  // NOTE: We can only notify invalidation watchers here, after everything
  // is fully committed and unlocked, in case any handlers turn right back
  // around and try to get fresh post-commit values.
  for (auto& invref : invalidationWatchersToNotify) {
    InvalidationWatcher::Ptr watcher(std::move(invref));
    watcher->invalidate();
  }
}

namespace {
const VTableRef static_cellInvocationVTable() {
  static auto singleton = RuntimeVTable::factory(static_cellInvocationType());
  static VTableRef vtable{singleton->vtable()};
  return vtable;
}
} // namespace

Cell::Cell(const MemoValue& initialValue)
    // The basic Invocation has no parameters so we need to allocate a little
    // extra to ensure that when we ask the Arena about a pointer 'this' is
    // actually within the object.
    : m_invocation(
          new (Arena::calloc(
              sizeof(Invocation) + sizeof(void*),
              Arena::Kind::iobj)) Invocation(static_cellInvocationVTable()),
          false) {
  auto rev = new Revision(
      1, kNeverTxnId, nullptr, nullptr, initialValue, m_invocation.get());
  m_invocation->m_headValue = rev;
  m_invocation->m_tailValue = rev;
}

Cell::~Cell() {
  // We need to clear the Revisions list to remove any circular references.
  auto lock = lockify(*m_invocation);
  m_invocation->detachRevisions_lck();
}

Invocation::Ptr Cell::invocation() const {
  return m_invocation;
}

SpinLock& Cell::mutex() const {
  return m_invocation->mutex();
}

void intrusive_ptr_add_ref(Revision* rev) {
  rev->incref();
}

void intrusive_ptr_release(Revision* rev) {
  rev->decref();
}

void intrusive_ptr_add_ref(InvalidationWatcher* watcher) {
  watcher->incref();
}

void intrusive_ptr_release(InvalidationWatcher* watcher) {
  watcher->decref();
}

void intrusive_ptr_add_ref(Invocation* inv) {
  inv->incref();
}

void intrusive_ptr_release(Invocation* inv) {
  inv->decref();
}

void intrusive_ptr_add_ref(IObj* iobj) {
  incref(iobj);
}

void intrusive_ptr_release(IObj* iobj) {
  decref(iobj);
}

bool g_oneThreadActive;

static void dumpRevision(
    std::ostream& out,
    const Revision* r,
    bool details,
    bool emitTrace,
    bool needLock = true) {
  if (r != nullptr) {
    out << "[Revision " << static_cast<const void*>(r);

    if (details) {
      RevisionLock lock;
      if (needLock && !lockManager().debugIsLocked(*r)) {
        lock = lockify(*r);
      }

      out << " [" << r->begin_lck() << ", ";

      if (r->end_lck() == kNeverTxnId) {
        out << "never";
      } else {
        out << r->end_lck();
      }
      out << ")";

      out << " -> " << r->value_lck();

      std::string flags;
      if (r->isAttached_lck()) {
        flags += 'a';
      }
      if (r->refresher_lck()) {
        flags += 'r';
      }
      if (r->m_subs.begin() != r->m_subs.end()) {
        flags += 's';
      }
      if (r->hasTrace_lck()) {
        flags += 't';
      }

      if (!flags.empty()) {
        out << " (+" << flags << ')';
      }

      if (emitTrace) {
        out << ' ' << r->m_trace;
      }

      out << ' ' << r->m_subs;
    }

    out << ']';
  } else {
    out << "[Revision NULL]";
  }
}

std::ostream& operator<<(std::ostream& out, const Revision* r) {
  dumpRevision(out, r, false, false);
  return out;
}

std::ostream& operator<<(std::ostream& out, const Revision& r) {
  return out << &r;
}

std::ostream& operator<<(std::ostream& out, const Revision::Ptr& r) {
  return out << r.get();
}

std::ostream& operator<<(std::ostream& out, RevDetails rev) {
  dumpRevision(out, rev.m_rev, true, rev.m_emitTrace);
  return out;
}

static void
dumpInvocation(std::ostream& out, const Invocation* inv, bool details) {
  if (inv != nullptr) {
    out << "[Invocation " << static_cast<const void*>(inv);
    if (details) {
      out << ' ';
      dumpRevisions(*inv, true, out);
    }
    out << ']';
  } else {
    out << "[Invocation NULL]";
  }
}

std::ostream& operator<<(std::ostream& out, const Invocation* inv) {
  dumpInvocation(out, inv, false);
  return out;
}

std::ostream& operator<<(std::ostream& out, const Invocation& inv) {
  return out << &inv;
}

std::ostream& operator<<(std::ostream& out, const Invocation::Ptr& inv) {
  return out << inv.get();
}

std::ostream& operator<<(std::ostream& out, InvDetails inv) {
  dumpInvocation(out, inv.m_inv, true);
  return out;
}

std::ostream& operator<<(std::ostream& out, const CleanupList& cl) {
  out << "[CleanupList";

  auto head = cl.m_head.load();

  bool first = true;
  for (Invocation* inv = head; inv != nullptr; inv = inv->m_lruNext.ptr()) {
    out << (first ? " " : ", ") << inv;
    first = false;
  }

  return out << ']';
}

std::ostream& operator<<(std::ostream& out, const Trace& trace) {
  auto believedInactive = trace.inactive();
  out << "[Trace inactive=0x" << std::hex << trace.inactive() << std::dec;

  uint64_t actuallyInactive = 0;
  for (EdgeIndex i = 0, size = trace.size(); i < size; ++i) {
    if (auto input = trace[i].target()) {
      auto inputLock = lockify(*input);
      actuallyInactive |= (uint64_t)(input->end_lck() < kNeverTxnId) << i;
    }
  }
  if (actuallyInactive & ~believedInactive) {
    out << " [BAD, should be 0x" << std::hex << actuallyInactive << std::dec
        << ']';
  }

  for (EdgeIndex i = 0, size = trace.size(); i < size; ++i) {
    auto input = trace[i].target();
    out << (i ? ", " : " ") << input;
  }

  return out << ']';
}

std::ostream& operator<<(std::ostream& out, const SubscriptionSet& subs) {
  out << "[Subs";

  bool first = true;
  for (auto& e : subs) {
    out << (first ? " " : ", ") << e.target();
    first = false;
  }

  return out << ']';
}

std::ostream& operator<<(std::ostream& out, const MemoValue& m) {
  switch (m.type()) {
    case MemoValue::Type::kUndef:
      out << "[undef]";
      break;

    case MemoValue::Type::kContext:
      out << "[context " << static_cast<const void*>(m.asContext()) << ']';
      break;

    case MemoValue::Type::kIObj:
      out << "[obj:" << static_cast<const void*>(m.asIObj()) << ']';
      break;

    case MemoValue::Type::kException:
      out << "[exception:" << static_cast<const void*>(m.asIObj()) << ']';
      break;

    case MemoValue::Type::kLongString:
    case MemoValue::Type::kShortString: {
      String::DataBuffer buf;
      out << "[string:" << m.asString()->slice(buf).begin() << ']';
      break;
    }

    case MemoValue::Type::kDouble: {
      boost::io::ios_precision_saver prec{out};
      out << std::setprecision(std::numeric_limits<double>::max_digits10);
      out << m.asDouble();
    } break;

    case MemoValue::Type::kInt64:
      out << m.asInt64();
      break;

    default:
      out << "[invalid MemoValue type:" << static_cast<int>(m.type()) << ']';
      break;
  }

  return out;
}

void dumpRevisions(const Invocation& inv, bool details, std::ostream& out) {
  // Lock inv if needed. Normally we would make the caller do it but this
  // gets called from gdb or other annoying debug situations where it's
  // not worth enforcing a locking discipline.
  InvocationLock lock;
  if (!lockManager().debugIsLocked(inv)) {
    lock = lockify(inv);
  }

  const char* sep = "";
  for (auto p = inv.m_headValue.ptr(); p != nullptr; p = p->m_next.ptr()) {
    out << sep;
    sep = ", ";

    dumpRevision(out, p, details, details, false);
  }
}

// Separate implementation to make it easier to call from a debugger.
void dumpRevisions(const Invocation& inv) {
  dumpRevisions(inv, true, std::cerr);
  std::cerr << '\n';
}

void InvocationHelperBase::static_evaluate_helper(
    std::future<MemoValue>&& future) {
  std::cerr << "Internal error: you have reached static_evaluate_helper\n";
  exit(70);
  // TODO: re-enable this.
  //
  // auto ctx = Context::current();
  //
  // std::move(future)
  //    .thenValue(
  //        [ctx](MemoValue&& value) { ctx->evaluateDone(std::move(value)); })
  //    .thenError(
  //        folly::tag_t<std::exception>{}, [ctx](const std::exception& e) {
  // To record an exception in the memoizer, we need to create an
  // interned Exception object. Currently the only way to produce one
  // is to allocate on the the obstack, then intern that.
  //
  // If we already have an Obstack (i.e. this thread has a Process),
  // just use that one, but politely pop the PosScope. If we don't,
  // create a throwaway Process and use its Obstack. That's pretty
  // wasteful but this just the error case, which is hopefully
  // uncommon.
  //
  // It might be better to intern from a non-obstack memory image
  // of this Exception, but there's no super-easy way to form one.

  //          auto report = [ctx, msg = std::string(e.what())]() {
  //            MemoValue excVal;
  //            {
  //              Obstack::PosScope obstackScope;
  //              auto& obstack = Obstack::cur();
  //              auto ex = SKIP_makeRuntimeError(String(msg));
  //              auto obj = obstack.intern(ex).asPtr();
  //              excVal = MemoValue(obj, MemoValue::Type::kException, true);
  //            }
  //            ctx->evaluateDone(std::move(excVal));
  //          };

  //          report();
  //        });
}

InvalidationWatcher::InvalidationWatcher(Refcount refcount)
    : m_refcount(refcount),
      m_revision(
          new Revision(
              0,
              kNeverTxnId,
              nullptr,
              nullptr,
              MemoValue(*this),
              nullptr,
              1),
          false) {}

InvalidationWatcher::~InvalidationWatcher() = default;

InvalidationWatcher::Ptr InvalidationWatcher::make(
    std::vector<Revision::Ptr>&& traceVec) {
  if (traceVec.empty()) {
    return InvalidationWatcher::Ptr();
  } else {
    auto watcher = make();
    bool invalidate;
    {
      auto rev = watcher->m_revision.get();
      auto lock = lockify(*rev);
      rev->createTrace_lck(traceVec.data(), traceVec.size());
      invalidate = rev->end_lck() != kNeverTxnId && watcher->unsubscribe_lck();
    }
    if (invalidate) {
      // By the time we finished running the code at least one of the
      // dependencies got invalidated, so note that now, since we
      // won't be seeing another notification.
      watcher->invalidate();
    }
    return watcher;
  }
}

void InvalidationWatcher::incref() {
  m_refcount.fetch_add(1, std::memory_order_relaxed);
}

void InvalidationWatcher::decref() {
  if (m_refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete this;
  }
}

void InvalidationWatcher::invalidate() {
#if SKIP_PARANOID
  // Only do this in paranoid mode since it requires taking a lock.
  assert(!isSubscribed());
#endif

  m_promise.set_value();
}

bool InvalidationWatcher::isSubscribed() {
  auto lock = lockify(*m_revision);
  return (
      m_revision->value_lck().type() == MemoValue::Type::kInvalidationWatcher);
}

bool InvalidationWatcher::unsubscribe_lck() {
  return static_cast<bool>(m_revision->detachInvalidationWatcher_lck());
}

bool InvalidationWatcher::unsubscribe() {
  auto lock = lockify(*m_revision);
  return unsubscribe_lck();
}

InvalidationWatcher::Ptr watchDependencies(
    const std::function<void(void)>& func) {
  auto ctx = std::make_unique<Context>(newestVisibleTxn());

  {
    Context::Guard ctxGuard(ctx.get());
    func();
  }

  return InvalidationWatcher::make(ctx->linearizeTrace());
}

struct AwaitableCaller final : GenericCaller,
                               Task,
                               LeakChecker<AwaitableCaller> {
  AwaitableCaller(
      Awaitable& awaitable,
      Invocation::Ptr invocation,
      MemoTask::Ptr memoTask)
      : GenericCaller(memoTask->m_queryTxn, nullptr),
        m_memoTask(std::move(memoTask)),
        m_invocation(std::move(invocation)),
        m_awaitable(&awaitable) {}

  AwaitableCaller(
      Awaitable& awaitable,
      Invocation::Ptr invocation,
      Context& callingContext)
      : GenericCaller(callingContext.m_queryTxn, &callingContext),
        m_invocation(std::move(invocation)),
        m_awaitable(&awaitable) {}

  void addDependency(Revision& lockedInput) override {
    m_value = lockedInput.value_lck();
    if (m_value.type() == MemoValue::Type::kUndef) {
      // This is a pretty weird case. This caller tried to evaluate an
      // Invocation, but by the time it got the answer back it had become
      // detached and forgotten its value, perhaps because the Invocation
      // fell off the bottom of LRU. In this case we will just trigger
      // a retry() in finish().
      refreshFailed();
    } else {
      if (auto ctx = m_callingContext) {
        ctx->addDependency(lockedInput);
      }
    }
  }

  void prepareForDeferredResult() override {
    if (!m_handle) {
      // We need to create a Handle here, before any GC can happen,
      // so we don't end up with a dangling Awaitable pointer.
      m_handle = Obstack::cur().makeHandle(m_awaitable);
      m_awaitable = nullptr;
      m_owningProcess = UnownedProcess(Process::cur());
    }
  }

  void retry() override {
    m_invocation->asyncEvaluate(*this);
  }

  void recordValue(Awaitable* awaitable) {
    auto iobj = m_value.asIObj();
    if (iobj) {
      Obstack::cur().registerIObj(iobj);
    }

    if (m_value.type() == MemoValue::Type::kException) {
      SKIP_awaitableThrow(awaitable, const_cast<MutableIObj*>(iobj));
    } else {
      SKIP_awaitableFromMemoValue(&m_value, awaitable);
      SKIP_awaitableNotifyWaitersValueIsReady(awaitable);
    }
  }

  // Provide Task::run so this can be posted to a remote Task.
  void run() override {
    // We know we are in the same Process as the Awaitable.
    recordValue(handleToAwaitable(*m_handle));
  }

  void finish() override {
    // ??? Yuck this is convoluted.
    if (m_refreshFailed) {
      m_refreshFailed = false;
      retry();
    } else {
      if (!m_handle) {
        // We never ran the Thunk, so we happen to know no one could be
        // suspended waiting on the MemoReturnAwaitable yet. Just fill it in.
        recordValue(m_awaitable);
        delete this;
      } else if (m_handle->isOwnedByCurrentProcess()) {
        // Provide the value right now, don't leave the caller with an
        // unfulfilled awaitable and a Task that would fulfill it. That
        // would work but it's inefficient to force everything to suspend
        // even though we have the value.
        run();
        delete this;
      } else {
        // Tidy up a bit before we post this Task, since it might hang
        // out there for quite a while.
        m_invocation.reset();
        m_callingContext = nullptr;

        // Need to notify a remote process.
        m_handle->scheduleTask(std::unique_ptr<Task>{this});
      }
    }
  }

 private:
  // Only finish() should be calling this.
  ~AwaitableCaller() override = default;

  // Task holding this txn ID alive. This can be null if there is
  // a calling context, as that context is responsible for keeping
  // this txn alive.
  MemoTask::Ptr m_memoTask;

  Invocation::Ptr m_invocation;

  // These two fields hold the Awaitable
  Awaitable* m_awaitable;
  std::unique_ptr<RObjHandle> m_handle;
};
} // namespace skip

using namespace skip;

void SKIP_memoizeCall(Awaitable* awaitable, RObj* uninternedInvocation) {
  assert(
      uninternedInvocation->type().internedMetadataByteSize() ==
      sizeof(Invocation));

  InvocationPtr inv(
      &Invocation::fromIObj(*intern(uninternedInvocation)), false);

  auto ctx = Context::current();
  AwaitableCaller* caller = ctx
      ? new AwaitableCaller(*awaitable, inv, *ctx)
      : new AwaitableCaller(*awaitable, inv, createMemoTask());

  inv->asyncEvaluate(*caller);
}

void SKIP_memoValueBoxFloat(MemoValue* mv, SkipFloat n) {
  *mv = MemoValue(n);
}

void SKIP_memoValueBoxInt(MemoValue* mv, SkipInt n) {
  *mv = MemoValue(n);
}

void SKIP_memoValueBoxString(MemoValue* mv, String n) {
  *mv = MemoValue(intern(n));
}

void SKIP_memoValueBoxObject(MemoValue* mv, RObjOrFakePtr fp) {
  if (auto ptr = fp.asPtr()) {
    *mv = MemoValue(intern(ptr), MemoValue::Type::kIObj, false);
  } else {
    *mv = MemoValue(fp.sbits(), MemoValue::Type::kFakePtr);
  }
}

// -----------------------------------------------------------------------------
// Memoizer serialization format:
//   uint64_t format_version
//   size_t build_hash
//   size_t iobj count
//   size_t invocation count
//   [objects]
//   uint8_t 'end' tag
//
// reference object format:
//   uint8_t kRefClassTag
//   size_t vtable_id
//   char[] data
//
// long string format:
//   uint8_t kLongStringTag
//   uint32_t u8length
//   char[] data
//
// array format:
//   uint8_t kArrayTag
//   size_t vtable_id
//   uint32_t array_size
//   char[] data
//
// invocation format:
//   uint8_t kInvocationTag
//   size_t vtable_id
//   char[] data
//   uintptr_t value
//   size_t trace_count
//   uintptr_t trace_targets[trace_count]
//
// Regex format:
//   uint8_t kRegexTag
//   uint32_t pattern_size
//   char[] pattern
//   int64_t flags
//
// references in data are serialized as indexes into the objects table.
//
// TODO: Think about using variable-length ints?

namespace {

constexpr bool kSerdeNoisy = true;

constexpr uint64_t kSerializeVersion = 0;

using VTableId = ssize_t;

constexpr VTableId kNonserializableId = -1;
constexpr VTableId kCellInvocationId = -2;
constexpr VTableId kRegexId = -3;

constexpr intptr_t kVTableOffset = 0x1000000000000000LL;

VTableRef getReferenceVTable() {
  static VTableRef ref = SKIP_createStringVector(1)->vtable();
  return ref;
}

VTableId vtableToId(skip::VTableRef vtable) {
  // On Linux the vtables don't move around (assuming the build doesn't change)
  // but on OSX with ASLR they change on every run.  Instead of storing a raw
  // pointer store an offset relative to Vector<String>'s vtable.  Because
  // negative vtable IDs are "magic" offset them by a large number to ensure
  // they're positive.

  if (vtable == static_cellInvocationVTable()) {
    return kCellInvocationId;
  } else if (vtable == Regex_static_vtable()) {
    return kRegexId;
  }

  static uintptr_t basis = getReferenceVTable().bits();
  return (intptr_t)((uintptr_t)vtable.bits() - basis + kVTableOffset);
}

skip::VTableRef idToVTable(VTableId id) {
  if (id == kCellInvocationId) {
    return static_cellInvocationVTable();
  } else if (id == kRegexId) {
    return Regex_static_vtable();
  }

  if (id < 0) {
    throwRuntimeError("Unexpected vtable id %zd\n", id);
  }

  static uintptr_t basis = getReferenceVTable().bits();
  return skip::VTableRef((uintptr_t)id + basis - kVTableOffset);
}

template <typename T, typename U>
void checkBounds(const T& obj, U value) {
  if ((size_t)value >= obj.size()) {
    throwRuntimeError("out of bounds");
  }
}

// Identifier of things being serialized/deserialized
enum Tag {
  kEndTag = 0,
  kRefClassTag = 1,
  kLongStringTag = 2,
  kArrayTag = 3,
  kInvocationTag = 4,
  kRegexTag = 5,
};

// A container which stores ref-counted Tk keys and associates them with an
// index.
template <typename Tk>
struct IndexMap {
  using map_type = fast_map<Tk, size_t>;
  using value_type = typename map_type::value_type;
  using iterator = typename map_type::iterator;
  using const_iterator = typename map_type::const_iterator;

  ~IndexMap() {
    for (auto& i : m_map) {
      intrusive_ptr_release(i.first);
    }
  }

  size_t size() const {
    return m_map.size();
  }

  size_t insert(Tk k) {
    size_t index = size() + 1;
    auto i = m_map.insert({k, index});
    if (!i.second) {
      throwRuntimeError("illegal second insertion in IndexMap");
    }
    intrusive_ptr_add_ref(k);
    return index;
  }

  const_iterator find(Tk k) const {
    return m_map.find(k);
  }
  const_iterator end() const {
    return m_map.end();
  }

  bool contains(Tk k) const {
    return find(k) != end();
  }

 private:
  map_type m_map;
};

struct MemoWriter {
  std::ostream& m_stream;

  explicit MemoWriter(std::ostream& f) : m_stream(f) {}

  uint64_t tell() const {
    return m_stream.tellp();
  }
  void seek(uint64_t pos) {
    m_stream.seekp(pos);
  }

  void writeUint8(uint8_t v) {
    write(&v, sizeof(v));
  }
  void writeInt64(int64_t v) {
    write(&v, sizeof(v));
  }
  void writeUint64(uint64_t v) {
    write(&v, sizeof(v));
  }
  void writeDouble(double v) {
    write(&v, sizeof(v));
  }
  void writeSize(size_t v) {
    write(&v, sizeof(v));
  }
  void writeSSize(ssize_t v) {
    write(&v, sizeof(v));
  }
  void writeUint32(uint32_t v) {
    write(&v, sizeof(v));
  }
  void writeUintPtr(uintptr_t v) {
    write(&v, sizeof(v));
  }

  void write(const void* p, size_t byteSize) {
    m_stream.write(static_cast<const char*>(p), byteSize);
    if (m_stream.fail())
      throwRuntimeError("error writing file");
  }

  void writeVTable(VTableRef vtable) {
    auto id = vtableToId(vtable);
    if (id == kNonserializableId) {
      skip::throwRuntimeError(
          "Attempt to serialize non-serializable IObj of type %s",
          vtable.type().name());
    }
    writeSSize((ssize_t)id);
  }

  void writeRef(RObjOrFakePtr obj, const IndexMap<IObj*>& mapping) {
    if (auto p = obj.asPtr()) {
      auto i = mapping.find(&p->cast<IObj>());
      if (i == mapping.end()) {
        skip::throwRuntimeError("Attempt to write unknown IObj");
      }
      writeUintPtr(i->second);
    } else {
      writeUintPtr(obj.bits());
    }
  }
};

struct MemoSerializer {
  MemoWriter m_writer;

  IndexMap<IObj*> m_iobjMapping;
  IndexMap<Invocation*> m_invMapping;

  intptr_t m_vtableDelta;
  size_t m_objCountPos;

  explicit MemoSerializer(std::ostream& f) : m_writer(f) {}

  void writeHeader() {
    // format version
    m_writer.writeUint64(kSerializeVersion);

    // build hash
    m_writer.writeSize(SKIPC_buildHash());

    // TODO: come up with a way to detect vtable changes - maybe some kind of
    // hash?

    m_objCountPos = m_writer.tell();
    // reserve space for object count
    m_writer.writeSize(0);

    // reserve space for invocation count
    m_writer.writeSize(0);
  }

  void writeFooter() {
    m_writer.writeUint8(kEndTag);

    // Go back and fill in the bits of the header that we couldn't know in
    // advance.
    m_writer.seek(m_objCountPos);
    m_writer.writeSize(m_iobjMapping.size());
    m_writer.writeSize(m_invMapping.size());
  }

  size_t lookupIObj(IObj& obj) {
    auto i = m_iobjMapping.find(&obj);
    if (i != m_iobjMapping.end()) {
      return i->second;
    }

    // This means that someone didn't visit a dependent pointer
    throwRuntimeError("Reference to unknown IObj");
  }

  size_t lookupInvocation(Invocation& inv) {
    auto i = m_invMapping.find(&inv);
    if (i != m_invMapping.end()) {
      return i->second;
    }

    // This means that somehow we have a cycle in the Invocation tree
    throwRuntimeError("Reference to unknown Invocation");
  }

  size_t visitIObj(IObj& obj) {
    if (obj.isCycleMember()) {
      // TODO: need to write all members of the cycle at one time and have a tag
      // indicating a cycle.
      throwRuntimeError("TODO: Unable to write IObj cycle");
    }

    auto i = m_iobjMapping.find(&obj);
    if (i == m_iobjMapping.end()) {
      return writeIObj(obj);
    } else {
      return i->second;
    }
  }

  const RObj* visitAndFixupObjectWithRefs(
      const RObj& obj,
      std::vector<char>& cloneBuffer) {
    auto& t = obj.type();
    if (!t.hasRefs()) {
      return &obj;
    }
    bool isArray = t.kind() == Type::Kind::array;
    size_t metaSize = isArray ? sizeof(AObjMetadata) : sizeof(RObjMetadata);
    size_t userSize = obj.userByteSize();
    auto p = reinterpret_cast<const char*>(&obj);
    cloneBuffer.assign(p - metaSize, p + userSize);
    RObj* copy =
        reinterpret_cast<RObj*>(mem::add(cloneBuffer.data(), metaSize));

    copy->forEachRef([this](RObjOrFakePtr& slot) {
      if (auto ptr = slot.asPtr()) {
        intptr_t index = visitIObj(ptr->cast<IObj>());
        slot.setSBits(index);
      }
    });

    return copy;
  }

  size_t writeIObj(IObj& obj) {
    assert(!m_iobjMapping.contains(&obj));

    if (obj.vtable() == Regex_static_vtable()) {
      writeRegex(obj);
    } else {
      auto& t = obj.type();
      switch (t.kind()) {
        case Type::Kind::array: {
          std::vector<char> cloneBuffer;
          auto copy = reinterpret_cast<const AObjBase*>(
              visitAndFixupObjectWithRefs(obj, cloneBuffer));
          m_writer.writeUint8(kArrayTag);
          m_writer.writeVTable(copy->vtable());
          m_writer.writeUint32(copy->arraySize());
          m_writer.write(copy, copy->userByteSize());
          break;
        }

        case Type::Kind::string: {
          m_writer.writeUint8(kLongStringTag);
          auto& str = obj.cast<const LongString>();
          m_writer.writeUint32(str.byteSize());
          m_writer.write(str.m_data, str.byteSize());
          break;
        }

        case Type::Kind::refClass: {
          std::vector<char> cloneBuffer;
          auto copy = visitAndFixupObjectWithRefs(obj, cloneBuffer);
          m_writer.writeUint8(kRefClassTag);
          m_writer.writeVTable(obj.vtable());
          m_writer.write(copy, copy->userByteSize());
          break;
        }

        case Type::Kind::cycleHandle:
        case Type::Kind::invocation:
          throwRuntimeError("Attempt to serialize unexpected IObj type");
          break;
      }
    }

    return m_iobjMapping.insert(&obj);
  }

  void writeRegex(IObj& obj) {
    m_writer.writeUint8(kRegexTag);

    String s = Regex_get_pattern(obj);
    m_writer.writeUint32(s.byteSize());
    String::DataBuffer buf;
    m_writer.write(s.data(buf), s.byteSize());
    m_writer.writeInt64(Regex_get_flags(obj));
  }

  void visitInvocation(Invocation& inv) {
    if (!m_invMapping.contains(&inv)) {
      writeInvocation(inv);
    }
  }

  static void trackRevisionAndUnlock(
      std::vector<Invocation::Ptr>& targets,
      Revision::Ptr&& rev,
      RevisionLock&& revLock) {
    auto& trace = rev->m_trace;
    const size_t sz = trace.size();

    std::vector<Revision::Ptr> subscriptions{sz};
    for (size_t i = 0; i < sz; ++i) {
      subscriptions[i] = Revision::Ptr(trace[i].target());
    }

    revLock.unlock();

    for (auto& sub : subscriptions) {
      auto subLock = lockify(sub.get());

      auto owner = sub->owner_lck();
      if (owner && !isCell(*owner)) {
        targets.push_back(owner);
      } else {
        // if there's no owner then we need to recursively track the
        // Revision's subscriptions to see who it depends on.
        trackRevisionAndUnlock(targets, std::move(sub), std::move(subLock));
      }
    }
  }

  void writeInvocation(Invocation& inv) {
    assert(!m_invMapping.contains(&inv));

    if (isCell(inv)) {
      // There's no way to know when deserializing a Cell where to put the value
      return;
    }

    MemoValue value;
    std::vector<Invocation::Ptr> targets;

    {
      auto invLock = lockify(inv);
      auto revision = Revision::Ptr(inv.m_headValue.ptr());
      // TODO: What if there are no Revisions?
      assert(revision != nullptr);
      value = revision->value_lck();

      auto revLock =
          revision->m_ownerAndFlags.convertLock(*revision, std::move(invLock));
      trackRevisionAndUnlock(targets, std::move(revision), std::move(revLock));
    }

    visitMemoValue(value);
    std::vector<char> cloneBuffer;
    auto copy = visitAndFixupObjectWithRefs(*inv.asIObj(), cloneBuffer);
    for (auto& i : targets) {
      if (i != nullptr) {
        visitInvocation(*i);
      }
    }

    m_writer.writeUint8(kInvocationTag);
    m_writer.writeVTable(inv.m_metadata.m_vtable);
    m_writer.write(copy, copy->userByteSize());
    writeMemoValue(value);

    m_writer.writeSize(targets.size());
    for (auto& i : targets) {
      assert(i != nullptr);
      auto id = lookupInvocation(*i);
      assert(id > 0 && id <= m_invMapping.size());
      m_writer.writeSize(id);
    }

    m_invMapping.insert(&inv);
  }

  void visitMemoValue(const MemoValue& value) {
    switch (value.type()) {
      case MemoValue::Type::kNull:
      case MemoValue::Type::kInt64:
      case MemoValue::Type::kDouble:
      case MemoValue::Type::kShortString:
      case MemoValue::Type::kFakePtr:
        break;
      case MemoValue::Type::kIObj:
      case MemoValue::Type::kException:
      case MemoValue::Type::kLongString: {
        visitIObj(*value.asIObj());
        break;
      }
      case MemoValue::Type::kUndef:
      case MemoValue::Type::kContext:
      case MemoValue::Type::kInvalidationWatcher: {
        skip::throwRuntimeError(
            "Attempt to serialize unhandled MemoValue type");
        break;
      }
    }
  }

  void writeMemoValue(const MemoValue& value) {
    m_writer.writeUint8((uint8_t)value.type());
    switch (value.type()) {
      case MemoValue::Type::kNull: {
        // no-op
        break;
      }
      case MemoValue::Type::kInt64: {
        m_writer.writeInt64(value.asInt64());
        break;
      }
      case MemoValue::Type::kDouble: {
        m_writer.writeDouble(value.asDouble());
        break;
      }
      case MemoValue::Type::kShortString: {
        m_writer.writeInt64(value.bits());
        break;
      }
      case MemoValue::Type::kIObj:
      case MemoValue::Type::kException:
      case MemoValue::Type::kLongString: {
        m_writer.writeRef(
            const_cast<MutableIObj*>(value.asIObj()), m_iobjMapping);
        break;
      }
      case MemoValue::Type::kFakePtr: {
        m_writer.writeUintPtr(value.bits());
        break;
      }
      case MemoValue::Type::kUndef:
      case MemoValue::Type::kContext:
      case MemoValue::Type::kInvalidationWatcher: {
        skip::throwRuntimeError(
            "Attempt to serialize unhandled MemoValue type");
        break;
      }
    }
  }
};

struct MemoReader {
  std::istream& m_stream;
  size_t m_pos = 0;

  explicit MemoReader(std::istream& f) : m_stream(f) {}

  uint8_t readUint8() {
    uint8_t v;
    read(&v, sizeof(v));
    return v;
  }
  uint32_t readUint32() {
    uint32_t v;
    read(&v, sizeof(v));
    return v;
  }
  uint64_t readUint64() {
    uint64_t v;
    read(&v, sizeof(v));
    return v;
  }
  int64_t readInt64() {
    int64_t v;
    read(&v, sizeof(v));
    return v;
  }
  double readDouble() {
    double v;
    read(&v, sizeof(v));
    return v;
  }
  size_t readSize() {
    size_t v;
    read(&v, sizeof(v));
    return v;
  }
  ssize_t readSSize() {
    ssize_t v;
    read(&v, sizeof(v));
    return v;
  }
  uintptr_t readUintPtr() {
    uintptr_t v;
    read(&v, sizeof(v));
    return v;
  }
  intptr_t readIntPtr() {
    intptr_t v;
    read(&v, sizeof(v));
    return v;
  }

  void read(void* p, size_t byteSize) {
    m_stream.read(static_cast<char*>(p), byteSize);
    if (m_stream.fail())
      throwRuntimeError("error reading file");
    m_pos += byteSize;
  }

  VTableRef readVTable() {
    return idToVTable(readSSize());
  }

  RObjOrFakePtr readRef(std::vector<IObjPtr>& mapping) {
    auto sbits = readIntPtr();
    if (sbits <= 0)
      return RObjOrFakePtr(sbits);
    checkBounds(mapping, sbits - 1);
    RObj* obj = const_cast<MutableIObj*>(mapping[sbits - 1].get());
    return RObjOrFakePtr(obj);
  }
};

struct MemoDeserializer {
  MemoReader m_reader;
  std::vector<IObjPtr> m_iobjMapping;
  std::vector<Revision::Ptr> m_revMapping;

  explicit MemoDeserializer(std::istream& f) : m_reader(f) {}

  void readHeader() {
    // format version
    if (m_reader.readUint64() != kSerializeVersion) {
      skip::throwRuntimeError("incorrect file version");
    }

    // build hash
    size_t hash = m_reader.readSize();
    if (hash != SKIPC_buildHash()) {
      skip::throwRuntimeError("mismatched build hash");
    }

    size_t objCount = m_reader.readSize();
    size_t invCount = m_reader.readSize();

    auto& table = getInternTable();
    table.reserve(table.size() + objCount + invCount);
  }

  void readObjectWithRefs(RObj& obj, size_t byteSize) {
    m_reader.read(&obj, byteSize);

    obj.forEachRef([this](RObjOrFakePtr& ptr) {
      if (ptr.sbits() > 0) {
        size_t id = ptr.sbits() - 1;
        checkBounds(m_iobjMapping, id);
        ptr = RObjOrFakePtr(const_cast<MutableIObj*>(m_iobjMapping[id].get()));
      }
    });
  }

  void readRegex() {
    arraysize_t u8length = m_reader.readUint32();
    std::vector<char> buf(u8length);
    m_reader.read(buf.data(), u8length);
    String pattern(buf.begin(), buf.end());

    int64_t flags = m_reader.readInt64();
    auto& obj = SKIP_Regex_initialize(pattern, flags)->cast<IObj>();
    m_iobjMapping.push_back(IObjPtr(&obj));
  }

  void readRefClass() {
    auto vtable = m_reader.readVTable();
    auto& type = vtable.type();

    // This could probably be improved by reading directly into IObj memory and
    // poking at the internals of the interner.
    auto obj = allocObject(type, type.userByteSize());
    obj->metadata().m_vtable = vtable;

    readObjectWithRefs(*obj, obj->userByteSize());
    IObjPtr iobj{skip::intern(obj), false};
    m_iobjMapping.push_back(std::move(iobj));
  }

  void readLongString() {
    arraysize_t u8length = m_reader.readUint32();
    auto& type = LongString::static_type();

    size_t userByteSize = String::computePaddedSize(u8length);
    auto& obj = allocObject(type, userByteSize)->cast<LongString>();
    obj.metadata().m_byteSize = u8length;

    m_reader.read(&obj, u8length);
    obj.metadata().m_hash = String::computeStringHash(obj.m_data, u8length);

    m_iobjMapping.push_back(IObjPtr(skip::intern(&obj), false));
  }

  void readArray() {
    auto vtable = m_reader.readVTable();
    auto& type = vtable.type();

    arraysize_t arraySize = m_reader.readUint32();
    size_t userByteSize = arraySize * type.userByteSize();
    auto& obj = allocObject(type, userByteSize)->cast<AObjBase>();
    obj.metadata().m_vtable = vtable;
    obj.setArraySize(arraySize);

    readObjectWithRefs(obj, userByteSize);

    m_iobjMapping.push_back(IObjPtr(skip::intern(&obj), false));
  }

  struct FakeCaller final : GenericCaller, LeakChecker<FakeCaller> {
    FakeCaller(Context& callingContext)
        : GenericCaller(callingContext.m_queryTxn, &callingContext) {}

    FakeCaller(MemoTask::Ptr memoTask)
        : GenericCaller(memoTask->m_queryTxn, nullptr),
          m_memoTask(std::move(memoTask)) {}

    void retry() override {
      // should never get a 'retry' during this fake caller
      abort();
    }

    void finish() override {
      // ??? Yuck this is convoluted.
      if (m_refreshFailed) {
        m_refreshFailed = false;
        retry();
        return;
      }

      delete this;
    }

   private:
    MemoTask::Ptr m_memoTask;
  };

  // Act as if the Invocation was called but instead of actually running it
  // set up the known dependencies and return the expected value.
  Invocation::Ptr fakeCallInvocation(
      RObj* uninternedInvocation,
      MemoValue&& value,
      std::vector<Revision*>& targets) {
    assert(
        uninternedInvocation->type().internedMetadataByteSize() ==
        sizeof(Invocation));

    InvocationPtr inv(
        &Invocation::fromIObj(*intern(uninternedInvocation)), false);

    auto ctx = Context::current();
    auto* caller = ctx
        ? new FakeCaller(*ctx) // AwaitableCaller(*awaitable, inv, *ctx)
        : new FakeCaller(createMemoTask()); // AwaitableCaller(*awaitable, inv,
                                            // createMemoTask());

    inv->asyncEvaluateWithCustomEval(
        *caller, [&targets, &value]() -> Awaitable* {
          // depend on the targets and then return the value

          auto curctx = Context::current();
          // TODO: This will fail horribly if some intermediate value gets
          // flushed and actually has to be recomputed.
          for (auto& r : targets) {
            withLockify<void>(r, [r, curctx]() { curctx->addDependency(*r); });
          }

          Context::current()->evaluateDone(std::move(value));

          return nullptr;
        });

    return inv;
  }

  void readInvocation() {
    auto vtable = m_reader.readVTable();
    auto& type = vtable.type();

    size_t userByteSize = type.userByteSize();
    auto& obj = *allocObject(type, userByteSize);
    obj.metadata().m_vtable = vtable;

    readObjectWithRefs(obj, userByteSize);

    auto value = readMemoValue();

    size_t sz = m_reader.readSize();
    std::vector<Revision*> targets;
    for (size_t i = 0; i < sz; ++i) {
      auto id = m_reader.readSize();
      checkBounds(m_revMapping, id - 1);
      targets.push_back(m_revMapping[id - 1].get());
    }

    Invocation::Ptr inv = fakeCallInvocation(&obj, std::move(value), targets);
    auto rev = withLockify<Revision::Ptr>(
        inv.get(), [&inv]() { return Revision::Ptr(inv->m_headValue.ptr()); });
    assert(rev != nullptr);
    m_revMapping.push_back(std::move(rev));
  }

  void readAll() {
    auto& obcur = Obstack::cur();
    auto note = obcur.note();

    while (true) {
      auto tag = (Tag)m_reader.readUint8();
      switch (tag) {
        case kEndTag:
          return;
        case kRefClassTag:
          readRefClass();
          break;
        case kLongStringTag:
          readLongString();
          break;
        case kArrayTag:
          readArray();
          break;
        case kInvocationTag:
          readInvocation();
          break;
        case kRegexTag:
          readRegex();
          break;
        default:
          throwRuntimeError("Unknown tag type %d\n", (int)tag);
      }

      obcur.collect(note);
    }
  }

  MemoValue readMemoValue() {
    auto type = (MemoValue::Type)m_reader.readUint8();
    switch (type) {
      case MemoValue::Type::kNull:
        return MemoValue{nullptr};
      case MemoValue::Type::kFakePtr: {
        auto p = m_reader.readIntPtr();
        return MemoValue{p, type};
      }
      case MemoValue::Type::kInt64:
        return MemoValue{m_reader.readInt64()};
      case MemoValue::Type::kDouble:
        return MemoValue{m_reader.readDouble()};
      case MemoValue::Type::kShortString: {
        auto s = m_reader.readInt64();
        return MemoValue{s, type};
      }
      case MemoValue::Type::kIObj:
      case MemoValue::Type::kException:
      case MemoValue::Type::kLongString: {
        auto id = m_reader.readUintPtr();
        checkBounds(m_iobjMapping, id - 1);
        return MemoValue{m_iobjMapping[id - 1].get(), type};
      }
      case MemoValue::Type::kUndef:
      case MemoValue::Type::kContext:
      case MemoValue::Type::kInvalidationWatcher: {
        skip::throwRuntimeError(
            "Attempt to deserialize unhandled MemoValue type %d", (int)type);
        break;
      }
    }

    // should never get here
    abort();
  }

  skip::RObj* allocObject(Type& type, size_t userByteSize) {
    size_t mbs = type.uninternedMetadataByteSize();
    void* rawData = skip::Obstack::cur().calloc(mbs + userByteSize);
    return static_cast<skip::RObj*>(mem::add(rawData, mbs));
  }
};

int64_t currentMillis() {
  return std::chrono::system_clock::now().time_since_epoch() /
      std::chrono::milliseconds(1);
}
} // namespace

void MemoSerde::serializeMemoCache(std::ostream& f, TxnId txnId) {
  int64_t a = currentMillis();
  if (kSerdeNoisy)
    fprintf(stderr, "--- serialize start\n");
  MemoSerializer ser(f);
  ser.writeHeader();

  auto& table = getInternTable();
  for (ssize_t i = table.numberOfBuckets() - 1; i >= 0; --i) {
    Bucket& bucket = table.lockHash(i);
    auto contents = table.internalGetBucketContentsAndUnlock(bucket);
    for (auto& p : contents) {
      if (p->type().kind() == Type::Kind::invocation) {
        // TODO: only visit invocations which have a revision at txnId
        ser.visitInvocation(Invocation::fromIObj(*p));
      }
    }
  }

  ser.writeFooter();

  if (kSerdeNoisy) {
    int64_t b = currentMillis();
    fprintf(stderr, "--- serialize end (%fs)\n", (b - a) / 1000.0);
  }
}

void MemoSerde::deserializeMemoCache(std::istream& f) {
  int64_t a = currentMillis();
  if (kSerdeNoisy)
    fprintf(stderr, "--- deserialize start\n");

  MemoDeserializer des(f);
  des.readHeader();
  des.readAll();

  if (kSerdeNoisy) {
    int64_t b = currentMillis();
    fprintf(stderr, "--- deserialize end (%fs)\n", (b - a) / 1000.0);
  }
}

void MemoSerde::deserializeMemoCache(const std::string& filename) {
  // In theory this could be done as a parallel worker task.
  std::ifstream fb(filename, std::ios::in | std::ios::binary);
  if (!fb.good()) {
    skip::throwRuntimeError("Unable to open file '%s'", filename.c_str());
  }
  MemoSerde::deserializeMemoCache(fb);
}

void MemoSerde::serializeMemoCache(const std::string& filename) {
  std::ofstream fb(filename, std::ios::out | std::ios::binary);
  if (!fb.good()) {
    skip::throwRuntimeError("Unable to open file '%s'", filename.c_str());
  }
  MemoSerde::serializeMemoCache(fb, newestVisibleTxn());
}
