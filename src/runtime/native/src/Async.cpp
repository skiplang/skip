/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Async-extc.h"
#include "skip/Async.h"
#include "skip/external.h"
#include "skip/memoize.h"
#include "skip/objects.h"
#include "skip/Obstack.h"
#include "skip/Process.h"
#include "skip/System.h"
#include "skip/System-extc.h"
#include "skip/Task.h"
#include "skip/Type.h"

using namespace skip;

Awaitable* skip::callMemoizeThunk(const skip::RObj* thunk) {
  auto rawCode = thunk->vtable().getFunctionPtr();
  auto code = reinterpret_cast<Awaitable* (*)(const skip::RObj*)>(rawCode);
  return code(thunk);
}

SkipBool SKIP_awaitableReadyOrThrow(Awaitable* awaitable) {
  auto c = awaitable->m_continuation.sbits();

  if (LIKELY(c == kAwaitableValueMarker)) {
    return true;
  } else if (c == kAwaitableExceptionMarker) {
    SKIP_throw(awaitable->m_exception.unsafeAsPtr());
  } else {
    return false;
  }
}

Awaitable* skip::handleToAwaitable(RObjHandle& handle) {
  return static_cast<Awaitable*>(handle.get().unsafeAsPtr());
}

void SKIP_awaitableSyncOrThrow(Awaitable* awaitable) {
  if (!SKIP_awaitableReadyOrThrow(awaitable)) {
    // Running tasks could theoretically move the awaitable pointer,
    // so we need to make a handle for it here.
    auto handle = Obstack::cur().makeHandle(awaitable);
    do {
      Process::cur()->runExactlyOneTaskSleepingIfNecessary();
    } while (!SKIP_awaitableReadyOrThrow(handleToAwaitable(*handle)));
  }
}

struct WakeAwaitablesTask final : Task {
  explicit WakeAwaitablesTask(std::unique_ptr<RObjHandle> awaitables)
      : m_awaitables(std::move(awaitables)) {}

  void run() override {
    auto awaitable = handleToAwaitable(*m_awaitables);
    m_awaitables.reset();

    // Ensure we run the rest of the list, if any.
    if (auto next = awaitable->m_nextAwaitable) {
      awaitable->m_nextAwaitable = nullptr;
      auto task =
          std::make_unique<WakeAwaitablesTask>(Obstack::cur().makeHandle(next));
      Process::cur()->scheduleTask(std::move(task));
    }

    SKIP_awaitableResume(awaitable);
  }

  std::unique_ptr<RObjHandle> m_awaitables;
};

// Handles the completion of an Awaitable, either a value or exception.
// It wakes up any waiters who were suspended waiting for 'awaitable'.
static void
awaitableFinish(Awaitable* awaitable, bool contextWaiting, intptr_t status) {
  // NOTE: This is designed so we can make this atomic if we ever make it
  // possible for multiple threads to manipulate an Awaitable at the same time.
  auto cont = awaitable->m_continuation;
  awaitable->m_continuation.setSBits(status);

  if (cont.sbits() != 0) {
    // Post one Task responsible for waking every waiter. We only need
    // a Handle to the first one.
    auto handle = Obstack::cur().makeHandle(cont);
    auto task = std::make_unique<WakeAwaitablesTask>(std::move(handle));
    Process::cur()->scheduleTask(std::move(task));
  }

  if (contextWaiting) {
    MemoValue v;
    SKIP_awaitableToMemoValue(&v, awaitable);
    Context::current()->evaluateDone(std::move(v));
  }
}

void SKIP_awaitableNotifyWaitersValueIsReady(Awaitable* awaitable) {
  const auto contextWaiting =
      (awaitable->m_exception.sbits() == kContextIsAwaitingThis);
  awaitableFinish(awaitable, contextWaiting, kAwaitableValueMarker);
}

void SKIP_awaitableThrow(Awaitable* awaitable, RObj* exception) {
  const auto contextWaiting =
      (awaitable->m_exception.sbits() == kContextIsAwaitingThis);
  awaitable->m_exception = exception;
  awaitableFinish(awaitable, contextWaiting, kAwaitableExceptionMarker);
}

void SKIP_awaitableSuspend(Awaitable* waiter, Awaitable* waitee) {
  // The waitee should not be complete yet.
  assert(waitee->m_continuation.sbits() >= 0);

  // Prepend to linked list.
  waiter->m_nextAwaitable = waitee->m_continuation.asPtr();
  waitee->m_continuation.setPtr(waiter);
}
