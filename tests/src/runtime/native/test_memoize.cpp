/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <thread>

#include <boost/algorithm/string/predicate.hpp>

#include <folly/executors/GlobalExecutor.h>
#include <folly/futures/Future.h>

#include <gtest/gtest.h>

#include "skip/Async.h"
#include "skip/leak.h"
#include "skip/LockManager.h"
#include "skip/memoize.h"
#include "skip/Obstack.h"
#include "skip/Process.h"
#include "testutil.h"

using namespace skip;
using namespace skip::test;

// clang-format off
ALLOW_PRIVATE_ACCESS{

static size_t countSubArrays(Revision* r) {
  size_t count = 0;
  for (auto a = r->m_subs.m_subs.asSubArray(); a != nullptr; a = a->m_next) {
    ++count;
  }

  return count;
}

};
// clang-format on

namespace {
size_t s_numTimesPlusNineCalled;

// A hacky barrier class that's Skip-event-loop aware. See T31331907.
struct SkipBarrier : boost::noncopyable {
  explicit SkipBarrier(std::vector<UnownedProcess>&& processes)
      : m_processes(std::move(processes)),
        m_count(m_processes.size()),
        m_generation(0) {}

  void wait() {
    const auto startGeneration = m_generation.load(std::memory_order_relaxed);

    if (--m_count != 0) {
      while (m_generation == startGeneration) {
        Process::cur()->runExactlyOneTaskSleepingIfNecessary();
      }
    } else {
      // We are the last thread to hit the barrier, so reset it and notify
      // everyone.
      m_count = m_processes.size();
      ++m_generation;

      // Wake up every thread in runExactlyOneTaskSleepingIfNecessary.
      for (auto& p : m_processes) {
        p.schedule([]() {});
      }
    }

    // It's possible we might see the generation increment without actually
    // running the callback posted for us (this always happens for the last
    // thread to the barrier, but could happen for others). So to be ecological
    // just run all pending tasks to ensure we discard them.
    Process::cur()->runReadyTasks();
  }

 private:
  std::vector<UnownedProcess> m_processes;
  std::atomic<size_t> m_count;
  std::atomic<size_t> m_generation;
};
} // namespace

// An Invocation that adds nine to single int64_t field.
template <typename Base>
struct Plus9Invocation : Base {
  int64_t n;

  static const VTableRef static_vtable() {
    static auto singleton = RuntimeVTable::factory(
        static_type(), reinterpret_cast<VTable::FunctionPtr>(plusNine));
    static auto vtable = VTableRef(singleton->vtable());
    return vtable;
  }

  static Type& static_type() {
    // Compute lazily to avoid the "static initialization order fiasco".
    static auto t = createInvocationType(
        typeid(Plus9Invocation).name(),
        sizeof(Plus9Invocation),
        {},
        comparator);
    return *t;
  }

  static bool comparator(const RObj& i1, const RObj& i2, RObjPairSet&) {
    // The caller ensured this is legal.
    auto& t1 = static_cast<const Plus9Invocation&>(i1);
    auto& t2 = static_cast<const Plus9Invocation&>(i2);

    // Compare the fields.
    return t1.vtable().getFunctionPtr() == t2.vtable().getFunctionPtr() &&
        t1.n == t2.n;
  }

 protected:
  ~Plus9Invocation() = default;
  explicit Plus9Invocation(int64_t arg) : n(arg) {}

 private:
  static Awaitable* plusNine(const IObj* fields) {
    auto args = reinterpret_cast<const Plus9Invocation*>(fields);
    ++s_numTimesPlusNineCalled;
    SkipInt result = args->n + 9;
    Context::current()->evaluateDone(MemoValue(result));
    return nullptr;
  }
};

struct RPlus9Invocation;

// An IPlus9Invocation is a Plus9Invocation which inherits from IObj
struct MutableIPlus9Invocation final : Plus9Invocation<IObj>,
                                       TestIObjHelper<MutableIPlus9Invocation> {
  RPlus9Invocation* asRObj();
};

using IPlus9Invocation = const MutableIPlus9Invocation;
using IPlus9InvocationPtr = boost::intrusive_ptr<IPlus9Invocation>;

// A RPlus9Invocation is a Plus9Invocation which inherits from RObj
struct RPlus9Invocation final
    : Plus9Invocation<RObj>,
      TestRObjHelper<RPlus9Invocation, IPlus9Invocation> {
  explicit RPlus9Invocation(int64_t arg) : Plus9Invocation(arg) {}
};

static Revision::Ptr makeRevision(TxnId end = kNeverTxnId) {
  auto ret = Revision::Ptr(new Revision(1, end, nullptr, nullptr), false);
  EXPECT_EQ(ret->currentRefcount(), 1U);
  return ret;
}

static std::vector<UpEdge> getSubscriptions(Revision* r) {
  return std::vector<UpEdge>(r->m_subs.begin(), r->m_subs.end());
}

static void flattenOutputsAux(Revision* r, std::vector<UpEdge>& outputs) {
  for (auto e : getSubscriptions(r)) {
    Revision* target = e.target();

    if (getSubscriptions(target).empty()) {
      // Target is a leaf. Record it.
      outputs.push_back(e);
    } else {
      // Recurse.
      flattenOutputsAux(target, outputs);
    }
  }
}

static std::vector<UpEdge> flattenOutputs(Revision* r) {
  std::vector<UpEdge> outputs;
  flattenOutputsAux(r, outputs);
  return outputs;
}

static void flattenInputsAux(Revision* r, std::vector<DownEdge>& inputs) {
  for (size_t i = 0, size = r->m_trace.size(); i < size; ++i) {
    auto e = r->m_trace[i];
    Revision* target = e.target();
    if (target->m_trace.empty()) {
      // Target is a leaf. Record it.
      inputs.push_back(e);
    } else {
      // Recurse.
      flattenInputsAux(target, inputs);
    }
  }
}

static std::vector<DownEdge> flattenInputs(Revision* r) {
  std::vector<DownEdge> inputs;
  flattenInputsAux(r, inputs);
  return inputs;
}

static int64_t rawGet(Invocation::Ptr inv, TxnId txn) {
  for (auto rev = inv->m_headValue.ptr(); rev; rev = rev->m_next.ptr()) {
    auto lock = lockify(*rev);
    if (rev->begin_lck() <= txn && rev->end_lck() > txn) {
      return rev->value_lck().asInt64();
    }
  }

  fatal("Could not find value at txn");
}

struct MemoizeFixture : ::testing::Test {
 protected:
  MemoizeFixture() {
    g_oneThreadActive = true;
  }

  ~MemoizeFixture() override {
    // Kill off any circular dependences in the Invocation lists.
    createMemoTask();
    assertNoCleanups();
    purgeLruList();

    assertLeakCountersZero();
  }
};

// Make sure Revision and SubArray get allocated with the expected alignment.
TEST_F(MemoizeFixture, testAlignment) {
  constexpr int count = 100;

  // Create 100 Revisions.
  Revision::Ptr revs[count];

  for (size_t i = 0; i < count; ++i) {
    auto r = makeRevision();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(r.get()) % kRevisionAlign, 0U);
    revs[i] = r;
  }

  // Create 100 SubArrays.
  {
    std::unique_ptr<SubArray> arrays[count];

    for (size_t i = 0; i < count; ++i) {
      auto a = new SubArray(*revs[i], nullptr);
      EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % kSubArrayAlign, 0U);
      arrays[i].reset(a);
    }
  }

  // Create all possible TraceArrays.
  {
    std::vector<std::unique_ptr<TraceArray>> arrays;

    for (size_t i = 2; i <= kMaxTraceSize; ++i) {
      auto a = TraceArray::make(i);
      EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % kTraceArrayAlign, 0U);
      arrays.emplace_back(a);
    }
  }
}

// This tests creating 'numChildren' Revisions and then creating
// 'numParents' Revisions that subscribe to all them.
static void doSubscriptionTest(size_t numParents, size_t numChildren) {
  EXPECT_EQ(purgeLruList(), 0U);

  std::vector<Revision::Ptr> children(numChildren);

  for (size_t i = 0; i < numChildren; ++i) {
    children[i] = makeRevision((i & 1) ? 2 : kNeverTxnId);
  }

  size_t maxParentsSubscribedTo = 0;

  // Try creating and removing parents multiple times.
  for (int j = 0; j < 3; ++j) {
    for (auto& child : children) {
      child->verifyInvariants();
    }

    std::vector<Revision::Ptr> parents;

    for (size_t i = 0; i < numParents; ++i) {
      // Each child should have a reference from the 'children' array and
      // one from each parent in 'parents'.
      for (auto& child : children) {
        EXPECT_EQ(child->currentRefcount(), 1 + parents.size());
      }

      // Create a new parent.
      parents.push_back(makeRevision());
      auto& parent = parents.back();

      if (numChildren > 0) {
        // createTrace_lck clobbers the array, so copy it.
        auto childrenCopy = children;

        // Subscribe it to the children.
        auto lock = lockify(*parent);
        parent->createTrace_lck(childrenCopy.data(), numChildren);
      }

      maxParentsSubscribedTo = std::max(maxParentsSubscribedTo, parents.size());

      // We should end up with strong references doing down the graph.
      EXPECT_EQ(parent->currentRefcount(), 1U);
      for (auto& child : children) {
        EXPECT_EQ(child->currentRefcount(), 1 + parents.size());

        // Make sure we find the expected number of SubArrays, i.e. that
        // freelist reuse is working properly.
        size_t numSubArrays = PRIVATE::countSubArrays(child.get());
        if (maxParentsSubscribedTo <= 1) {
          EXPECT_EQ(numSubArrays, 0U);
        } else {
          EXPECT_EQ(
              numSubArrays, maxParentsSubscribedTo / SubArray::size() + 1);
        }
      }

      if (numChildren <= kMaxTraceSize) {
        EXPECT_EQ(parent->m_trace.size(), numChildren);
      }

      // The trace should have the same contents as the inputs array.
      auto deps = flattenInputs(parent.get());
      EXPECT_EQ(deps.size(), numChildren);
      for (size_t k = 0; k < numChildren; ++k) {
        auto input = deps[k];
        EXPECT_EQ(input.target(), children[k]);

        if (numParents == 1 || (parents.size() == 1 && j == 0)) {
          // The first ever subscription goes in the special slot.
          EXPECT_EQ(input.index(), DownEdge::kInlineSubscriptionIndex);
        } else {
          EXPECT_EQ(input.index(), parents.size() % SubArray::size());
        }
      }

      // Create an array of all the parents we expect each child to have.
      std::vector<Revision*> expectedOutputs;
      for (auto& p : parents) {
        expectedOutputs.push_back(p.get());
      }
      std::sort(expectedOutputs.begin(), expectedOutputs.end());

      // To make the test faster, we only check some children
      // (by advancing through the array quadratically). They should all
      // be identical anyway.
      for (size_t k = 0, stride = 0; k < numChildren; k += ++stride) {
        auto& child = children[k];

        auto flattened = flattenOutputs(child.get());
        std::vector<Revision*> actualOutputs(flattened.size());
        size_t outIndex = 0;
        for (auto out : flattened) {
          actualOutputs[outIndex++] = out.target();
        }
        std::sort(actualOutputs.begin(), actualOutputs.end());

        EXPECT_EQ(actualOutputs, expectedOutputs);
      }
    }

    // Remove parents one by one and make sure the children update properly.
    for (size_t i = 0, stride = 0;; i += ++stride) {
      if (numChildren != 0) {
        // To make the test faster, we only check some children
        auto& child = children[i % numChildren];

        child->verifyInvariants();

        // Freeing the trace should have decref'd the child back to 1.
        EXPECT_EQ(child->currentRefcount(), 1 + parents.size());

        // Destroying the parent should have freed its trace and unsubscribed.
        EXPECT_EQ(getSubscriptions(child.get()).size(), parents.size());
      }

      if (parents.empty()) {
        break;
      }

      parents.back()->verifyInvariants();
      parents.pop_back();
    }
  }
}

TEST_F(MemoizeFixture, testSubscribe) {
  // Try one parent subscribing to various numbers of children.
  const std::vector<size_t> numChildrenTestCases = {
      0,
      1,
      2,
      10,
      kMaxTraceSize - 1,
      kMaxTraceSize,
      kMaxTraceSize + 1,
      kMaxTraceSize * kMaxTraceSize - 1,
      kMaxTraceSize * kMaxTraceSize,
      kMaxTraceSize * kMaxTraceSize + 1,
      (kMaxTraceSize + 1) * (kMaxTraceSize + 1)};

  const std::vector<size_t> numParentsTestCases = {
      1, 2, SubArray::size(), SubArray::size() * 3 + 1};

  for (auto numParents : numParentsTestCases) {
    for (auto numChildren : numChildrenTestCases) {
      doSubscriptionTest(numParents, numChildren);
    }
  }
}

// Evaluate a call to a Plus9Invocation
static void invokePlus9(Invocation* inv, int64_t expected) {
  inv->verifyInvariants();

  auto f = inv->asyncEvaluate();

  // This call is synchronous so it should be ready right now.
  EXPECT_TRUE(f.isReady());

  // Ensure that "plusNine" added 9 to its argument.
  EXPECT_EQ(f.value().m_value, MemoValue(expected + 9));

  inv->verifyInvariants();

  // The Invocation should be in the LRU list.
  EXPECT_EQ(inv->m_owningList, OwningList::kLru);

  // There should be exactly one Revision in the list.
  auto r = inv->m_headValue.ptr();
  {
    auto lock = lockify(r);
    EXPECT_EQ(r, inv->m_tailValue.ptr());
    EXPECT_EQ(r->begin_lck(), 0U);
    EXPECT_EQ(r->end_lck(), kNeverTxnId);
    EXPECT_TRUE(r->m_trace.empty());
    EXPECT_TRUE(r->m_subs.obviouslyEmpty());
  }

  inv->verifyInvariants();
}

TEST_F(MemoizeFixture, testSimpleMemoize) {
  EXPECT_EQ(purgeLruList(), 0U);

  int64_t arg = 37;

  auto iobj = newTestInstance<RPlus9Invocation>(arg)->intern();
  auto inv = &Invocation::fromIObj(*iobj);

  for (size_t j = 0; j < 2; ++j) {
    for (size_t i = 0; i < 3; ++i) {
      invokePlus9(inv, arg);
    }

    // Even though we invoked it 3 times, the second two calls should be cached.
    // When we loop back around (j == 1) it will be removed from the cache
    // and have to get run again.
    EXPECT_EQ(s_numTimesPlusNineCalled, j + 1);

    auto numPurged = purgeLruList();
    EXPECT_EQ(numPurged, 1U);

    // Ripping out of the LRU list should have cleared its versions.
    EXPECT_EQ(inv->m_headValue, nullptr);

    EXPECT_FALSE(inv->inList_lck());
    inv->verifyInvariants();
  }

  purgeLruList();
}

namespace SimpleInvalidate {
// Create a simple value and a node which depends on that value and make sure
// that if we invalidate the child and ask for the parent value we get evaluated
// properly.

int64_t s_externalValue = 0;

size_t s_childEvaluateCalled = 0;
size_t s_parentEvaluateCalled = 0;

struct Child {
  struct Args {
    int m_value;
    explicit Args(int value) : m_value(value) {}

    void asyncEvaluate(Child& /*obj*/, folly::Promise<MemoValue>&& promise)
        const {
      s_childEvaluateCalled++;

      // Pretend to be driven by some external source by sleeping for 10
      // milliseconds and then returning our result.
      folly::futures::sleep(std::chrono::milliseconds(10))
          .via(folly::getCPUExecutor().get())
          .thenValue([this, promise = std::move(promise)](
                         folly::Unit /*unit*/) mutable {
            int64_t result = m_value + s_externalValue;
            promise.setValue(MemoValue(result));
          });
    }

    static std::vector<size_t> static_offsets() {
      return {};
    }
  };

  Child(IObj& /*iobj*/, const Args& /*args*/) {}
  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}
};

struct Parent {
  struct Args {
    int64_t m_value;
    IObj* m_child;

    // As a convenience we're passing the child IObj* in as a parameter.
    // Normally we'd look this up at runtime.
    Args(int64_t value, IObj* child) : m_value(value), m_child(child) {}

    void asyncEvaluate(Parent& /*obj*/, folly::Promise<MemoValue>&& promise)
        const {
      s_parentEvaluateCalled++;

      auto& childInv = Invocation::fromIObj(*m_child);

      // Our value is directly based on our child's value.
      childInv.asyncEvaluate().thenValue(
          [this, promise = std::move(promise)](AsyncEvaluateResult v) mutable {
            int64_t result = v.m_value.asInt64() + m_value;
            promise.setValue(MemoValue(result));
          });
    }

    static std::vector<size_t> static_offsets() {
      return {offsetof(Args, m_child)};
    }
  };

  Parent(IObj& /*iobj*/, const Args& /*args*/) {}
  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}
};

static int64_t evaluateParent(
    Invocation& parentInv,
    InvalidationWatcher::Ptr* watcher = nullptr) {
  int64_t parentValue;

  std::mutex mutex;
  std::condition_variable cond;
  bool ready = false;

  // Asynchronously ask the parent for its value...
  parentInv.asyncEvaluate(true, watcher != nullptr)
      .thenValue([&](AsyncEvaluateResult value) {
        std::unique_lock<std::mutex> lock(mutex);
        parentValue = value.m_value.asInt64();
        if (watcher != nullptr && value.m_watcher) {
          *watcher = std::move(value.m_watcher);
        };
        ready = true;
        cond.notify_all();
      });

  // ... and wait for the result to come back.
  std::unique_lock<std::mutex> lock(mutex);
  cond.wait(lock, [&] { return ready; });

  return parentValue;
}

static void testSimpleInvalidateHelper(bool subscribe, bool unsubscribe) {
  ProcessContextSwitcher guard{Process::make()};

  s_childEvaluateCalled = 0;
  s_parentEvaluateCalled = 0;
  s_externalValue = 0;

  // Create our child.
  auto child = InvocationHelper<Child, Child::Args>::factory(Child::Args(42));
  auto& childInv = Invocation::fromIObj(*child);
  // Mark the child as non-MVCC aware so we can invalidate it at will.  If we
  // don't do this the runtime will realize that the child has no dependencies
  // and just assume its value never changes.
  childInv.m_isNonMvccAware = true;

  // Create our parent
  auto parent = InvocationHelper<Parent, Parent::Args>::factory(
      Parent::Args(314, child.get()));
  auto& parentInv = Invocation::fromIObj(*parent);

  EXPECT_EQ(s_childEvaluateCalled, 0U);
  EXPECT_EQ(s_parentEvaluateCalled, 0U);

  // The child has no Revisions so this should be a no-op.
  {
    Transaction txn;
    txn.assignMemoValue(childInv, MemoValue());
  }

  bool notifiedOfInvalidate = false;
  InvalidationWatcher::Ptr watcher;

  EXPECT_EQ(
      evaluateParent(parentInv, (subscribe ? &watcher : nullptr)),
      42 + 314 + 0);

  if (watcher) {
    watcher->getFuture().thenValue(
        [&notifiedOfInvalidate](folly::Unit) { notifiedOfInvalidate = true; });
  }

  EXPECT_EQ(s_childEvaluateCalled, 1U);
  EXPECT_EQ(s_parentEvaluateCalled, 1U);
  EXPECT_FALSE(notifiedOfInvalidate);

  // Nothing should change because we didn't invalidate the child.
  EXPECT_EQ(evaluateParent(parentInv), 42 + 314 + 0);
  EXPECT_EQ(s_childEvaluateCalled, 1U);
  EXPECT_EQ(s_parentEvaluateCalled, 1U);
  EXPECT_FALSE(notifiedOfInvalidate);

  if (watcher && unsubscribe) {
    watcher->unsubscribe();
  }

  // Invalidate the child.
  {
    Transaction txn;
    txn.assignMemoValue(childInv, MemoValue());
    s_externalValue = 1;
  }

  // Now the values should change.
  EXPECT_EQ(notifiedOfInvalidate, subscribe && !unsubscribe);
  EXPECT_EQ(evaluateParent(parentInv), 42 + 314 + 1);
  EXPECT_EQ(s_childEvaluateCalled, 2U);
  EXPECT_EQ(s_parentEvaluateCalled, 2U);
}

TEST_F(MemoizeFixture, testSimpleInvalidate) {
  testSimpleInvalidateHelper(false, false);
}

TEST_F(MemoizeFixture, testInvalidateWatcher) {
  testSimpleInvalidateHelper(true, false);
}

TEST_F(MemoizeFixture, testInvalidateWatcherWithUnsubscribe) {
  testSimpleInvalidateHelper(true, true);
}

// Test a couple of other simple but weird situations with invalidation
TEST_F(MemoizeFixture, testSimpleInvalidate2) {
  // Create our child.
  auto child = InvocationHelper<Child, Child::Args>::factory(Child::Args(42));
  auto& childInv = Invocation::fromIObj(*child);
  childInv.m_isNonMvccAware = true;

  {
    Transaction txn;
    txn.assignMemoValue(childInv, MemoValue(7));
  }
}
} // namespace SimpleInvalidate

// Return the entire LRU list as an array, in an evil, non-threadsafe way.
static std::vector<Invocation::Ptr> extractLruList() {
  std::vector<Invocation::Ptr> ret;
  auto p = mostRecentlyUsedInvocation();
  if (p) {
    ret.push_back(p);

    // The first entry is preceded by the same sentinel that ends the list.
    auto sentinel = p->m_lruPrev.ptr();

    auto prev = p.get();

    for (Invocation* inv = p->m_lruNext.ptr(); inv != sentinel;
         inv = inv->m_lruNext.ptr()) {
      // Make sure list is linked properly.
      EXPECT_EQ(inv->m_lruPrev.ptr(), prev);
      prev = inv;

      ret.emplace_back(inv);
    }

    EXPECT_EQ(sentinel->m_lruPrev.ptr(), prev);
  }

  for (auto& r : ret) {
    EXPECT_EQ(r->m_owningList, OwningList::kLru);
  }

  return ret;
}

TEST_F(MemoizeFixture, testLru) {
  EXPECT_EQ(purgeLruList(), 0U);
  EXPECT_EQ(mostRecentlyUsedInvocation(), nullptr);

  // Set up four test calls.
  std::vector<IPlus9Invocation::Ptr> iobjs(4);
  std::vector<Invocation*> objs(iobjs.size());
  for (size_t i = 0; i < iobjs.size(); ++i) {
    iobjs[i] = newTestInstance<RPlus9Invocation>(i)->intern();
    objs[i] = &Invocation::fromIObj(*iobjs[i]);
  }

  // Nothing has been invoked yet, so there should be nothing in LRU.
  EXPECT_EQ(mostRecentlyUsedInvocation(), nullptr);

  // 1 for being in iobjs plus 1 from Revision plus 1 for being in LRU.
  constexpr uint32_t expectedRefcount = 1 + 1 + 1;

  for (size_t i = 0; i < objs.size(); ++i) {
    // It should start with a refcount of 1 from the iobjs list.
    EXPECT_REFCOUNT(iobjs[i], 1U);

    invokePlus9(objs[i], i);

    // 1 for being in iobjs plus 1 for Revision plus 1 for being in LRU.
    EXPECT_REFCOUNT(iobjs[i], expectedRefcount);

    // Make sure the LRU list is in the right order.
    auto list = extractLruList();
    EXPECT_EQ(list.size(), i + 1);
    for (size_t j = 0; j <= i; ++j) {
      EXPECT_EQ(list[j], objs[i - j]);
    }
  }

  for (auto& obj : objs) {
    EXPECT_REFCOUNT(obj->asIObj(), expectedRefcount);
  }

  {
    LameRandom rand(999);

    auto list = extractLruList();

    for (auto& obj : list) {
      // Plus 1 more for being in 'list'.
      EXPECT_REFCOUNT(obj->asIObj(), expectedRefcount + 1);
    }

    // Try running them in scrambled order and make sure the last one run is
    // moved to the head of the LRU.
    for (size_t i = 0; i < 100; ++i) {
      // Pick a list entry at random to invoke.
      auto index = rand.next(list.size());
      auto obj = list[index].get();

      invokePlus9(obj, std::find(objs.begin(), objs.end(), obj) - objs.begin());

      // Manually update the LRU list as we would expect, moving the entry
      // at 'index' to the head and sliding everything else over to make room.
      std::rotate(list.begin(), list.begin() + index, list.begin() + index + 1);

      EXPECT_EQ(list, extractLruList());

      // Plus 1 more for being in 'list'.
      EXPECT_REFCOUNT(obj->asIObj(), expectedRefcount + 1);
    }
  }

  for (auto obj : objs) {
    EXPECT_REFCOUNT(obj->asIObj(), expectedRefcount);
  }

  // Purging LRU will drop both the LRU ref and the Revision, which is
  // another ref.
  auto numPurged = purgeLruList();
  EXPECT_EQ(numPurged, objs.size());

  for (auto obj : objs) {
    // Only 'list' remains.
    EXPECT_REFCOUNT(obj->asIObj(), 1U);
  }
}

namespace {
size_t s_numTimesSumCalled;
}

// This is the type of Invocations that take a single int64_t argument.
template <typename Base>
struct SumInvocation : Base {
  // These must both be Invocations.  SumInvocation sums their values.
  // They are IObj so that the GC can trace these pointers.
  IObj* m_arg1;
  IObj* m_arg2;

  // This is an arbitrary value that can be used by the test to prevent
  // otherwise identical-looking SumInvocations from being interned together.
  size_t m_distinguisher;

  static const VTableRef static_vtable() {
    static auto singleton = RuntimeVTable::factory(
        static_type(), reinterpret_cast<VTable::FunctionPtr>(sumTwoInputs));
    static auto vtable = VTableRef(singleton->vtable());
    return vtable;
  }

  static Type& static_type() {
    // Compute lazily to avoid the "static initialization order fiasco".
    static auto t = createInvocationType(
        typeid(SumInvocation).name(),
        sizeof(SumInvocation),
        {0, sizeof(Invocation*)},
        comparator);
    return *t;
  }

  static bool comparator(const RObj& i1, const RObj& i2, RObjPairSet&) {
    // The caller ensured this is legal.
    auto& t1 = static_cast<const SumInvocation&>(i1);
    auto& t2 = static_cast<const SumInvocation&>(i2);

    // Compare the fields.
    return (
        t1.vtable().getFunctionPtr() == t2.vtable().getFunctionPtr() &&
        t1.m_arg1 == t2.m_arg1 && t1.m_arg2 == t2.m_arg2 &&
        t1.m_distinguisher == t2.m_distinguisher);
  }

 protected:
  ~SumInvocation() = default;
  SumInvocation(Invocation& arg1, Invocation& arg2, size_t distinguisher = 0)
      : m_arg1(arg1.asIObj()),
        m_arg2(arg2.asIObj()),
        m_distinguisher(distinguisher) {}

 private:
  static Awaitable* sumTwoInputs(const IObj* fields) {
    ++s_numTimesSumCalled;

    auto args = reinterpret_cast<const SumInvocation*>(fields);

    auto ctx = Context::current();

    auto future1 = Invocation::fromIObj(*args->m_arg1).asyncEvaluate();
    auto future2 = Invocation::fromIObj(*args->m_arg2).asyncEvaluate();

    collect(future1, future2)
        .thenValue(
            [ctx](
                const std::tuple<AsyncEvaluateResult, AsyncEvaluateResult>& t) {
              SkipInt result =
                  (std::get<0>(t).m_value.asInt64() +
                   std::get<1>(t).m_value.asInt64());
              ctx->evaluateDone(MemoValue(result));
            });

    return nullptr;
  }
};

struct RSumInvocation;

// An ISumInvocation is a SumInvocation which inherits from IObj
struct MutableISumInvocation final : SumInvocation<IObj>,
                                     TestIObjHelper<MutableISumInvocation> {
  RSumInvocation* asRObj();
};

using ISumInvocation = const MutableISumInvocation;

// A RSumInvocation is a SumInvocation which inherits from RObj
struct RSumInvocation final : SumInvocation<RObj>,
                              TestRObjHelper<RSumInvocation, ISumInvocation> {
  RSumInvocation(Invocation& arg1, Invocation& arg2, size_t distinguisher = 0)
      : SumInvocation(arg1, arg2, distinguisher) {}
};

TEST_F(MemoizeFixture, testLifespanIntersection) {
  const auto startNumTimesSumCalled = s_numTimesSumCalled;
  MemoTask::Ptr memoTask = createMemoTask();
  const TxnId startTxn = memoTask->m_queryTxn;

  const int64_t n1 = 111;
  const int64_t n2 = 333;

  Cell c1{MemoValue(n1)};
  Cell c2{MemoValue(n2)};

  // Cells do not go in LRU.
  EXPECT_EQ(purgeLruList(), 0U);

  auto iobj =
      newTestInstance<RSumInvocation>(*c1.invocation(), *c2.invocation())
          ->intern();
  auto inv = &Invocation::fromIObj(*iobj);

  auto f = inv->asyncEvaluate();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 1);
  EXPECT_EQ(f.value().m_value, MemoValue(n1 + n2));

  inv->verifyInvariants();

  // inv should have cached exactly one value.
  auto cachedRev = inv->m_headValue.ptr();
  EXPECT_NE(cachedRev, nullptr);
  {
    auto lock = lockify(*cachedRev);
    EXPECT_EQ(cachedRev, inv->m_tailValue.ptr());
    EXPECT_EQ(cachedRev->value_lck(), MemoValue(n1 + n2));
    EXPECT_EQ(cachedRev->begin_lck(), 1U);
    EXPECT_EQ(cachedRev->end_lck(), kNeverTxnId);
  }

  // Call it a second time, this time it should be cached.
  f = inv->asyncEvaluate();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 1);
  EXPECT_EQ(f.value().m_value, MemoValue(n1 + n2));

  const int64_t n1New = 5;

  // Change one of the cells.
  {
    Transaction txn2;

    {
      Transaction txn;
      txn.assign(c1, n1New);

      txn2 = std::move(txn);
    }
    EXPECT_EQ(startTxn, newestVisibleTxn());
  }

  EXPECT_EQ(newestVisibleTxn(), startTxn + 1);

  // The old head should still be there, except its end() should have changed.
  {
    auto lock = lockify(*inv);
    EXPECT_EQ(inv->m_headValue.ptr(), cachedRev);
    EXPECT_EQ(cachedRev, inv->m_tailValue.ptr());
    EXPECT_EQ(cachedRev->value_lck(), MemoValue(n1 + n2));
    EXPECT_EQ(cachedRev->begin_lck(), 1U);
    EXPECT_EQ(cachedRev->end_lck(), startTxn + 1);
  }

  f = inv->asyncEvaluate();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 2);
  EXPECT_EQ(f.value().m_value, MemoValue(5 + n2));

  // Now there should be two things in the MVCC list.,
  auto newHead = inv->m_headValue.ptr();
  {
    auto lock = lockify(*inv);
    EXPECT_EQ(numRevisions(*inv), 2U);
    EXPECT_EQ(newHead->value_lck(), MemoValue(5 + n2));
    EXPECT_EQ(newHead->begin_lck(), startTxn + 1);
    EXPECT_EQ(newHead->end_lck(), kNeverTxnId);
  }

  // Nothing to purge, as inv is in cleanup, not LRU.
  EXPECT_EQ(purgeLruList(), 0U);

  // The original value should still be in the MVCC cache.
  f = inv->asyncEvaluate(memoTask);
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 2);
  EXPECT_EQ(f.value().m_value, MemoValue(n1 + n2));

  // Try a change that has no net effect on the caller -- keep sum the same.
  {
    Transaction txn;
    txn.assign(c1, n1New + 20);
    txn.assign(c2, n2 - 20);
  }

  // That should have truncated the lifespan of the old head.
  {
    auto lock = lockify(*inv);
    EXPECT_EQ(inv->m_headValue.ptr(), newHead);
    EXPECT_EQ(numRevisions(*inv), 2U);
    EXPECT_EQ(newHead->value_lck(), MemoValue(n1New + n2));
    EXPECT_EQ(newHead->begin_lck(), startTxn + 1);
    EXPECT_EQ(newHead->end_lck(), startTxn + 2);
  }

  // Now re-run it. The inputs are different so it needs to reexecute the code.
  f = inv->asyncEvaluate();
  inv->verifyInvariants();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 3);
  EXPECT_EQ(f.value().m_value, MemoValue(n1New + n2));

  // That should have just extended the lifespan of the old head by (logically)
  // creating a consecutive Revision with the same value. Those get combined
  // into one physical Revision.
  {
    auto lock = lockify(*inv);
    EXPECT_EQ(inv->m_headValue.ptr(), newHead);
    EXPECT_EQ(numRevisions(*inv), 2U);
    EXPECT_EQ(newHead->value_lck(), MemoValue(n1New + n2));
    EXPECT_EQ(newHead->begin_lck(), startTxn + 1);
    EXPECT_EQ(newHead->end_lck(), kNeverTxnId);
  }

  // Discarding the task should run old cleanups.
  memoTask.reset();

  // The old tail should have been cleaned up.
  {
    auto lock = lockify(*newHead);
    EXPECT_EQ(numRevisions(*inv), 1U);
    EXPECT_EQ(newHead->begin_lck(), startTxn + 1);
    EXPECT_EQ(newHead->end_lck(), kNeverTxnId);
  }

  // Now it should be moved back to LRU.
  EXPECT_EQ(purgeLruList(), 1U);
}

TEST_F(MemoizeFixture, testRefresh) {
  const auto startNumTimesSumCalled = s_numTimesSumCalled;
  const TxnId startTxn = newestVisibleTxn();

  int64_t n1 = 111;
  int64_t n2 = 222;
  int64_t n3 = 333;

  Cell c1{MemoValue(n1)};
  Cell c2{MemoValue(n2)};
  Cell c3{MemoValue(n3)};

  // "inv1 = c1 + c2";
  auto iobj1 =
      newTestInstance<RSumInvocation>(*c1.invocation(), *c2.invocation())
          ->intern();
  auto inv1 = &Invocation::fromIObj(*iobj1);

  // "inv2 = inv1 + c3";
  auto iobj2 =
      newTestInstance<RSumInvocation>(*inv1, *c3.invocation())->intern();
  auto inv2 = &Invocation::fromIObj(*iobj2);

  inv1->verifyInvariants();
  inv2->verifyInvariants();

  auto f = inv2->asyncEvaluate();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 2);
  EXPECT_EQ(f.value().m_value, MemoValue(n1 + n2 + n3));

  auto task = createMemoTask();

  // Make a no-op change to iobj1's value. Swapping order preserves sum.
  {
    Transaction txn;
    txn.assign(c1, n2);
    txn.assign(c2, n1);
  }

  inv1->verifyInvariants();
  inv2->verifyInvariants();
  EXPECT_TRUE(inv1->m_headValue->canRefresh());
  EXPECT_TRUE(inv2->m_headValue->canRefresh());

  {
    auto head1 = inv1->m_headValue.ptr();
    auto lock = lockify(*head1);
    EXPECT_EQ(head1->currentRefcount(), 2U);
    EXPECT_EQ(head1, inv1->m_tailValue.ptr());
    EXPECT_EQ(head1->begin_lck(), 1U);
    EXPECT_EQ(head1->end_lck(), startTxn + 1);
  }

  {
    auto head2 = inv2->m_headValue.ptr();
    auto lock = lockify(*head2);
    EXPECT_EQ(head2->currentRefcount(), 1U);
    EXPECT_EQ(head2, inv2->m_tailValue.ptr());
    EXPECT_EQ(head2->begin_lck(), 1U);
    EXPECT_EQ(head2->end_lck(), startTxn + 1);
  }

  f = inv2->asyncEvaluate();
  EXPECT_TRUE(f.isReady());

  inv1->verifyInvariants();
  inv2->verifyInvariants();
  EXPECT_EQ(inv1->m_headValue->currentRefcount(), 2U);
  EXPECT_EQ(inv2->m_headValue->currentRefcount(), 1U);

  // We needed to re-run inv1, since its inputs changed, but should not have
  // re-run inv2 since its inactive input (inv1's result) turned out not to
  // change after all.
  EXPECT_EQ(s_numTimesSumCalled, startNumTimesSumCalled + 3);
  EXPECT_EQ(f.value().m_value, MemoValue(n1 + n2 + n3));

  inv1->verifyInvariants();
  inv2->verifyInvariants();

  EXPECT_EQ(purgeLruList(), 2U);
}

// Build a branchy call DAG that will take exponential time for
// a non-memoizeng algorithm to traverse, or for a naive invalidation
// algorithm.
TEST_F(MemoizeFixture, testDeep) {
  const auto startNumTimesSumCalled = s_numTimesSumCalled;

  constexpr size_t depth = 62;
  Cell zeroCell{MemoValue{0}};
  Cell oneCell{MemoValue{1}};
  Invocation& zero = *zeroCell.invocation();
  Invocation& one = *oneCell.invocation();

  Invocation::Ptr root{&one};

  for (size_t i = 0; i < depth; ++i) {
    // Create two "pass through" values (dummies that add "root" to "zero").
    auto i1 = newTestInstance<RSumInvocation>(*root, zero, i)->intern();
    auto i2 = newTestInstance<RSumInvocation>(zero, *root, i)->intern();

    // Sum the previous two values, doubling the previous "root".
    auto iobj = newTestInstance<RSumInvocation>(
                    Invocation::fromIObj(*i1), Invocation::fromIObj(*i2), i)
                    ->intern();
    root.reset(&Invocation::fromIObj(*iobj));
  }

  auto task1 = createMemoTask();
  MemoTask::Ptr task2;

  size_t numCalls = startNumTimesSumCalled;

  constexpr size_t numIterations = 100;
  for (size_t i = 1; i <= numIterations; ++i) {
    numCalls += depth * 3;

    for (int j = 0; j < 3; ++j) {
      auto f = root->asyncEvaluate();
      EXPECT_TRUE(f.isReady());

      // We double "one" depth times.
      EXPECT_EQ(f.value().m_value, MemoValue(int64_t(i & 1) << depth));
      EXPECT_EQ(s_numTimesSumCalled, numCalls);

      EXPECT_EQ(numRevisions(*root), i);
    }

    if (i == numIterations / 2) {
      task2 = createMemoTask();
    }

    // Toggle the leaf between 0 and 1, which should invalidate everything.
    {
      Transaction txn;
      txn.assign(oneCell, !(i & 1));
    }
  }

  EXPECT_EQ(numRevisions(*root), numIterations);

  // Kill off an old task and see that only some revisions get cleaned up.
  task1.reset();
  EXPECT_EQ(numRevisions(*root), (numIterations + 1) / 2 + 1);

  // Kill off all tasks and we should only keep the latest revision.
  task2.reset();
  EXPECT_EQ(numRevisions(*root), 1U);

  EXPECT_EQ(purgeLruList(), depth * 3);
}

/// This test creates a binary tree with 1024 leaves, where each inner node
/// sums the values of its two inputs.
///
/// A bunch of worker threads evaluate random nodes in the tree, while
/// the main thread commits "random" changes to the leaves. However the
/// changes are made such that each block of 32 leaves always has the same
/// sum it started with. So if a worker asks for one of the leaves
/// or bottom 5 levels it can't assert any known value, but the top 5
/// levels of the tree will always yield the same sums since they are
/// summing blocks of 32 that never change.
TEST_F(MemoizeFixture, testThreads) {
  //
  // First, make a big binary tree of "sum" operations.
  //

  // Total height of the tree.
  static constexpr int depth = 10;
  static constexpr uint32_t numLeaves = 1U << (depth - 1);

  // The "top" levels of the tree always keep the same sum.
  static constexpr int unchangingDepth = depth / 2;

  // These are the leaves of a big binary tree.
  std::vector<std::unique_ptr<Cell>> cells(numLeaves);

  constexpr size_t treeSize = numLeaves * 2 - 1;
  Invocation::Ptr tree[treeSize];
  int64_t values[treeSize];

  // Fill in the leaf nodes.
  for (size_t i = 0; i < numLeaves; ++i) {
    values[i] = i;
    cells[i] = std::make_unique<Cell>(MemoValue{static_cast<int64_t>(i)});
    tree[i] = cells[i]->invocation();
  }

  size_t parent[treeSize] = {
      0,
  };

  // Fill in the inner nodes.
  for (size_t out = numLeaves; out < treeSize; ++out) {
    size_t in = (out - numLeaves) * 2;
    auto& in1 = tree[in];
    auto& in2 = tree[in + 1];
    auto inner = newTestInstance<RSumInvocation>(*in1, *in2)->intern();

    parent[out] = in;

    tree[out] = &Invocation::fromIObj(*inner);
    values[out] = values[in] + values[in + 1];
  }

  // Every node in the tree should be unique.
  {
    skip::fast_set<Invocation*> seen;
    for (auto& p : tree) {
      seen.insert(p.get());
    }
    EXPECT_EQ(seen.size(), treeSize);
  }

  // Sanity check that all of the nodes look good before we start.
  for (size_t i = 0; i < treeSize; ++i) {
    tree[i]->verifyInvariants();
  }

  // NOTE: If this is set to zero we will do everything in the main thread.
  constexpr int numThreads = 16;

  g_oneThreadActive = (numThreads == 0);

  //
  // Spawn a bunch of threads that churn for a while evaluating random
  // tree nodes.
  //
  std::thread threads[numThreads];
  std::vector<Process::Ptr> processes;
  std::atomic<bool> stop{false};
  std::atomic<bool> pause{false};

  std::atomic<TxnId> failed{0};
  std::atomic<size_t> badIndex{0};

  for (size_t i = 0; i < numThreads; ++i) {
    processes.emplace_back(Process::make());
  }

  // Create a SkipBarrier.
  std::vector<UnownedProcess> barrierProcesses;
  for (auto& p : processes) {
    barrierProcesses.emplace_back(p);
  }
  barrierProcesses.emplace_back(Process::cur());
  SkipBarrier barrier{std::move(barrierProcesses)};

  // Evaluate a random tree node.
  auto runOneReaderTest = [&](LameRandom& rand) -> TxnId {
    auto task = createMemoTask();
    const TxnId txn = task->m_queryTxn;

    // Pick a random tree node and evaluate it.
    auto index = rand.next(treeSize);

    MemoValue value;
    UnownedProcess waiter{Process::cur()};

    auto future = tree[index]->asyncEvaluate(task).thenValue(
        [&value, &waiter](AsyncEvaluateResult v) {
          waiter.schedule([&value, v = std::move(v.m_value)]() { value = v; });
        });

    // T31331907: Requiring running the event loop is unsatisfying for Future
    // users, but *someone* has to run the completion handler tasks.
    // This needs more thought.
    while (value.type() == MemoValue::Type::kUndef) {
      Process::cur()->runExactlyOneTaskSleepingIfNecessary();
    }

    // The first levels of the tree have no fixed sum.
    if (index >= treeSize - ((1U << unchangingDepth) - 1)) {
      if (value != MemoValue{values[index]}) {
        TxnId old = 0;

        // Tell the main thread about our failure.
        if (failed.compare_exchange_strong(old, txn)) {
          badIndex = index;

          std::ostringstream sstr;
          sstr << "\nFound value discrepancy at index ";
          sstr << index << ": " << value << " != " << MemoValue{values[index]}
               << '\n';
          std::cerr << sstr.str() << std::flush;
        }

        while (true) {
          barrier.wait();
        }
      }
      EXPECT_EQ(value, MemoValue{values[index]});
    }

    return txn;
  };

  for (int rank = 0; rank < numThreads; ++rank) {
    threads[rank] = std::thread([&, rank]() {
      ProcessContextSwitcher guard{processes[rank]};

      // Wait for all threads to be ready.
      barrier.wait();

      LameRandom readerRand{uint32_t(rank)};

      TxnId lastTxn = 0;
      size_t numTimesUnchanged = 0;

      while (!stop) {
        if (pause) {
          barrier.wait();
          // ... main thread can do stuff here with workers frozen ...
          barrier.wait();
        }

        // Test one tree node.
        TxnId txn = runOneReaderTest(readerRand);

        // Take action if the tree hasn't changed in a while.
        if (txn != lastTxn) {
          lastTxn = txn;
          numTimesUnchanged = 0;
        } else {
          ++numTimesUnchanged;
          if (numTimesUnchanged >= 10) {
            // We've done a bunch of iterations at the same txn.
            // Give the master thread a chance to commit a fresh one.
            std::this_thread::yield();
          }
        }
      }
    });
  }

  barrier.wait();

  constexpr size_t infiniteIterations = ~(size_t)0;
  constexpr size_t maxIterations = infiniteIterations;

  // Run the test for this many seconds, unless
  // maxIterations != infiniteIterations, in which case we will run
  // for that many iterations.
  const auto duration = std::chrono::milliseconds(5000);
  const auto endTime = std::chrono::steady_clock::now() + duration;

  // The main thread will make random changes to the tree while the
  // workers run.
  LameRandom writerRand{~0u};

  // Track what the leaves were at each step.
  std::vector<int64_t> initialLeaves(&values[0], &values[numLeaves]);
  std::map<TxnId, std::vector<int64_t>> leaves = {
      {newestVisibleTxn(), initialLeaves}};

  // Aligned blocks of this many leaves always keep the same sum.
  constexpr auto blockSize = 1U << (depth - unchangingDepth);

  auto runOneWriterTest = [&]() {
    // Pick how many pairs of changes we'll make, such that it will usually
    // be small but could be large.
    uint32_t numChanges = 1 + writerRand.next(numLeaves / 2);
    for (int i = 0; i < 3; ++i) {
      numChanges = std::min(numChanges, 1 + writerRand.next(numChanges));
    }

    {
      Transaction txn;

      for (uint32_t i = 0; i < numChanges; ++i) {
        // Choose two indices in the same block.
        auto index1 = writerRand.next(numLeaves);
        auto index2 = index1 ^ (1 + writerRand.next(blockSize - 1));

        values[index1] += 1;
        values[index2] -= 1;

        txn.assign(*cells[index1], values[index1]);
        txn.assign(*cells[index2], values[index2]);
      }
    }

    // Remember these values.
    leaves.insert({newestVisibleTxn(),
                   std::vector<int64_t>(&values[0], &values[numLeaves])});
  };

  if (numThreads == 0) {
    for (size_t iter = 1; iter <= maxIterations; ++iter) {
      {
        TestHookGuard guard([&]() {
          // Sometimes make a write change in the middle of reading something,
          // but in a determistic manner.
          if (writerRand.next(64) == 0) {
            runOneWriterTest();
          }
        });

        runOneReaderTest(writerRand);
      }

      for (size_t i = 0; i < treeSize; ++i) {
        tree[i]->verifyInvariants();

        // Since no tasks are active here we should have cleaned up
        // anything older.
        EXPECT_LE(
            numRevisions(*tree[i]),
            newestVisibleTxn() - oldestVisibleTxn() + 1);
      }

      if (maxIterations == infiniteIterations &&
          std::chrono::steady_clock::now() >= endTime) {
        break;
      }
    }
  } else {
    for (size_t iter = 1; iter <= maxIterations; ++iter) {
      if (iter % 1000000 == 0) {
        std::cerr << iter << "..." << std::flush;
      }

      // Throw away old sets of leaves.
      {
        const TxnId oldest = oldestVisibleTxn();
        while (!leaves.empty() && leaves.begin()->first < oldest) {
          leaves.erase(leaves.begin());
        }
      }

      runOneWriterTest();

      if (const TxnId badTxn = failed) {
        // We got the wrong result, try to report some info about what
        // happened. You may be happier dropping depth to 4 if you can
        // reproduce this, to test a smaller tree.
        const size_t index = badIndex;

        // Stop churn from all the other threads.
        pause = true;
        barrier.wait();
        pause = false;

        std::cerr << "Main thread thinking about the situation at " << badTxn
                  << "; newestVisible = " << newestVisibleTxn()
                  << ", index = " << index << "\n";

        EXPECT_NE(leaves.find(badTxn), leaves.end());

        const auto& bad = leaves[badTxn];

        for (size_t i = 0; i < numLeaves; i += blockSize) {
          int64_t sum1 = 0;
          int64_t sum2 = 0;

          for (size_t j = i; j < i + blockSize; ++j) {
            sum1 += initialLeaves[j];
            sum2 += bad[j];
          }

          EXPECT_EQ(sum1, sum2);
        }

        for (size_t i = 0; i < numLeaves; ++i) {
          EXPECT_EQ(rawGet(tree[i], badTxn), bad[i]);
        }

        std::cout << "Inputs OK\n";

        auto dumpify = [&](size_t idx) {
          std::cout << "idx " << idx << ": ";
          if (idx >= numLeaves) {
            std::cout << "parents " << parent[idx] << ", " << (parent[idx] + 1)
                      << ": ";
          }
          dumpRevisions(*tree[idx]);
        };

        std::vector<size_t> check = {index};
        while (!check.empty()) {
          auto x = check.back();
          check.pop_back();

          auto sum = rawGet(tree[x], badTxn);
          auto n1 = rawGet(tree[parent[x]], badTxn);
          auto n2 = rawGet(tree[parent[x] + 1], badTxn);

          if (sum != n1 + n2) {
            std::cout << sum << " != " << n1 << " + " << n2 << " for index "
                      << x << " = " << parent[x] << " + " << (parent[x] + 1)
                      << " at txn=" << badTxn << '\n';

            std::cout << '\n';
            dumpify(x);
            std::cout << '\n';
            dumpify(parent[x]);
            std::cout << '\n';
            dumpify(parent[x] + 1);
            std::cout << std::endl;

            EXPECT_EQ(sum, n1 + n2);
          }

          if (parent[x] >= numLeaves) {
            check.push_back(parent[x]);
            check.push_back(parent[x] + 1);
          }
        }

        exit(1);

        barrier.wait();
      }

      if (writerRand.next(100) == 0) {
        // Pause all the worker threads before we check invariants, as
        // some "invariants" are momentarily invalid in an active system.
        pause = true;
        barrier.wait();
        pause = false;

        for (size_t i = 0; i < treeSize; ++i) {
          tree[i]->verifyInvariants();
        }

        // Let the other threads proceed.
        barrier.wait();
      }

      if (maxIterations == infiniteIterations &&
          std::chrono::steady_clock::now() >= endTime) {
        break;
      }
    }

    stop = true;

    for (auto& t : threads) {
      t.join();
    }
  }

  // Evaluate the root, just to be 100% sure every node has been evaluated.
  auto f = tree[treeSize - 1]->asyncEvaluate();
  EXPECT_EQ(std::move(f).get().m_value, MemoValue{values[treeSize - 1]});

  cells.clear();

  // Create and discard a task just to flush out any final cleanups.
  createMemoTask();

  assertNoCleanups();

  // Every internal node should be on the LRU list.
  EXPECT_EQ(purgeLruList(), treeSize - numLeaves);

  // Disassemble the tree, top-to-bottom.
  for (size_t i = treeSize; i > 0; --i) {
    if (i <= numLeaves) {
      EXPECT_EQ(tree[i - 1]->currentRefcount(), 1U);
    } else {
      EXPECT_REFCOUNT(tree[i - 1]->asIObj(), 1U);
    }
    tree[i - 1].reset();
  }

  g_oneThreadActive = true;
}

namespace {

size_t s_argConstructCount = 0;
size_t s_argEvaluateCount = 0;
size_t s_argStaticOffsetCount = 0;
size_t s_objConstructCount = 0;
size_t s_objDestructCount = 0;
size_t s_objFinalizeCount = 0;

struct Plus10Obj;

struct Plus10Args {
  int64_t n;

  explicit Plus10Args(int64_t _n) : n(_n) {
    s_argConstructCount++;
  }

  void asyncEvaluate(Plus10Obj& /*obj*/, folly::Promise<MemoValue>&& promise)
      const {
    s_argEvaluateCount++;
    promise.setValue(MemoValue(n + 10));
  }

  static std::vector<size_t> static_offsets() {
    s_argStaticOffsetCount++;
    return {};
  }
};

struct Plus10Obj {
  size_t m_value;

  ~Plus10Obj() {
    s_objDestructCount++;
  }

  Plus10Obj() = delete;
  Plus10Obj(const Plus10Obj&) = delete;

  Plus10Obj(IObj& /*iobj*/, const Plus10Args& /*args*/) {
    s_objConstructCount++;
  }

  void finalize(IObj& /*iobj*/, const Plus10Args& /*args*/) {
    s_objFinalizeCount++;
  }
};

TEST_F(MemoizeFixture, testSimpleFinalize) {
  const int64_t arg = 37;

  EXPECT_EQ(s_argConstructCount, 0U);
  EXPECT_EQ(s_argEvaluateCount, 0U);
  EXPECT_EQ(s_argStaticOffsetCount, 0U);
  EXPECT_EQ(s_objConstructCount, 0U);
  EXPECT_EQ(s_objDestructCount, 0U);
  EXPECT_EQ(s_objFinalizeCount, 0U);

  auto iobj = InvocationHelper<Plus10Obj, Plus10Args>::factory(Plus10Args(arg));

  EXPECT_EQ(s_argConstructCount, 1U);
  EXPECT_EQ(s_argEvaluateCount, 0U);
  EXPECT_EQ(s_argStaticOffsetCount, 1U);
  EXPECT_EQ(s_objConstructCount, 1U);
  EXPECT_EQ(s_objDestructCount, 0U);
  EXPECT_EQ(s_objFinalizeCount, 0U);

  auto iobj2 =
      InvocationHelper<Plus10Obj, Plus10Args>::factory(Plus10Args(arg));

  EXPECT_EQ(iobj.get(), iobj2.get());
  iobj2 = nullptr;

  EXPECT_EQ(s_argConstructCount, 2U);
  EXPECT_EQ(s_argEvaluateCount, 0U);
  EXPECT_EQ(s_argStaticOffsetCount, 1U);
  EXPECT_EQ(s_objConstructCount, 1U);
  EXPECT_EQ(s_objDestructCount, 0U);
  EXPECT_EQ(s_objFinalizeCount, 0U);

  auto f = Invocation::fromIObj(*iobj).asyncEvaluate();
  EXPECT_TRUE(f.isReady());
  EXPECT_EQ(f.value().m_value, MemoValue(arg + 10));

  EXPECT_EQ(s_argConstructCount, 2U);
  EXPECT_EQ(s_argEvaluateCount, 1U);
  EXPECT_EQ(s_argStaticOffsetCount, 1U);
  EXPECT_EQ(s_objConstructCount, 1U);
  EXPECT_EQ(s_objDestructCount, 0U);
  EXPECT_EQ(s_objFinalizeCount, 0U);

  iobj = nullptr;

  EXPECT_EQ(s_argConstructCount, 2U);
  EXPECT_EQ(s_argEvaluateCount, 1U);
  EXPECT_EQ(s_argStaticOffsetCount, 1U);
  EXPECT_EQ(s_objConstructCount, 1U);
  EXPECT_EQ(s_objDestructCount, 0U);
  EXPECT_EQ(s_objFinalizeCount, 0U);

  purgeLruList();

  EXPECT_EQ(s_argConstructCount, 2U);
  EXPECT_EQ(s_argEvaluateCount, 1U);
  EXPECT_EQ(s_argStaticOffsetCount, 1U);
  EXPECT_EQ(s_objConstructCount, 1U);
  EXPECT_EQ(s_objDestructCount, 1U);
  EXPECT_EQ(s_objFinalizeCount, 1U);
}
} // anonymous namespace

// Test the exception handling through futures.
namespace {

struct NoResponseNode {
  struct Args {
    size_t uniq;
    explicit Args(size_t _uniq) : uniq(_uniq) {}

    void asyncEvaluate(
        NoResponseNode& /*obj*/,
        folly::Promise<MemoValue>&& /*promise*/) const {
      // Don't respond to the promise
    }

    static std::vector<size_t> static_offsets() {
      return {};
    }
  };

  NoResponseNode(IObj& iobj, const Args& /*args*/) {
    Invocation::fromIObj(iobj).m_isNonMvccAware = true;
  }

  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}

  using InvHelper = InvocationHelper<NoResponseNode, Args>;
};

struct ThrowNode {
  struct Args {
    size_t uniq;
    explicit Args(size_t _uniq) : uniq(_uniq) {}

    void asyncEvaluate(
        ThrowNode& /*obj*/,
        folly::Promise<MemoValue>&& /*promise*/) const {
      throw std::runtime_error("This is a thrown error");
    }

    static std::vector<size_t> static_offsets() {
      return {};
    }
  };

  ThrowNode(IObj& iobj, const Args& /*args*/) {
    Invocation::fromIObj(iobj).m_isNonMvccAware = true;
  }

  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}

  using InvHelper = InvocationHelper<ThrowNode, Args>;
};

TEST_F(MemoizeFixture, testException) {
  {
    auto node = NoResponseNode::InvHelper::factory(NoResponseNode::Args(0));

    std::mutex mutex;
    std::condition_variable cond;
    bool ready = false;
    bool gotError = false;
    std::string what;

    Invocation::fromIObj(*node)
        .asyncEvaluate()
        .thenValue([&](AsyncEvaluateResult /*value*/) {
          std::unique_lock<std::mutex> lock(mutex);
          ready = true;
          gotError = false;
        })
        .thenError(
            folly::tag_t<std::exception>{}, [&](const std::exception& e) {
              std::unique_lock<std::mutex> lock(mutex);
              ready = true;
              gotError = true;
              what = e.what();
            });

    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&] { return ready; });
    EXPECT_TRUE(gotError);
    EXPECT_TRUE(boost::starts_with(what, "Broken promise"));
  }
  {
    auto node = ThrowNode::InvHelper::factory(ThrowNode::Args(0));

    std::mutex mutex;
    std::condition_variable cond;
    bool ready = false;
    bool gotError = false;
    std::string what;

    Invocation::fromIObj(*node)
        .asyncEvaluate()
        .thenValue([&](AsyncEvaluateResult /*value*/) {
          std::unique_lock<std::mutex> lock(mutex);
          ready = true;
          gotError = false;
        })
        .thenError(
            folly::tag_t<std::exception>{}, [&](const std::exception& e) {
              std::unique_lock<std::mutex> lock(mutex);
              ready = true;
              gotError = true;
              what = e.what();
            });

    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&] { return ready; });
    EXPECT_TRUE(gotError);
    EXPECT_EQ(what, "This is a thrown error");
  }
}
} // anonymous namespace

// Test string handling of memoization
namespace {

struct StringConcat {
  struct Args {
    IObj* m_lhs;
    IObj* m_rhs;

    explicit Args(IObj& lhs, IObj& rhs) : m_lhs(&lhs), m_rhs(&rhs) {}

    static std::vector<size_t> static_offsets() {
      return {offsetof(Args, m_lhs), offsetof(Args, m_rhs)};
    }

    void asyncEvaluate(
        StringConcat& /*obj*/,
        folly::Promise<MemoValue>&& promise) const {
      collect(
          Invocation::fromIObj(*m_lhs).asyncEvaluate(),
          Invocation::fromIObj(*m_rhs).asyncEvaluate())
          .thenValue(
              [promise = std::move(promise)](
                  const std::tuple<AsyncEvaluateResult, AsyncEvaluateResult>&
                      t) mutable {
                // Since we're allocating a temporary string (via
                // String::concat2()) we need to make sure to clear out the
                // Obstack when we're done.
                Obstack::PosScope obstackScope;

                const MemoValue& lhs = std::get<0>(t).m_value;
                const MemoValue& rhs = std::get<1>(t).m_value;

                String lhString;
                if (lhs.isNull()) {
                  lhString = String("<null>");
                } else if (lhs.isString()) {
                  lhString = *lhs.asString();
                } else {
                  lhString = String("<invalid>");
                }

                String rhString;
                if (rhs.isNull()) {
                  rhString = String("<null>");
                } else if (rhs.isString()) {
                  rhString = *rhs.asString();
                } else {
                  rhString = String("<invalid>");
                }

                String res = String::concat2(lhString, rhString);
                auto i = intern(res);

                promise.setValue(MemoValue(i));
              });
    }
  };

  ~StringConcat() = default;

  StringConcat() = delete;
  StringConcat(const StringConcat&) = delete;

  StringConcat(IObj& /*iobj*/, const Args& /*args*/) {}

  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}

  using Ptr = boost::intrusive_ptr<const InvocationHelper<StringConcat, Args>>;
};

struct StringNode {
  struct Args {
    size_t uniq;
    explicit Args(size_t _uniq) : uniq(_uniq) {}

    void asyncEvaluate(StringNode& /*obj*/, folly::Promise<MemoValue>&& promise)
        const {
      // By default we have no value
      promise.setValue(MemoValue(nullptr));
    }

    static std::vector<size_t> static_offsets() {
      return {};
    }
  };

  StringNode(IObj& iobj, const Args& /*args*/) {
    Invocation::fromIObj(iobj).m_isNonMvccAware = true;
  }

  void finalize(IObj& /*iobj*/, const Args& /*args*/) {}

  using InvHelper = InvocationHelper<StringNode, Args>;
};

TEST_F(MemoizeFixture, testStringConcat) {
  StringConcat::Ptr concat;

  auto node1 = StringNode::InvHelper::factory(StringNode::Args(0));
  auto& inode1 = Invocation::fromIObj(*node1);

  auto node2 = StringNode::InvHelper::factory(StringNode::Args(1));
  auto& inode2 = Invocation::fromIObj(*node2);

  concat = InvocationHelper<StringConcat, StringConcat::Args>::factory(
      StringConcat::Args(*node1, *node2));

  auto synchEval = [&]() {
    std::mutex mutex;
    std::condition_variable cond;
    bool ready = false;
    StringPtr result;

    Invocation::fromIObj(*concat).asyncEvaluate().thenValue(
        [&](AsyncEvaluateResult value) {
          result = value.m_value.asString();

          std::unique_lock<std::mutex> lock(mutex);
          ready = true;
          cond.notify_all();
        });

    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&] { return ready; });

    return result;
  };

  EXPECT_EQ(*synchEval(), String("<null><null>"));

  {
    Transaction txn;
    txn.assignMemoValue(inode1, MemoValue(intern(String("111"))));
    txn.assignMemoValue(inode2, MemoValue(intern(String("222"))));
  }

  EXPECT_EQ(*synchEval(), String("111222"));

  {
    Transaction txn;
    txn.assignMemoValue(inode1, MemoValue(intern(String("333333333"))));
    txn.assignMemoValue(inode2, MemoValue(intern(String("444444444"))));
  }

  EXPECT_EQ(*synchEval(), String("333333333444444444"));

  purgeLruList();
}
} // anonymous namespace
