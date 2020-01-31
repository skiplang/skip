/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "Obstack.h"
#include "Refcount.h"
#include "Task.h"
#include "Type.h"

#include <condition_variable>
#include <memory>

#include <boost/intrusive_ptr.hpp>

namespace skip {

extern __thread Process* tl_currentProcess;

// Corresponds to a Process that the current thread does not necessarily
// own, so it can't run Tasks in it, etc. You can only do a limited set
// of operations; namely, ask it to do asynchronous work (ala posting
// to an event loop).
struct UnownedProcess {
  UnownedProcess() = default;
  UnownedProcess(const UnownedProcess&) = default;
  UnownedProcess(UnownedProcess&&) = default;
  UnownedProcess& operator=(UnownedProcess&& other) = default;

  explicit UnownedProcess(boost::intrusive_ptr<Process> process)
      : m_process(std::move(process)) {}

  template <typename T>
  void schedule(T&& function) {
    scheduleTask(std::make_unique<LambdaTask<T>>(std::move(function)));
  }

  // Cause the task to be run by some thread that owns the Process,
  // scheduling a worker thread if there is no owner. Always returns
  // immediately.
  void scheduleTask(std::unique_ptr<Task> task);

  // If this Process is still live (not kDeadTag), post task,
  // taking ownership of it, and return true.
  //
  // Else, leave task alone and return false.
  bool scheduleTaskIfNotDead(std::unique_ptr<Task>& task);

  explicit operator bool() const {
    return m_process.get() != nullptr;
  }

 private:
  boost::intrusive_ptr<Process> m_process;
};

// A Process is analogous to an operating system process, containing
// an "address space" (Obstack, in our case) and other "local" context.
// It does not retain an associated program stack.
//
// Just as an OS process can be either suspended in the kernel or actively
// run by some core, so can a Process be suspended or actively run by some
// thread.
//
// Each Process can only be run by one thread at a time.  A thread can
// only be running up to one Process at a time, and it could be
// running zero Processes, in which case it is not allowed to run any
// Skip code.
//
// "Context switching" to a Process installs the Process object as
// Process::cur(), and may also set up other thread-local variables
// associated with that Process.
struct Process final : private skip::noncopyable {
  using Ptr = ProcessPtr;

  // Create a new, empty Process.
  static Ptr make(Context* ctx = nullptr);

  static Ptr make(UnownedProcess parent, SkipObstackPos pos, Context* ctx);

  // Create a process and install it permanently on the calling thread.
  static void installPermanently();

  // Get the Process currently running in this thread, if any.
  static Process* cur() {
    return tl_currentProcess;
  }

  void runReadyTasks();
  void runReadyTasksThenDisown();
  void runExactlyOneTaskSleepingIfNecessary();

  // Wait until all tasks and handles are resolved before returning.
  // This must be currently context-switched in.
  void drainEverythingSleepingIfNecessary();

  template <typename T>
  void schedule(T&& function) {
    scheduleTask(std::make_unique<LambdaTask<T>>(std::move(function)));
  }
  void scheduleTask(std::unique_ptr<Task> task);

  // Transfer state from the child Process to this one, which must be currently
  // context-switched in, in proparation for the child Process being freed.
  // This involves several steps:
  //
  // - Transfer all chunks, objects, and iobjRefs
  // - Transfer over all RObjHandles.
  // - Transfer over all Tasks.
  //
  // This is done in a properly atomic way such that if another Process wants
  // to post a lambda to the owner of an RObjHandle, it won't get lost.
  void joinChild(Process& child, SkipObstackPos parentNote);

  // If the child has no valid handles and we don't have any pointers to
  // its memory, do a fast zero-root collection, aka sweep.
  void sweepChild(Process& child);

  // Makes the specified Process the current one, returning the previous one.
  // Context switching to nullptr is allowed, it means there will be no
  // current Process.
  static Ptr contextSwitchTo(Ptr process);

  // Get the Obstack for this Process. This is only legal if the Process
  // is not currently context-switched in (because if it is, the Obstack
  // is temporarily moved to tl_obstack).
  Obstack& obstackAssumingSuspended();

  Refcount currentRefcount() const;

  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;

 private:
  friend struct UnownedProcess;
  friend struct TestPrivateAccess;

  explicit Process(Context* ctx = nullptr);
  Process(UnownedProcess parent, SkipObstackPos pos, Context* ctx);
  friend void intrusive_ptr_add_ref(Process*);
  friend void intrusive_ptr_release(Process*);

  void incref();
  void decref();

  // Context-switching methods.
  void suspend();
  void resume();

  Task* pushTask(std::unique_ptr<Task> task);
  Task* pushTaskList(Task& head, Task& tail);
  Task* maybePopTask();

  // For unit tests.
  bool isSleeping() const;

  // This is a stack of tasks that need to be run. It's represented as a
  // linked list of Tasks that have been atomically pushed, ending
  // in either kIdleTag, kBusyTag or kSleepingTag depending on the
  // ownership state of the Process.
  //
  // Because we atomically prepend to the list, this list is in "most recent
  // first" order.
  std::atomic<Task*> m_tasks;

  // Baton used when the Process's owning thread is sleeping, waiting
  // for a Task to be posted.
  std::mutex m_batonLock;
  std::condition_variable m_batonSignal;
  bool m_batonCond = false;

  // WARNING: While a thread is actively running context-switched to this
  // process, this field is junk, because for speed we temporarily move it
  // to tl_obstack and make that the "active" one.
  Obstack m_obstack;

  AtomicRefcount m_refcount;

 public:
  Context* m_ctx{nullptr};

  // The parent Process that spawned us, if any. We may ask it for help
  // running a kOrphanedTag process.
  UnownedProcess m_parent;
};

// RAII guard for context switching to a Skip Process.
// On destruction, it switches back.
struct ProcessContextSwitcher final : private skip::noncopyable {
  explicit ProcessContextSwitcher(Process::Ptr newProcess)
      : m_oldProcess(Process::contextSwitchTo(std::move(newProcess))) {}

  ~ProcessContextSwitcher() {
    Process::contextSwitchTo(std::move(m_oldProcess));
  }

 private:
  Process::Ptr m_oldProcess;
};
} // namespace skip
