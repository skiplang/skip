/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "Obstack.h"
#include "String.h"
#include "map.h"
#include "intern.h"
#include "leak.h"
#include "memoize-extc.h"
#include "objects.h"
#include "Process.h"

#include <functional>
#include <future>

#include <boost/intrusive_ptr.hpp>
#include <boost/operators.hpp>

namespace skip {

namespace detail {
// To make Revision more compact we don't use all 64 bits of a TxnId.
constexpr size_t kNumTxnIdBytes = 6;
} // namespace detail

// A 48-bit transaction ID.
using TxnId = uint64_t;

/**
 * An "infinity" TxnId, used for the end lifespan of values which, as far as
 * we know now, are valid forever.
 *
 * If the "begin" of that lifespan is zero, then the guarantee is permanent --
 * the value can never change. Otherwise, some external state could change and
 * "invalidate" the lifespan "end" back the finite TxnId of the commit that
 * truncated the lifespan.
 */
constexpr TxnId kNeverTxnId = ~0ULL >>
    ((sizeof(~0ULL) - detail::kNumTxnIdBytes) * 8);

// Inclusive begin..end transaction range for active reader tasks.
TxnId oldestVisibleTxn();
TxnId newestVisibleTxn();

// Every Revision pointer is aligned mod 64 bytes. In addition to being
// cache-efficient, this allows us to "steal" the low 6 pointer bits as an
// EdgeIndex.
constexpr size_t kLog2RevisionAlign = 6;
constexpr size_t kRevisionAlign = (size_t)1 << kLog2RevisionAlign;

// Every SubArray pointer is aligned mod 64 bytes. In addition to being
// cache-efficient, this allows us to "steal" the low 6 pointer bits as an
// index into a SubArray's m_subs array; see Edge.
constexpr size_t kLog2SubArrayAlign = kLog2RevisionAlign;
constexpr size_t kSubArrayAlign = (size_t)1 << kLog2SubArrayAlign;

// We align traces mod 16 so Trace has 4 tag bits to play with.
constexpr size_t kLog2TraceArrayAlign = 4;
constexpr size_t kTraceArrayAlign = (size_t)1 << kLog2TraceArrayAlign;

template <typename T>
struct LockGuard;
struct TestPrivateAccess;
struct Refresher;
struct SubArray;
struct UpEdge;
struct alignas(kRevisionAlign) Revision;

using RevisionLock = LockGuard<const Revision>;
using InvocationLock = LockGuard<const Invocation>;
using InvocationPtr = boost::intrusive_ptr<Invocation>;

/// See docs for Edge.
using EdgeIndex = uint8_t;

/*

Edge is an edge in the memoization dependency graph. The nodes are
Revision objects.

Each Edge is either a DownEdge, meaning it points from a Revision to
one of the "input" Revisions that were used to compute its value,
or UpEdge, meaning it points from a Revision to a Revision that
depends on its value. (The suggested mental picture is that changes
"bubble up" from leaves at the bottom of the graph, ultimately reaching
external consumers at the top, hence the "up" vs. "down" terminology.)

Each Revision has an optional ordered sequence of DownEdges in its Trace
and an optional unordered set of UpEdges in its SubscriptionSet.

Despite having two subclasses, Edge has no vtable because it is very
space-sensitive. Edge::isDownEdge() allows the runtime subclass to
be determined, although this is rarely needed.

The graph is bidirectional, with each UpEdge corresponding to a DownEdge
and vice versa. Removing one of those edges also requires removing the
"reverse" edge. Each edge knows how to "find" the corresponding reverse
edge efficiently.

Semantically (but not always physically), an Edge encodes two things:

(1) target(): the node (Revision*) in the graph to which
    the edge points.

(2) index(): an integer (EdgeIndex) indicating where in target() the
    reverse edge can be found.

UpEdge and DownEdge have somewhat different representations.

Each UpEdge really is just a (Revision*, EdgeIndex) pair, encoded
as a tagged pointer. The EdgeIndex indicates the index in that
Revision's m_trace (Trace) for the corresponding DownEdge.

DownEdge is a bit more complicated. Over time a Revision can accrue
an unlimited number of UpEdges, and DownEdge needs to be able to identify
any of them (i.e. its reverse edge) using only a single pointer and a
6-bit index().

Typically, a DownEdge points to one of the SubArray objects owned by the
target's m_subs (SubscriptionSet).
That SubArray in turn points to the target() Revision. In this case,
DownEdge::index() is the index in that SubArray's m_subs array where
the corresponding UpEdge can be found.

As a special case, however, a SubscriptionSet with only a single UpEdge does
not actually allocate a SubArray object. Instead, the SubscriptionSet
pointer field that would normally point to its SubArray linked list is
repurposed to hold a single UpEdge. So DownEdges with an index() of
kInlineSubscriptionIndex actually point directly to the Revision
rather than to a SubArray. If a second UpEdge gets added then the original
"inline" UpEdge gets moved to a SubArray and is overwritten with a pointer
to that SubArray. Such an UpEdge is always found at index 1 in that first
SubArray, so it can still be found in O(1) time by a DownEdge with
index() == kInlineSubscriptionIndex even if it has "moved" to no longer
be inline.

*/
struct Edge : boost::totally_ordered<Edge> {
  enum {
    kInlineSubscriptionIndex = kRevisionAlign - 1,
    kNoEdgeIndex = kInlineSubscriptionIndex - 1
  };

  Edge();

  Edge(SubArray* array, EdgeIndex index);

  Edge(Revision* rev, EdgeIndex index);

  Revision* target() const;

  EdgeIndex index() const;

  void* rawPointer() const;

  void reset();

  bool isSubArray() const;

  SubArray* asSubArray() const;

  Revision* asRevision() const;

  bool isNull() const;

  bool isDownEdge() const;

  bool operator==(const Edge& other) const;

  bool operator<(const Edge& other) const;

 private:
  // An Edge can point to either a SubArray or a Revision. Because they
  // have high alignment requirements, that leaves the low bits to be used
  // as an index() that locates the corresponding reverse edge.
  //
  // Keeping the two alignments the same makes the bookkeeping a bit easier.
  static_assert(kLog2SubArrayAlign == kLog2RevisionAlign, "");

  enum {
    // Number of tag bits to be used for index().
    kAlignBits = kLog2SubArrayAlign,

    // One extra tag bit used to distinguish a SubArray* from Revision*.
    kSubArrayFlag = 1U << kAlignBits
  };

  using Rep =
      SmallTaggedPtr<void, 1 + kAlignBits, false, false, true, kAlignBits>;

  Rep m_pointerAndIndex;
};

// See docs for Edge.
struct DownEdge final : Edge {
  DownEdge();
  DownEdge(SubArray& array, EdgeIndex index);
  explicit DownEdge(const Revision& rev);

  // Checked downcast from Edge to DownEdge.
  explicit DownEdge(Edge edge);

  DownEdge(const DownEdge& other) = default;
  DownEdge& operator=(const DownEdge& other) = default;

  UpEdge dereference() const;
};

// See docs for Edge.
struct UpEdge final : Edge {
  UpEdge(Revision& rev, EdgeIndex index);

  // Checked downcast from Edge to UpEdge.
  explicit UpEdge(Edge edge);

  DownEdge dereference() const;

  void assign(DownEdge d);
};

/**
 * TraceArray is the underlying implementation of Trace.
 *
 * A TraceArray consists of an array of DownEdges and a bit mask indicating
 * which of those DownEdges points to a Revision whose end() != kNeverTxnId.
 * Those are the only DownEdges that need further examination while refreshing.
 *
 * Once created, these are immutable except for the kNumInactiveFlagBits field.
 */
class alignas(kTraceArrayAlign) TraceArray final : private skip::noncopyable {
  // You can only create one by calling the "make" factory.
  explicit TraceArray(EdgeIndex size) : m_size(size), m_inactive(0) {
    std::uninitialized_fill_n(m_inputs, size, DownEdge());
  }

 public:
  /// Factory.
  static TraceArray* make(size_t size);

  ~TraceArray();

  void operator delete(void* ptr);

  enum { kNumSizeFieldBits = 8, kNumInactiveFlagBits = 64 - kNumSizeFieldBits };

  // Total size of the m_inputs array below (at most kMaxTraceSize).
  uint64_t m_size : kNumSizeFieldBits;

  // Bit mask of which entries in m_inputs have an "end" TxnId other than
  // kNeverTxnId (i.e. are "inactive"). When refreshing a Trace, those are
  // the only ones that need to be visited.
  uint64_t m_inactive : kNumInactiveFlagBits;

  // TODO: Consider adding another uint64_t here with a 1 bit for every
  // control flow point. When refreshing, everything up to and including
  // the next control flow point can be refreshed in parallel.
  // Or just have a tag bit that says that an entire sub-trace can be
  // done in parallel.

  DownEdge m_inputs[];
};

// Fit as many entries as we can in a TraceArray while filling up cache lines.
// Traces larger than this will be arranged in a kMaxTraceSize-ary tree.
// For example with 6-byte Edges this expression yields 52, making a
// 320 byte TraceArray, which is one of jemalloc's predefined sizes.
constexpr EdgeIndex kMaxTraceSize =
    ((((offsetof(TraceArray, m_inputs) +
        (((int)TraceArray::kNumInactiveFlagBits < Edge::kNoEdgeIndex
              ? (int)TraceArray::kNumInactiveFlagBits
              : Edge::kNoEdgeIndex) *
         sizeof(((TraceArray*)nullptr)->m_inputs[0]))) &
       -kCacheLineSize) -
      offsetof(TraceArray, m_inputs)) /
     sizeof(((TraceArray*)nullptr)->m_inputs[0]));

static_assert(
    kMaxTraceSize <= Edge::kNoEdgeIndex &&
        kMaxTraceSize < (1 << TraceArray::kNumSizeFieldBits) &&
        kMaxTraceSize <= TraceArray::kNumInactiveFlagBits &&
        kMaxTraceSize == (EdgeIndex)kMaxTraceSize,
    "Bad kMaxTraceSize value.");

/**
 * A Trace records the sequence of calls made by a memoized Invocation
 * as a sequence of DownEdges.
 *
 * DownEdges are "strong" references, so a Trace's edge array "keeps alive"
 * the inputs to a Revision.
 */
struct Trace final : private skip::noncopyable {
  Trace() : Trace(0) {}
  explicit Trace(size_t size);

  Trace(Trace&& other) noexcept;

  Trace& operator=(Trace&& other) noexcept;

  ~Trace();

  void verifyInvariants(const Revision& owner) const;

  // Returns the number of DownEdges in the Trace.
  EdgeIndex size() const;

  // Returns true iff size() == 0. An empty() trace is typically one that
  // has been discarded to save memory.
  bool empty() const;

  // Returns the Nth DownEdge.
  const DownEdge operator[](EdgeIndex index) const;

  // Discard all edges.
  void clear();

  // A bit mask of which DownEdges are "inactive".
  uint64_t inactive() const;

  // Mark a given edge as inactive.
  void setInactive(EdgeIndex index, Revision& owner, int line);

  // Mark a given edge as active.
  void setActive(EdgeIndex index, Revision& owner, int line);

  // Set the given trace entry to the DownEdge.
  //
  // WARNING: This must only be used while initially building the Trace!
  void assign(EdgeIndex index, DownEdge downEdge);

 private:
  // The underlying array, or nullptr if none. Note that if there is only
  // one DownEdge it is not stored in a separate array, so this will return
  // nullptr. Use operator[] to fetch edges.
  TraceArray* array() const;

  // Assuming that size() == 1, returns the DownEdge.
  const DownEdge inlineEdge() const;

  enum {
    // m_rep is actually a TraceArray* (possibly nullptr, meaning "no trace").
    kTraceArrayTag = 0x1e,

    // There is a single inline edge, and m_rep encodes a Revision* with
    // an EdgeIndex implicitly equal to kInlineSubscriptionIndex.
    kShortInlineSubscriptionIndex = 0xe,

    // There will be a single inline edge but it's not set up yet.
    kShortNoEdgeIndex = 0xd,

    // ...else there is a single inline edge, and m_rep holds a SubArray* with
    // an EdgeIndex equal to the tag.
  };

  static_assert(kTraceArrayTag > kShortInlineSubscriptionIndex * 2 + 1, "");

  // The m_rep field is one of three things:
  //
  // - if tag() != kTraceArrayTag:
  //     m_rep holds a single "inline" DownEdge and an "inactive" flag.
  //     The intent is to avoid the memory overhead of allocating a
  //     TraceArray for just one edge.  This DownEdge is encoded
  //     specially, taking advantage of the fact that we only need a
  //     few bits for a SubArray index.  (see operator[] for details).
  // - else if ptr() != nullptr:
  //     it is a TraceArray* holding the inputs.
  // - else:
  //     there is no trace.
  using Rep = SmallTaggedPtr<void, 5, false, false, true, kLog2TraceArrayAlign>;
  Rep m_rep;

  static_assert(alignof(TraceArray) >= 1U << Rep::kNumAlignBits, "");
  static_assert(1 << Rep::kNumTagBits > kShortInlineSubscriptionIndex, "");
};

/**
 * An array of "invalidate()" subscriber information. The full set of
 * subscriptions for a Revision may be composed of many of these,
 * chained together in a linked list.
 */
struct alignas(kSubArrayAlign) SubArray final : Aligned<SubArray>,
                                                LeakChecker<SubArray> {
  friend struct SubscriptionSet;

  // Choose an array size such that this packs what we can into a cache line.
  enum : EdgeIndex {
    kArraySize = (kCacheLineSize - 2 * sizeof(void*)) / sizeof(Edge)
  };
  static_assert(
      kArraySize > 1 && kArraySize <= (int)DownEdge::kNoEdgeIndex &&
          kArraySize <= (int)DownEdge::kInlineSubscriptionIndex,
      "SubArray size is bad");

  SubArray(Revision& owner, SubArray* nextArray);

  static constexpr EdgeIndex size() {
    return kArraySize;
  }

  // Revision that owns the SubscriptionSet that owns this SubArray.
  Revision& m_owner;

  // Next in linked list of all SubArrays for this SubscriptionSet.
  SubArray* m_next;

  // Each array entry can be in one of two states:
  //
  // - if asRevision() != nullptr then it's a pointer to a subscriber,
  //   used to send it invalidate() messages.
  //
  // - Else, asSubArray() + index() indicates that this slot is "free" and
  //   points the the next entry in the freelist of Edges for the
  //   SubscriptionSet that owns this SubArray, or isNull() for end of list.
  //   As a special case the first Edge in the head of the SubArray linked
  //   list is always used to hold the head of the freelist.
  Edge m_subs[kArraySize];
};

static_assert(
    sizeof(SubArray) <= kCacheLineSize &&
        sizeof(SubArray) > kCacheLineSize - sizeof(Edge),
    "");

/**
 * An unordered set of UpEdges to Revisions that want to be notified via
 * invalidate() when the end_lck() TxnId for the Revision that owns this
 * SubscriptionSet transitions from kNeverTxnId to some "finite" value,
 * or when the owning Revision loses its ability to refresh.
 *
 * Since subscriptions come and go, available storage for UpEdges is chained
 * together into a freelist.
 */
struct SubscriptionSet : private skip::noncopyable {
  SubscriptionSet();

  ~SubscriptionSet();

  // Tell all subscribers to examine their edges back to this object's
  // owner and update their state appropriately.
  void invalidateSubscribers_lck(Revision& owner);

  // Discard all UpEdges.
  void clear();

  // Quick check that returns true if there are no subscriptions and
  // no memory allocated to hold subscriptions. Even if this returns false
  // there may still be no subscriptions.
  bool obviouslyEmpty() const;

  // Returns the UpEdge stored at DownEdge::kInlineSubscriptionIndex.
  // It might still be stored "inline" or it may have been moved to a SubArray.
  UpEdge inlineSubscriber();

  // Remove the subscription identified by DownEdge (DownEdges point to UpEdges
  // and vice versa, so this is how we find what to unsubscribe).
  void unsubscribe(DownEdge edge);

  // Given 'owner' which owns this SubscriptionSet, subscribe 'subscriber'.
  // This also modifies subscriber to point back to the allocated subscription
  // slot.
  void subscribe(Revision& owner, UpEdge subscriber);

  // Compares the size of this set to the given set, returning -1, 0, 1
  // if this set is smaller, equal to, or larger than 'other'.
  //
  // WARNING: this takes O(n) time, where n is the size of the smaller set.
  int compareSizes(const SubscriptionSet& other) const;

  void verifyInvariants(const Revision& owner) const;

  // Iterator for walking through subscriptions. It knows how to skip over
  // freelisted entries.
  struct iterator final : boost::iterator_facade<
                              iterator,
                              const UpEdge,
                              boost::forward_traversal_tag> {
    iterator();

    iterator(const iterator&) = default;
    iterator& operator=(const iterator&) = default;

   private:
    // Provide the boost iterator_facade with the magic it needs.
    friend boost::iterator_core_access;

    friend struct SubscriptionSet;

    explicit iterator(Edge pos);

    const UpEdge& dereference() const;

    bool equal(const iterator& other) const;

    void increment();

    Edge m_pos;
  };

  iterator begin() const;

  iterator end() const;

 private:
  friend struct TestPrivateAccess;

  // The first subscribed UpEdge is stored "inline", but when a second
  // subscription is taken we need to reuse that inline storage to point
  // to the SubArray list. So we move that first inline edge to the head
  // of the SubArray list at this index.
  enum { kIndexForInlineSubscriptionMovedToSubArray = 1 };

  Edge& freelistHead();

  const Edge freelistHead() const;

  // This is one of three things:
  //
  // - If m_subs.isNull(), there are no subscriptions at all.
  // - Else if m_subs.asRevision(), there is exactly one subscriber, and
  //   it is stored directly in this field.
  // - Else (m_subs.asSubArray()), it points to a linked list of
  //   SubArray objects holding all subscriptions. The first slot in the first
  //   SubArray points to the head of the freelist. [NOTE: It would be possible
  //   to be slightly more memory efficient and recover one more slot by
  //   making the SubArray linked list circular, and having m_subs point to
  //   the freelist head (or to some arbitrary list member + kNoEdgeIndex if
  //   the freelist is empty), but then finding an entry moved to a SubArray
  //   becomes messy so it does not seem worth it].
  Edge m_subs;
};

// A future-like object that gets notified when the value returned by
// asyncEvaluate() becomes invalidated by some input dependency change.
struct InvalidationWatcher : LeakChecker<InvalidationWatcher> {
  using Ptr = boost::intrusive_ptr<InvalidationWatcher>;

  static Ptr make() {
    // The RefCount of 2 is for one coming from m_revision, and one
    // for the Ptr we are creating.
    return Ptr(new InvalidationWatcher(2), false);
  }

  static Ptr make(std::vector<boost::intrusive_ptr<Revision>>&& trace);

  ~InvalidationWatcher();

  // Stops watching for invalidations. Returns true if successful, false
  // if it was already not watching (either because it was already detached,
  // or because it was already notified of an invalidation or is in the
  // process of being notified).
  bool unsubscribe();

  // Is this subscribed to invalidation notifications?
  bool isSubscribed();

  // For internal use by InvalidationWatcher::Ptr.
  void incref();
  void decref();

  std::future<void> getFuture() {
    return m_promise.get_future();
  }

  // This method is called during invalidation notify. The lock is not held
  // during this call, but the caller does keep a InvalidationWatcher::Ptr
  // reference throughout the invocation, to guarantee it does not get freed.
  void invalidate();

 private:
  explicit InvalidationWatcher(Refcount refcount = 1);

  // Grant ability to call invalidateIfReady().
  friend struct Transaction;

  friend struct GenericCaller;
  friend struct FutureCaller;

  bool unsubscribe_lck();

  AtomicRefcount m_refcount;

  // This is a special, dummy Revision used only as a node in the invalidation
  // graph. When its value is invalidated, it uses the InvalidationWatcher*
  // value in its MemoValue to notify this InvalidationWatcher after the
  // transaction commit is finished. That back pointer increases the refcount
  // of this InvalidationWatcher by one. Once either the invalidation
  // notification is delivered, or detach() is called, the Revision discards
  // its pointer back to this InvalidationWatcher and decrements the
  // InvalidationWatcher's refcount.
  //
  // The InvalidationWatcher always points to the m_revision, even when it
  // no longer points back, and uses m_revision's lock as its own lock.
  boost::intrusive_ptr<Revision> m_revision;

  std::promise<void> m_promise;
};

// Execute the given code and collect any memoized dependencies it visited into
// an InvalidationWatcher. If no dependencies were observed, this returns null.
InvalidationWatcher::Ptr watchDependencies(
    const std::function<void(void)>& func);

/** A memoized value (a tagged union). A Revision holds one of these. */
struct __attribute__((packed)) MemoValue {
  // NOTE: If you change this enum, update isSkipValue().
  enum class Type : uint8_t {
    // No known value.
    kUndef,

    // m_context: A Context indicating that computation is still in flight.
    kContext,

    // An InvalidationWatcher waiting for an invalidation notification.
    kInvalidationWatcher,

    //
    // NOTE: the values below are considered isSkipValue():
    //

    // m_IObj: A normally returned interned Skip object. This MemoValue
    // holds a refcount on the object.
    kIObj,

    // m_IObj: A thrown Skip exception object. This MemoValue
    // holds a refcount on the object. Its backtrace extends only up
    // to the memoization point.
    kException,

    // m_IObj: An interned LongString.
    kLongString,

    // No data: A null value.
    kNull,

    // m_double: An IEEE double.
    kDouble,

    // m_int64: An int64 (or smaller integer scalar).
    kInt64,

    // m_int64: Bits of a short String.
    kShortString,

    // m_int64: Bits of a fake pointer (so isFakePtr() must be true).
    kFakePtr,
  };

  static MemoValue* fromC(SkipMemoValue* p) {
    return reinterpret_cast<MemoValue*>(p);
  }

  MemoValue() : MemoValue(Type::kUndef) {}
  explicit MemoValue(Type type);

  explicit MemoValue(Context& ctx);

  explicit MemoValue(InvalidationWatcher& watcher);

  MemoValue(IObj* iobj, Type type, bool incref = true);

  explicit MemoValue(std::nullptr_t) : MemoValue(Type::kNull) {}

  // Create a MemoValue for an integer scalar.
  explicit MemoValue(long n);
  explicit MemoValue(long long n);

  // Create a MemoValue for a fake pointer.
  MemoValue(intptr_t n, Type type);

  explicit MemoValue(int n);

  // Create a MemoValue for a double scalar.
  explicit MemoValue(double n);

  // Create a MemoValue for a String.  The String must have already been
  // interned.
  explicit MemoValue(const StringPtr& s);
  explicit MemoValue(StringPtr&& s) noexcept;

  MemoValue(MemoValue&& v) noexcept;

  MemoValue(const MemoValue& v);

  ~MemoValue();

  MemoValue& operator=(MemoValue&& v) noexcept;

  MemoValue& operator=(const MemoValue&);

  // What value does this hold?
  Type type() const;

  // Discard the value, setting it to "kUndef" state and decrefing the old
  // value if appropriate.
  void reset();

  void swap(MemoValue& v) noexcept;

  bool operator==(const MemoValue& v) const;

  bool operator!=(const MemoValue& v) const;

  // Is this a valid value for a Skip program (not undef or some internal
  // bookkeeping value)?
  bool isSkipValue() const;

  // If this holds a Context, returns it, else nullptr.
  Context* asContext() const;

  // If this holds an IObj (object or exception), return it, else nullptr.
  // The MemoValue holds one refcount for the object, and this does not
  // adjust it.
  IObj* asIObj() const;

  IObjOrFakePtr asIObjOrFakePtr() const;

  // Reset and return the underlying IObj without decrementing its refcount.
  IObj* detachIObj();

  // Reset and return the underlying InvalidationWatcher without decrementing
  // its refcount.
  InvalidationWatcher::Ptr detachInvalidationWatcher();

  // Returns the value assuming it holds int64. Else it dies.
  int64_t asInt64() const;

  // Returns the value assuming it holds double. Else it dies.
  double asDouble() const;

  bool isNull() const {
    return m_type == Type::kNull;
  }
  bool isString() const {
    return m_type == Type::kShortString || m_type == Type::kLongString;
  }
  StringPtr asString() const;

  // Raw underlying bits. Use with care!
  uint64_t bits() const {
    return static_cast<uint64_t>(m_value.m_int64);
  }

  // A tagged union holding the memoized value. m_type distinguishes.
  union Value {
    Context* m_context;
    IObj* m_IObj;
    double m_double;
    int64_t m_int64;
    InvalidationWatcher* m_invalidationWatcher;
  };

 private:
  // This is unaligned so it only takes up 9 bytes in Revision.
  Value m_value;
  Type m_type;
};

/**
 * The continuation that handles the result of an asyncEvaluate being ready.
 */
struct Caller : private skip::noncopyable {
  explicit Caller(TxnId queryTxn) : m_queryTxn(queryTxn) {}

  virtual ~Caller() = default;

  void refreshFailed() {
    m_refreshFailed = true;
  }

  virtual void addDependency(Revision& lockedValue) = 0;

  virtual void prepareForDeferredResult() {}

  virtual void retry() = 0;

  virtual void finish() = 0;

  const TxnId m_queryTxn;

  /// Next in stack or list of Callers.
  Caller* m_next = nullptr;

  bool m_refreshFailed = false;

  // The memoization process that "owns" this object.
  UnownedProcess m_owningProcess;
};

class OwnerAndFlags {
  /*
    This object is a collection of a few fields grouped into an atomic
    64-bit field. It knows how to lock this Revision and how to find the
    Invocation that owns the containing Revision (if any), as well as holding
    a few flags.

    It's complicated because it needs to delegate locking to the owning
    Invocation, while there is one, and then switch to using its own mutex
    field. It has to handle various races around locking at the same time
    that the object is being detached.

    This object is logically the following 64-bit bitfield, but stored in an
    atomic so we don't literally implement it that way.

    // A SpinLock for this Revision. We only use this once m_owner
    // becomes nullptr; until then, locking the Revision means locking
    // the owning Invocation, and therefore the entire Revision list.
    uintptr_t m_lock : 2;

    // Is this Revision attached (i.e. present in its Invocation's
    // linked list of Revisions?)
    uintptr_t m_isAttached : 1;

    // Can this Revision still be refreshed (extend its lifespan)? This
    // can only transition from true to false, and then never back to true.
    uintptr_t m_canRefresh : 1;

    // This is a reference count on the "owner" field itself. As long as
    // it is nonzero, "owner" cannot be cleared.
    //
    // This object holds one refcount on "owner", making "owner" safely
    // dereferenceable. Whichever thread drives this to
    // zero is responsible for nulling out "owner". This count exists
    // to solve the race where some thread gets a pointer to owner, but
    // owner is freed before it can dereference it (e.g. to incref),
    // thereby dereferencing freed memory.
    //
    // In C++ terms, the pair of fields (m_count, m_owner) act somewhat like
    // a std::shared_ptr<Invocation::Ptr>. As long as the
    // outer shared_ptr has a nonzero refcount, users can take new references
    // to the underlying Invocation. But once the shared_ptr count hits
    // zero, the intrusive_ptr gets decrefed, the shared_ptr becomes nullptr
    // and can no longer reach the Invocation (even though it may still be
    // alive due to other, external references to it).
    //
    // We only need one counter here per physical thread, so this is big enough.
    uintptr_t m_count : 13;

    // An Invocation* for the owner of this Revision, or nullptr if none.
    // If m_attached is true, this must not be nullptr, since every attached
    // Revision knows its owner. If detached this might still be non-null
    // for a while, until we hit a moment where no threads are trying to
    // dereference it and m_count hits zero, at which point the last thread
    // will set it nullptr, where it will stay forever.
    uintptr_t m_owner : 47;
  */

  enum : uintptr_t {
    // Bits for "m_lock", described above.
    //
    // TODO: Combine with kLockBitsMask in InternTable.cpp -- same thing.
    //
    // TODO: We could choose to overlap this with "count", as this starts
    // getting used exactly when "count" hits zero and we don't need it
    // any more. That would give "count" more range, but we don't need it.
    kLockMask = 0x3,

    // Bits for "m_isAttached", described above.
    kIsAttachedFlag = 0x4,

    // Bits for "m_canRefresh", described above.
    kCanRefreshFlag = 0x8,

    // Start of the m_count bitfield, described above.
    kFirstCountBitIndex = 4,

    // Value of the lowest bit of m_count.
    kFirstCountBit = uintptr_t{1} << kFirstCountBitIndex,

    // Start of the m_owner bitfield, described above.
    kFirstOwnerBitIndex = sizeof(uintptr_t) * 8 - detail::kMinPtrBits,
  };

  // Forward declaration.

 public:
  explicit OwnerAndFlags(Invocation* owner);

  ~OwnerAndFlags();

  /**
   * The owner of this Revision, or nullptr if detach() has been called.
   *
   * This does not require any locks to be held; however, if not locked
   * then this could become detached any time after this returns its value,
   * rendering a valid Invocation pointer but one which is no longer the owner.
   *
   * Note that this may return nullptr even if this object still
   * physically points* to the owning Invocation. Once detach() is called
   * there is officially no more owner(), but even then there
   * may still be a physical pointer to the Invocation if, for example,
   * multiple threads have indicated their intention to lock and are trying
   * to follow the Invocation pointer to find its mutex().
   */
  // TODO: Do we need this, or is owner_lck enough?
  InvocationPtr owner() const;

  /**
   * A more efficient version of owner() that only works if the
   * containing Revision is locked.
   */
  InvocationPtr owner_lck() const;

  bool canRefresh() const;

  void clearCanRefresh();

  bool isAttached() const;

  void clearIsAttached();

  /**
   * Indicate that the owning Revision has been removed from its
   * linked list.
   */
  void detach(bool canRefresh = true);

  void lock();

  void unlock();

  const void* lockManagerKey() const;

  // See Revision::convertLock.
  RevisionLock convertLock(Revision& rev, InvocationLock&& lock);

 private:
  uintptr_t currentBits() const;

  /// Drop one reference to the m_owner field.
  void decrefRef(uintptr_t bits, uintptr_t extraFlagsToClear = 0);

  /// Extract the "m_count" bitfield.
  static size_t extractCount(uintptr_t bits);

  /// Extract the "m_owner" bitfield.
  static Invocation* extractOwner(uintptr_t bits);

  union {
    std::atomic<uintptr_t> m_bits;
    SpinLock m_mutex;
  };
};

static_assert(sizeof(OwnerAndFlags) == sizeof(uintptr_t), "");

/**
 * A Revision caches a MemoValue over a [begin, end) TxnId "lifespan".
 * It serves two main purposes:
 *
 * (1) An entry in a MVCC linked list of cache values for an Invocation.
 *
 * (2) A node in a dependency graph cached values (see Edge docs).
 *
 * A Revision can outlive its Invocation, existing as a graph node
 * long after it has forgotten how it computed its value in the first place.
 */
struct alignas(kRevisionAlign) Revision final : Aligned<Revision>,
                                                LeakChecker<Revision> {
  using Ptr = boost::intrusive_ptr<Revision>;

  Revision(
      TxnId begin,
      TxnId end,
      Revision* prev,
      Revision* next,
      MemoValue value = MemoValue(),
      Invocation* owner = nullptr,
      Refcount refcount = 1);

  void verifyInvariants() const;

  void verifyInvariants_lck() const;

  /// Happens when one of our subscriptions changes.
  void invalidate(EdgeIndex index);

  /// Can this refresh its value? This can be called without a lock
  /// and can therefore transition from true -> false at any time, but
  /// will never transition false -> true.
  bool canRefresh() const;

  /// Discard the Trace and indicate that no attempt should be made to
  /// refresh this Revision. This allows some memory to be recovered.
  void preventRefresh_lck();

  void decref();

  // Private decref called by ThreadLocalLockManager when no locks are held.
  void decrefAssumingNoLocksHeld();

  void incref();

  /// Subscribe to changes. The subscriber must be locked.
  ///
  /// Atomically with subscribing, this updates the subscriber's lifespan
  /// and inactive flags at the moment the subscription was created, so
  /// that no invalidation is ever missed.
  void subscribe_lck(UpEdge subscriber);

  void unsubscribe(DownEdge edge);

  // This attempts to extend the lifespan of 'this' such that its "end"
  // encompasses caller.m_queryTxn. It may fail to do so.
  //
  // This method guarantees that one of the following things will happen,
  // either before or after this method returns:
  //
  // - success:
  //   If a value is successfully computed for caller.m_queryTxn, this will
  //   (just like Invocation::asyncEvaluate) first call caller.addDependency()
  //   with that Revision locked (so the callee should not do anything
  //   complex or reentrant), then call caller.finish() with all locks
  //   released. The Revision provided to addDependency() will be
  //   this object if it was successfully refreshed, but may be a different
  //   one, perhaps with a different value, if this Revision's lifespan
  //   was not able to be extended.
  //
  // - retry:
  //   This indicates the caller should restart its operation (the dreaded
  //   EINTR of the Skip runtime), and it calls caller.retry(). This happens
  //   when multiple threads are trying to evaluate the same Invocation at
  //   different TxnIds. We suspend all but one until the value is computed,
  //   hoping the resulting lifespan will satisfy the suspended callers too.
  //   But if doesn't they need to retry. We always make some progress
  //   whenever this happens, so it should not spin retrying forever.
  //
  // - failed: ??? what does this mean for non-refreshers?
  //
  // ??? Should this method even exist on this class, or on RefreshCaller.

  void asyncRefresh_lck(Caller& caller, RevisionLock thisLock);

  void resetMemoizedValue_lck();

  /// Pure values have no external dependencies and so can never become
  /// invalid. They do not get added to the dependency graph.
  bool isPure_lck() const;

  Refresher* refresher_lck() const;

  /// Callable by the holder of mutex().
  boost::intrusive_ptr<Invocation> owner_lck() const;

  bool isPlaceholder() const;

  Context* valueAsContext() const;

  // Calling this requires locking either mutex() OR, if still attached, the
  // owning Invocation's mutex.
  TxnId end_lck() const;

  // Calling this requires locking both mutex() AND, if still attached, the
  // owning Invocation's mutex.
  void setEnd_lck(TxnId end, int line);

  // Calling this requires locking either mutex() OR, if still attached, the
  // owning Invocation's mutex.
  TxnId begin_lck() const;

  // Calling this requires locking both mutex() AND, if still attached, the
  // owning Invocation's mutex.
  void setBegin_lck(TxnId begin);

  // Calling this requires locking either mutex() OR, if still attached, the
  // owning Invocation's mutex.
  const MemoValue& value_lck() const;

  void setValue_lck(const MemoValue& value);
  void setValue_lck(MemoValue&& value);

  bool isAttached_lck() const;

  Refcount currentRefcount() const;

  bool hasTrace_lck() const;

  // Discard the Trace (if any).
  void clearTrace_lck();

  // Create a Trace from the given inputs, and set m_begin and m_end to
  // the intersection of the lifespans found in the trace.
  //
  // NOTE: This detach()es each array entry, stealing the refcounts.
  void createTrace_lck(Revision::Ptr* inputs, size_t size);

  // Special case to create the Trace for the phony Revision in an
  // InvalidationWatcher. Does nothing if already detach()ed.
  void createInvalidationWatcherTrace(Revision& input);

  // If this holds an InvalidationWatcher, detaches and returns it and
  // clears the trace. Else returns null.
  InvalidationWatcher::Ptr detachInvalidationWatcher_lck();

  // Clear the current trace (if any) then transfer other's trace onto
  // ourself, clearing other's trace.
  void stealTrace_lck(Revision& other);

  // The OwnerAndFlags plays the role of the mutex, as it knows how to
  // delegate to the owning Invocation if needed, and how to prevent that
  // owning Invocation from being freed while holding the lock.
  OwnerAndFlags& mutex() const;

  const void* lockManagerKey() const;

  // "Downgrade" an Invocation lock on the owner to a lock on just the Revision,
  // This is not *really* a downgrade, because locking a Revision locks the
  // Invocation anyway, but sometimes we need this conversion.
  RevisionLock convertLock(InvocationLock&& lock);

 private:
  // Allow this class to access our destructor, because it embeds one.
  friend struct Context;

  friend struct TestPrivateAccess;

  // Recompute the value of m_canRefresh. Done when one of the things
  // it depends on changes.
  void recomputeCanRefresh_lck(bool allowRefresh = true);

  // private: only destroy via decrefAssumingNoLocksHeld().
  ~Revision();

 public:
  // 0 bytes
  OwnerAndFlags m_ownerAndFlags;

  // 8 bytes
  AtomicRefcount m_refcount;

  // 12 bytes
  SmallTaggedPtr<Revision, 0, true, true, true, kLog2RevisionAlign> m_prev;

  // 18 bytes
  SmallTaggedPtr<Revision, 0, true, true, true, kLog2RevisionAlign> m_next;

  // 24 bytes
  // Protected by mutex().
  SubscriptionSet m_subs;

  // 30 bytes
  // Protected by mutex().
  Trace m_trace;

 private:
  // 36 bytes
  // End of lifespan (exclusive). Read this with end_lck() and write it
  // with setEnd_lck() -- see those methods for locking details.
  typename detail::UIntTypeSelector<detail::kNumTxnIdBytes, true, true>::type
      m_end;

  // Start of lifespan (inclusive). Read this with begin_lck() and write
  // it with setBegin_lck() -- see those methods for locking details.
  typename detail::UIntTypeSelector<detail::kNumTxnIdBytes, true, true>::type
      m_begin;

  // 48 bytes
  // Read this with value_lck() and write it with setValue_lck() -- see
  // those methods for locking details.
  MemoValue m_value;

  // 57 bytes
  uint8_t m_PADDING = 0;

 public:
  // 58 bytes
  SmallTaggedPtr<Refresher, 0, true, false, true, 0> m_refresher;

  // 64 bytes
};

static_assert(sizeof(Revision) <= 64, "Revision too big; want one cache line.");

// An Invocation can be in the LRU list or some CleanupList, or neither,
// but not both. This enum tracks which kind of list it's in, although
// if it's in a CleanupList we don't actually know which one.
//
// If scalability of LRU updates ever becomes a problem, there are many
// things we can do:
//
// 1) prefetch the "prev" and "next" objects before taking the lock,
//   to reduce lock hold times. We just have to be sure that prefetching
//   can never segfault (but hopefully it resolves TLB misses when possible).
// 2) shard the LRU lists based on the hash of the Invocation's address,
// 3) use a try_lock and simply don't update LRU if the list is busy
//    (unless moving from a CleanupList).
enum class OwningList : uint8_t { kNone, kLru, kCleanup };

// The result of an asyncEvaluate() call.
struct AsyncEvaluateResult {
  // The value computed by the asyncEvaluate() call.
  MemoValue m_value;

  // Optionally, a subscription to a one-shot "invalidate" event indicating
  // when the value is no longer valid. If not subscribed, this is null.
  //
  // To be subscribed, two things must be true:
  //
  // (1) a subscription must be requested during the asyncEvaluate() call.
  // (2) the computed value must be invalidatable. If a computed value
  //     can never change (e.g. because it is the result of a pure mathematical
  //     function) then no subscription will be created and this will be null,
  //     even if a subscription was requested.
  InvalidationWatcher::Ptr m_watcher;
};

// RAII struct used to keep Revisions for an old TxnID alive.
struct MemoTask final : LeakChecker<MemoTask> {
  using Ptr = std::shared_ptr<MemoTask>;

  MemoTask(TxnId queryTxn, CleanupList& cleanupList);

  ~MemoTask();

  const TxnId m_queryTxn;
  CleanupList& m_cleanupList;
};

// Invocation is the metadata for an asynchronous function which is
// called to begin computation of a memoized value.  The function
// pointer is in the VTable's m_pFn field and the function parameters
// are in the frozen object.  When the asynchronous function is
// finished it needs to call one of the SKIP_return* functions with
// the result of the computation.
//
// While it's being invoked the current Context can be found in a
// thread-local variable via t_currentContext. This is also used to
// figure out what to do with the return value so it must be set up
// properly when SKIP_return* is eventually called.
struct Invocation final : LeakChecker<Invocation> {
  using Ptr = InvocationPtr;

 private:
  friend struct Cell;
  friend struct CleanupList;
  friend struct InvocationList;
  friend struct InvocationType;
  friend IObj& shallowCloneIntoIntern(const RObj& obj);
  friend void Type::static_invocationOnStateChange(
      IObj* obj,
      StateChangeType type);

  // Instances can only be created via Type::shallowClone() and
  // deleted via freeInternObject().
  ~Invocation();
  explicit Invocation(const VTableRef vtable);

  // So freeInternObject() can delete this.
  friend void freeInternObject(IObj& obj);

 public:
  void verifyInvariants() const;

  void verifyInvariants_lck() const;

  Refcount currentRefcount() const;

  /**
   * Clear the Revisions list, cutting the Revisions loose to surive on their
   * own if they are part of a dependency graph.
   */
  void detachRevisions_lck();

  /**
   * Discard cached state that's no longer useful given that no one will
   * call asyncEvaluate() with a queryTxn older than oldestVisible.
   */
  void cleanup();

  /**
   * Detach some member of the revision list. Does nothing if already detached.
   */
  void detach_lck(Revision& rev, bool permanent = true);

  void insertBefore_lck(Revision& rev, Revision* before);

  /**
   * Moves to the head of the LRU list, if not in a cleanup list.
   */
  void moveToLruHead_lck();

  void setOwningList_lck(OwningList list);

  // Computes the value of this invocation at caller.m_queryTxn then calls
  // caller.addDependency() with the Revision holding that value locked
  // (so the callee should not do anything complex or reentrant), then
  // finally call caller.finish() with all locks released.
  void asyncEvaluate(Caller& caller);

  // Like asyncEvaluate() but instead of calling the invocation's eval it calls
  // the passed eval.
  template <typename FN>
  void asyncEvaluateWithCustomEval(Caller& caller, FN eval);

  std::future<AsyncEvaluateResult> asyncEvaluateAndSubscribe();

  std::future<AsyncEvaluateResult> asyncEvaluate(
      MemoTask::Ptr memoTask = MemoTask::Ptr());

  std::future<AsyncEvaluateResult> asyncEvaluate(
      bool preserve,
      bool subscribeToInvalidations,
      MemoTask::Ptr memoTask = MemoTask::Ptr());

 public:
  /**
   * Given a Context that has finished computing its value, removes its
   * placeholder from the m_values list (if still present) and
   * updates the list to hold its new value, which it returns.
   *
   * See the "Linked-list maintenance algorithm" description in memoize.cpp.
   */
  std::pair<Revision::Ptr, RevisionLock> replacePlaceholder(
      Context& ctx,
      MemoValue&& value);

  Revision::Ptr insertRevision_lck(Revision::Ptr insert, bool preferExisting);

  IObj* asIObj() const;
  static Invocation& fromIObj(IObj& iobj);

  void incref();
  void decref();

  bool inList_lck() const;

  SpinLock& mutex() const;

  static const uint8_t kMetadataSize;

  mutable SpinLock m_mutex;
  bool m_isNonMvccAware = false;

  // Which list is this in, if any? Protected by mutex().
  // NOTE: We could bury this in the tag bits of m_lruPrev if we wanted,
  // or in the byte holding mutex().
  OwningList m_owningList = OwningList::kNone;

  /// Prev/next in either LRU list or cleanup list.
  /// ??? When exactly is it safe to look at these?
  ///     Probably depends on the value of m_owningList.
  SmallTaggedPtr<Invocation, 0, true, true, true, 0> m_lruPrev;
  SmallTaggedPtr<Invocation, 0, true, true, true, 0> m_lruNext;
  /// Head and tail of doubly-linked list of values, "newest" at the head.
  SmallTaggedPtr<Revision, 0, true, true> m_headValue;
  SmallTaggedPtr<Revision, 0, true, true> m_tailValue;

  // These fields are the standard metadata fields held by any IObj.
  IObjMetadata m_metadata;
};

/**
 * A Cell is a mutable value that participates in the memoization dependency
 * graph. It always has a valid value. Modify the value using Transaction.
 */
struct Cell final : LeakChecker<Cell> {
  explicit Cell(const MemoValue& initialValue);

  ~Cell();

  Invocation::Ptr invocation() const;

  SpinLock& mutex() const;

 private:
  Invocation::Ptr m_invocation;
};

/**
 * A linked list of Callers, supporting lock-free prepending.
 */
struct CallerList final : LeakChecker<CallerList> {
  CallerList();

  explicit CallerList(Caller& first);

  CallerList(CallerList&& other) noexcept;
  CallerList& operator=(CallerList&& other) noexcept;

  Caller* stealList_lck();

  /// Prepends a Caller to this list.
  void addCaller_lck(Caller& caller);

  // Whatever object owns this CallerList (Context or Revision) is
  // locked by 'lock'.
  void notifyCallers_lck(Revision& rev, RevisionLock revLock);

 private:
  friend std::ostream& operator<<(std::ostream& out, CallerList& caller);
  SmallTaggedPtr<Caller> m_callers;
};

// This needs special alignment because it contains a Revision.
struct Context final : Aligned<Context>, LeakChecker<Context> {
  Context(Invocation& owner, Caller& firstCaller, TxnId begin, TxnId end);

  // Special Context for synchronous evaluation.
  explicit Context(TxnId begin);

  ~Context();

  // Returns the current Context for this thread.
  static Context* current();

  // Changes the Context for this thread returns the previous context.
  static Context* setCurrent(Context* ctx);

  // RAII guard that changes then restores the thread-local Context.
  struct Guard : private skip::noncopyable {
    explicit Guard(Context* newContext);
    ~Guard();

   private:
    Context* const m_old;
  };

  void evaluateDone(MemoValue&& v);

  void discardCalls();

  void addCaller_lck(Caller& caller);

  void addDependency(Revision& lockedInput);

  // Turn the scrambled m_calls map into a linear array listing the
  // inputs in the order in which they were first visited.
  std::vector<Revision::Ptr> linearizeTrace();

  /// The original TxnId at which this function is being evaluated.
  /// This is also the original "begin" for its placeholder.
  const TxnId m_queryTxn;

  SpinLock& mutex() const;

 private:
  Invocation::Ptr m_owner;

  CallerList m_callers;

  // Set of every dependency seen so far, to weed out duplicates.
  // Maps the pointer to the order in which it was first seen (0, 1, 2, ...)
  // These are strong pointers (hold a reference).
  //
  // Protected by m_mutex (which is different than m_placeholder.mutex()).
  using CallSet = skip::fast_map<Revision*, size_t>;
  CallSet m_calls;

  // NOTE: This is separate from m_placeholder::mutex to simplify the lock
  // ordering rules. It only protects m_calls.
  mutable SpinLock m_mutex;

 public:
  // This is the actual entry that shows up in the Invocation linked list.
  Revision m_placeholder;
};

// Creates a vector of MemoTasks, each querying as of the same, latest TxnId.
std::vector<MemoTask::Ptr> createMemoTasks(size_t count);

// Creates one MemoTask querying as of the latest TxnId.
MemoTask::Ptr createMemoTask();

struct Transaction {
  Transaction();
  ~Transaction();

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  Transaction(Transaction&&) noexcept;
  Transaction& operator=(Transaction&&) noexcept;

  /// Enqueues an assignment to happen to be committed when this transaction
  /// is destroyed. If the same Cell is written more than once, the last
  /// write "wins".
  template <typename T>
  void assign(Cell& cell, T&& value) {
    assignMemoValue(cell, MemoValue(std::forward<T>(value)));
  }

  void assignMemoValue(Cell& cell, MemoValue&& value);
  void assignMemoValue(Invocation& inv, MemoValue&& value);

  /// Abandons any proposed changes with no side effects.
  void abort();

  std::unique_lock<std::mutex> commitWithoutUnlock(
      std::vector<InvalidationWatcher::Ptr>& invalidationWatchersToNotify);

 private:
  /// The destructor calls this implicitly.
  void commit();

  std::vector<std::pair<Invocation::Ptr, MemoValue>> m_commits;
};

// boost::intrusive_ptr support.
void intrusive_ptr_add_ref(Revision* rev);
void intrusive_ptr_release(Revision* rev);
void intrusive_ptr_add_ref(Invocation* inv);
void intrusive_ptr_release(Invocation* inv);
void intrusive_ptr_add_ref(InvalidationWatcher* watcher);
void intrusive_ptr_release(InvalidationWatcher* watcher);

/**
 * Attempt to recover memory by throwing away the least-recently used
 * Invocation (to the extent that refcounting will let us). Returns true
 * if it did something, else false, which means the LRU list is empty.
 */
bool discardLeastRecentlyUsedInvocation();

/**
 * For tests: verify that no cleanups remain.
 */
void assertNoCleanups();

/**
 * Return the newest entry in the LRU list, for testing only.
 */
Invocation::Ptr mostRecentlyUsedInvocation();

// For debugging: if only one thread is currently running, then various
// invariant assertions can be tightened because they don't need to worry
// about momentary inconsistencies that are in the process of being fixed.
extern bool g_oneThreadActive;

// Debugging/tracing support.
std::ostream& operator<<(std::ostream& out, const MemoValue& m);
std::ostream& operator<<(std::ostream& out, const Revision* r);
std::ostream& operator<<(std::ostream& out, const Revision& r);
std::ostream& operator<<(std::ostream& out, const Revision::Ptr& r);
std::ostream& operator<<(std::ostream& out, const Invocation* inv);
std::ostream& operator<<(std::ostream& out, const Invocation& inv);
std::ostream& operator<<(std::ostream& out, const Invocation::Ptr& inv);
std::ostream& operator<<(std::ostream& out, const CleanupList& cl);
std::ostream& operator<<(std::ostream& out, const Trace& trace);
std::ostream& operator<<(std::ostream& out, const SubscriptionSet& subs);

/// Dump out a human-readable description of an Invocation's Revisions.
void dumpRevisions(const Invocation& inv, bool details, std::ostream& out);
void dumpRevisions(const Invocation& inv);

// Debug output helper: allows you to print out a Revision for which you
// have a lock, so it can only dump out more info.
struct RevDetails {
  explicit RevDetails(const Revision* rev, bool emitTrace = true)
      : m_rev(rev), m_emitTrace(emitTrace) {}

  explicit RevDetails(const Revision& rev, bool emitTrace = true)
      : RevDetails(&rev, emitTrace) {}

  explicit RevDetails(Revision::Ptr rev, bool emitTrace = true)
      : RevDetails(rev.get(), emitTrace) {}

  const Revision* m_rev;
  bool m_emitTrace;
};

std::ostream& operator<<(std::ostream& out, RevDetails rev);

// Debug output helper: allows you to print out a Revision for which you
// have a lock, so it can only dump out more info.
struct InvDetails {
  explicit InvDetails(const Invocation* inv) : m_inv(inv) {}

  explicit InvDetails(const Invocation& inv) : InvDetails(&inv) {}

  explicit InvDetails(Invocation::Ptr inv) : m_inv(inv.get()) {}

  const Invocation* m_inv;
};

std::ostream& operator<<(std::ostream& out, InvDetails inv);

struct InvocationHelperBase : IObj {
 protected:
  ~InvocationHelperBase() = default;
  InvocationHelperBase() = default;

  static void static_evaluate_helper(std::future<MemoValue>&& future);
};

/**
  This is a helper class for constructing an Invocation based on C++ objects.
  It's given two classes - an ObjType class and an ArgType class.  The ObjType
  instance is mutable and can hold mutable state.  The ArgType instance is
  immutable and holds the "args" to the interned function.

  The ArgType class must provide the following minimal form:

  struct ArgType {

    // The asyncEvaluate() function is called when we need to compute a new
    // value for these args.

    // VERY IMPORTANT: The value MUST be solely dependent on the values of *this
    // and other Invocation::asyncEvaluate() results and not on any external
    // state or undefined behavior WILL result.

    // WARNING: Be very careful about having padding holes in ArgType which
    // could be uninitialized.  This could end up with your ArgType structure
    // being interned based on random input.  A helper class (ZeroOnConstruct)
    // is provided below to assist in this.

    // asyncEvaluate() should eventually (either directly or indirectly) call
    // promise.setValue().

    int64_t asyncEvaluate(std::promise<MemoValue>&& promise) const;

    // The static_offsets() function should return the offsets of any internal
    // pointers within ArgType.

    static std::vector<size_t> static_offsets();
  };


  The ObjType class must provide the following minimal form:

  struct ObjType {

    // The constructor is called when the ObjType is first interned.  Note that
    // the constructor is called while the intern table is still locked so it
    // should be VERY minimal or it will affect performance.  If ArgType is part
    // of a cycle there is no guarantee made as to the order that the nodes are
    // constructed.  There is also no guarantee that a prior ArgType's
    // Invocation will be finalized before a new Invocation is constructed -
    // this can happen if the last reference to an Invocation is dropped at the
    // same time as a new reference is attempted.

    ObjType(IObj& iobj, const ArgType& args);


    // The finalize() function is called when the ObjType is being destroyed.
    // Note that there is no ordering guarantees made so a new object with the
    // same ArgType value may have already been constructed before this object
    // is destroyed.  The only difference between the finalize() call and the
    // subsequent destructor call is that finalize() is passed the iobj and
    // args.

    void finalize(IObj& iobj, const ArgType& args);
  };

**/
template <typename _ObjType, typename _ArgType>
struct InvocationHelper : InvocationHelperBase {
  using ArgType = _ArgType;
  using ObjType = _ObjType;
  using Ptr = boost::intrusive_ptr<InvocationHelper<ObjType, ArgType>>;

  static boost::intrusive_ptr<const InvocationHelper> factory(ArgType&& args) {
    auto vtable = static_vtable();
    auto& type = static_type();

    const size_t metadataSize = type.uninternedMetadataByteSize();
    const size_t size = metadataSize + type.userByteSize();

    auto raw = ::calloc(1, size);
    if (raw == nullptr)
      fatal("out of memory");

    auto ptr = static_cast<RObj*>(mem::add(raw, metadataSize));
    new (&ptr->metadata()) RObjMetadata(vtable);

    auto ret = new (ptr) InvocationHelperRObj(std::move(args));
    auto deleter = [&](InvocationHelperRObj* obj) mutable {
      obj->~InvocationHelperRObj();
      ::free(raw);
    };
    auto robj = std::unique_ptr<InvocationHelperRObj, decltype(deleter)>(
        ret, std::move(deleter));

    auto iobj = boost::intrusive_ptr<const InvocationHelper>(
        reinterpret_cast<const InvocationHelper*>(intern(robj.get())), false);

    return iobj;
  }

  const ArgType& asArgType() const {
    return iobjToArgType(*this);
  }

  ObjType& asObjType() const {
    return iobjToObjType(*this);
  }

  static InvocationHelper<ObjType, ArgType>& fromObjType(ObjType& obj) {
    return *reinterpret_cast<InvocationHelper<ObjType, ArgType>*>(
        mem::add(&obj, static_type().internedMetadataByteSize()));
  }

  static const VTableRef static_vtable() {
    static const auto singleton = RuntimeVTable::factory(
        static_type(), reinterpret_cast<VTable::FunctionPtr>(static_evaluate));
    static const VTableRef vtable{singleton->vtable()};
    return vtable;
  }

 private:
  ArgType m_args;

  ~InvocationHelper() = default;

  explicit InvocationHelper(const ArgType& args) : m_args(args) {}

  struct InvocationHelperRObj : RObj {
    ArgType m_args;

    explicit InvocationHelperRObj(const ArgType& args) : m_args(args) {}
  };

  static std::vector<size_t> computeOffsets() {
    // We want ArgType::static_offsets() to have its offsets relative to itself
    // and not have to worry about the InvBody - but that means we need to
    // offset them here.
    std::vector<size_t> offsets;
    for (auto i : ArgType::static_offsets()) {
      offsets.push_back(i + offsetof(InvocationHelper, m_args));
    }
    return offsets;
  }

  static Type& static_type() {
    static const std::unique_ptr<Type> singleton = Type::invocationFactory(
        typeid(InvocationHelper).name(),
        sizeof(InvocationHelper),
        computeOffsets(),
        roundUp(sizeof(ObjType), alignof(Invocation)),
        &InvocationHelper::static_onStateChange);
    return *singleton;
  }

  static ObjType& iobjToObjType(IObj& obj) {
    return *static_cast<ObjType*>(const_cast<void*>(
        mem::sub(&obj, obj.type().internedMetadataByteSize())));
  }

  static const ArgType& iobjToArgType(IObj& obj) {
    return *static_cast<const ArgType*>(
        mem::add(&obj, offsetof(InvocationHelper, m_args)));
  }

  static Awaitable* static_evaluate(IObj* obj) {
    auto& argType = iobjToArgType(*obj);
    auto& objType = iobjToObjType(*obj);

    std::promise<MemoValue> promise;
    static_evaluate_helper(promise.get_future());
    try {
      argType.asyncEvaluate(objType, std::move(promise));
    } catch (const std::exception& e) {
      // TODO: For some reason if I just pass 'e' then it just returns the
      // exception type instead of the exception message.
      promise.set_exception(make_exception_ptr(std::runtime_error(e.what())));
    }

    return nullptr;
  }

  static void static_onStateChange(IObj* obj, Type::StateChangeType type) {
    auto& objType = iobjToObjType(*obj);
    auto& argType = iobjToArgType(*obj);
    switch (type) {
      case Type::StateChangeType::initialize: {
        new (&objType) ObjType(*obj, argType);
        break;
      }

      case Type::StateChangeType::finalize: {
        objType.finalize(*obj, argType);
        objType.~ObjType();
        break;
      }
    }

    Type::static_invocationOnStateChange(obj, type);
  }
};

// This is an evil helper class which zeros out its contents.  It can be used as
// the first base class which will force the class to zero all its memory before
// actually writing any values.  Note that this is almost certainly dangerous in
// the presence of vtables.  This is useful to use with interned classes which
// require that they have no uninitialized data.
template <typename Derived>
struct alignas(Obstack::kAllocAlign) ZeroOnConstruct {
  ZeroOnConstruct() {
    static_assert(std::is_final<Derived>(), "Derived class must be final");
    static_assert(
        sizeof(Derived) % Obstack::kAllocAlign == 0,
        "Derived class must be multiple of pointer size");
    memset(this, 0, sizeof(Derived));
  }

  ZeroOnConstruct(const ZeroOnConstruct& o) {
    memset(this, 0, sizeof(Derived));
  }

  // memset() here could be a disaster - this is already initialized with
  // structured values.
  ZeroOnConstruct& operator=(const ZeroOnConstruct& o) = default;

  ZeroOnConstruct(ZeroOnConstruct&& o) noexcept {
    memset(this, 0, sizeof(Derived));
  }

  ZeroOnConstruct& operator=(ZeroOnConstruct&& o) noexcept {
    // memset() here could be a disaster - this is already initialized with
    // structured values.
    return *this;
  }
};
} // namespace skip

namespace std {
template <>
struct hash<boost::intrusive_ptr<const skip::MutableIObj>> {
  size_t operator()(boost::intrusive_ptr<skip::IObj> p) const noexcept {
    return p->hash();
  }
};

template <>
struct hash<skip::Invocation::Ptr> {
  size_t operator()(skip::Invocation::Ptr p) const noexcept {
    return p->asIObj()->hash();
  }
};
} // namespace std

namespace skip {
struct MemoSerde {
  // Memoizer Serializer/Deserializer helpers
  static void serializeMemoCache(std::ostream& f, TxnId txnId);
  static void deserializeMemoCache(std::istream& f);

  static void serializeMemoCache(const std::string& filename);
  static void deserializeMemoCache(const std::string& filename);
};
} // namespace skip
