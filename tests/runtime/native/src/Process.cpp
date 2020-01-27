/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Process.h"
#include "skip/parallel.h"
#include "skip/System.h"
#include "skip/util.h"

#include "ObstackDetail.h"

#include <utility>

namespace skip {

// This is a faster-to-access copy of tl_currentProcessPtr,
// because thread_local has overhead that __thread does not.
__thread Process* tl_currentProcess;

namespace {
thread_local Process::Ptr tl_currentProcessPtr;

// Process::m_tasks holds a linked list ending in a special tag value
// (a bogus pointer). The tag conveys information about the ownership
// state of the Process, allowing atomic Process ownership transfer and
// thread sleeping/waking.
//
// NOTE: If you modify these tags be sure to update isEmptyList().
//

// No thread owns the Process. Any thread can take ownership of it and
// run tasks in it.
static Task* const kOrphanedTag = nullptr;

// Some thread owns the Process, but is waiting on m_baton for some thread
// to post a task then wake it up.
static Task* const kSleepingTag = kOrphanedTag + 1;

// The Process has been joined, posting new tasks is an internal error.
static Task* const kDeadTag = kSleepingTag + 1;

// Some thread owns the Process and will eventually run all posted tasks.
static Task* const kOwnedTag = kDeadTag + 1;

static bool isEmptyList(Task* task) {
  // Any of the tags can terminate a Task list.
  return task <= kOwnedTag;
}

static void runTask(Task* task) {
  std::unique_ptr<Task> sentinel{task};

  // We can't do anything reasonable with exceptions, since we
  // need to process the Task list and more work may remain, so just die.
  try {
    task->run();
  } catch (std::exception& exc) {
    fatal("Unexpected exception in runTask: %s", exc.what());
  } catch (...) {
    fatal("Unexpected exception in runTask");
  }
}
} // namespace

void intrusive_ptr_add_ref(Process* p) {
  p->incref();
}

void intrusive_ptr_release(Process* p) {
  p->decref();
}

Process::Process(Context* ctx)
    : m_tasks(kOwnedTag), m_obstack(), m_refcount(1), m_ctx(ctx) {}

Process::Process(UnownedProcess parent, SkipObstackPos pos, Context* ctx)
    : m_tasks(kOwnedTag),
      m_obstack(pos),
      m_refcount(1),
      m_ctx(ctx),
      m_parent(std::move(parent)) {}

Process::Ptr Process::make(Context* ctx) {
  return Ptr(new Process(ctx), false);
}

Process::Ptr
Process::make(UnownedProcess parent, SkipObstackPos pos, Context* ctx) {
  return Ptr(new Process(std::move(parent), pos, ctx), false);
}

void Process::installPermanently() {
  contextSwitchTo(Process::make());
}

Obstack& Process::obstackAssumingSuspended() {
  // This will be nullptr if we are running, rather than suspended.
  assert(m_obstack.m_detail != nullptr);
  return static_cast<Obstack&>(m_obstack);
}

Refcount Process::currentRefcount() const {
  return m_refcount.load(std::memory_order_relaxed);
}

Process::Ptr Process::contextSwitchTo(Process::Ptr processPtr) {
  auto oldProcess = tl_currentProcess;
  auto newProcess = processPtr.get();

  if (oldProcess != newProcess) {
    if (oldProcess) {
      oldProcess->suspend();
    }

    // Install the new process, if any.
    tl_currentProcess = newProcess;
    tl_currentProcessPtr.swap(processPtr);

    if (newProcess) {
      newProcess->resume();
    }
  }

  return processPtr;
}

void Process::incref() {
  m_refcount.fetch_add(1, std::memory_order_relaxed);
}

void Process::decref() {
  if (m_refcount.fetch_sub(1, std::memory_order_release) == 1) {
    delete this;
  }
}

void Process::suspend() {
  m_obstack = std::move(Obstack::cur());
}

void Process::resume() {
  Obstack::cur() = std::move(obstackAssumingSuspended());
}

Task* Process::pushTaskList(Task& head, Task& tail) {
  auto oldHead = m_tasks.load(std::memory_order_relaxed);
  do {
    tail.m_next = oldHead;
  } while (UNLIKELY(!m_tasks.compare_exchange_weak(
      oldHead, &head, std::memory_order_release)));
  return oldHead;
}

// Atomically prepend a Task to the list, returning the old head.
Task* Process::pushTask(std::unique_ptr<Task> utask) {
  auto task = utask.release();
  return pushTaskList(*task, *task);
}

void Process::scheduleTask(std::unique_ptr<Task> task) {
  pushTask(std::move(task));
}

Task* Process::maybePopTask() {
  // NOTE: Normally atomically popping from a linked list is not safe
  // (the "A-B-A" problem), but in this case we know that this thread
  // (the Process owner), is the only thread doing popping, so it's safe.
  // Only pushes could be happening in parallel, not other pops, and
  // we handle those correctly.
  for (auto task = m_tasks.load(std::memory_order_relaxed);
       !isEmptyList(task);) {
    // Leave the list head containing either a valid Task or kOwnedTag.
    auto next = task->m_next;
    auto newHead = isEmptyList(next) ? kOwnedTag : next;

    if (LIKELY(m_tasks.compare_exchange_weak(
            task, newHead, std::memory_order_acquire))) {
      return task;
    }
  }

  return nullptr;
}

void Process::runReadyTasks() {
  while (auto task = maybePopTask()) {
    runTask(task);
  }
}

void Process::runReadyTasksThenDisown() {
  while (true) {
    // Try to give up ownership by replacing kOwnedTag with kOrphanedTag,
    // i.e. transitioning from the "no remaining tasks" state to the
    // "no owner" state.
    //
    // If someone pushed another Task while we were running, this will
    // fail and we'll run whatever Tasks exist.
    auto expected = kOwnedTag;
    if (m_tasks.compare_exchange_weak(
            expected, kOrphanedTag, std::memory_order_release)) {
      break;
    }

    // Run ready tasks, context-switched to this Process.
    //
    // Note that it's important to context-switch away from this
    // Process before possibly yielding ownership back to another thread.
    ProcessContextSwitcher guard(Ptr(this));
    runReadyTasks();
  }
}

void Process::runExactlyOneTaskSleepingIfNecessary() {
  while (true) {
    if (auto task = maybePopTask()) {
      runTask(task);
      return;
    }

    auto expected = kOwnedTag;
    if (m_tasks.compare_exchange_weak(
            expected, kSleepingTag, std::memory_order_relaxed)) {
      // We successfully went to sleep without any new task showing up.
      // Now any thread that pushes a new task will post() to m_baton.
      // Wait for that to happen.
      std::unique_lock<std::mutex> lock(m_batonLock);
      while(!m_batonCond) {
        m_batonSignal.wait(lock);
      }
      m_batonCond = false;
    }
  }
}

void Process::drainEverythingSleepingIfNecessary() {
  assert(this == Process::cur());

  while (true) {
    runReadyTasks();
    if (!Obstack::cur().anyHandles()) {
      break;
    }
    runExactlyOneTaskSleepingIfNecessary();
  }
}

bool Process::isSleeping() const {
  return m_tasks.load(std::memory_order_relaxed) == kSleepingTag;
}

void Process::sweepChild(Process& child) {
  auto& source = child.obstackAssumingSuspended();
  if (!source.anyValidHandles()) {
    source.collect();
  }
}

void Process::joinChild(Process& child, SkipObstackPos parentNote) {
  auto& parentObstack = Obstack::cur();
  auto& parentObstackDetail = *parentObstack.m_detail;
  auto& childObstack = child.obstackAssumingSuspended();

  // Copy over all live objects and all handles.
  parentObstackDetail.stealObjectsAndHandles(
      *Process::cur(), parentObstack, parentNote, childObstack);

  // Steal all the tasks as well. Now that we have moved all the handles,
  // no one should be attempting to post any more tasks to the child.
  auto head = child.m_tasks.exchange(kDeadTag, std::memory_order_acquire);
  if (!isEmptyList(head)) {
    auto tail = head;

    while (true) {
      auto next = tail->m_next;
      if (isEmptyList(next)) {
        break;
      }
      tail = next;
    }

    // Push the entire task list at once, which preserves its order.
    pushTaskList(*head, *tail);
  }
}

bool UnownedProcess::scheduleTaskIfNotDead(std::unique_ptr<Task>& task) {
  auto oldHead = m_process->m_tasks.load(std::memory_order_relaxed);
  do {
    if (UNLIKELY(oldHead == kDeadTag)) {
      return false;
    }
    task->m_next = oldHead;
  } while (UNLIKELY(!m_process->m_tasks.compare_exchange_weak(
      oldHead, task.get(), std::memory_order_release)));

  task.release();

  if (oldHead == kOrphanedTag) {
    // No one owned the task before we posted, and we do now.
    // So make sure someone executes this Task exactly once at some point.

    auto lambda = [process = m_process]() {
      process->runReadyTasksThenDisown();
    };
    auto arbiter = OneShotTask::Arbiter::make(
        std::make_unique<LambdaTask<decltype(lambda)>>(std::move(lambda)));

    bool requested ATTR_UNUSED = false;

    if (m_process->m_parent) {
      m_process->m_parent.schedule([arbiter]() { arbiter->runIfFirst(); });
      requested = true;
    }

    // Someone, somwehere must have been asked to run this...
    assert(requested);
  } else if (oldHead == kSleepingTag) {
    // We were the first to post after the owner went to sleep waiting
    // for a Task to show up. So wake it up.
    std::unique_lock<std::mutex> lock(m_process->m_batonLock);
    m_process->m_batonCond = true;
    m_process->m_batonSignal.notify_one();
  }

  return true;
}

void UnownedProcess::scheduleTask(std::unique_ptr<Task> task) {
  if (!scheduleTaskIfNotDead(task)) {
    fatal("Attempted to schedule task onto dead process");
  }
}
} // namespace skip
