/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "util.h"

#include <atomic>
#include <memory>

#include <boost/intrusive_ptr.hpp>

namespace skip {

// NOTE: These are here so Obstack.h can see them without causing
// a circular #include by pussing in all of Process.h.
void intrusive_ptr_add_ref(Process* p);
void intrusive_ptr_release(Process* p);
using ProcessPtr = boost::intrusive_ptr<Process>;

// Work that a Process can be asked to do.
struct Task : private skip::noncopyable {
  virtual ~Task() = default;
  virtual void run() = 0;

  Task* m_next;
};

// A Task that runs a void() lambda, std::function<void(void)>, etc.
// We make it generic rather than using a std::function on the theory
// that we can store a lambda more efficiently using its real type.
template <typename T>
struct LambdaTask final : Task {
  explicit LambdaTask(T&& func) : m_func(std::move(func)) {}

  void run() override {
    m_func();
  }

 private:
  T m_func;
};

// This is a Task intended to give multiple Processes the chance to
// run one underlying Task. Whoever gets there first will run the underlying
// Task, and the rest will just see it's already been taken and do nothing.
struct OneShotTask final : Task {
  // A first come, first serve guard around the underlying Task we want
  // to run. Many OneShotTasks can point to the same Arbiter.
  struct Arbiter final : skip::noncopyable {
    using Ptr = boost::intrusive_ptr<Arbiter>;
    static Ptr make(std::unique_ptr<Task> task);

    // Return the underlying Task if this is the first call to this method,
    // else a null unique_ptr.
    void runIfFirst();

    // Has the underlying Task already been taken? (this can of course
    // transition from false -> true at any time, but once true it stays true).
    bool done();

   private:
    explicit Arbiter(std::unique_ptr<Task> task);
    ~Arbiter();

    friend void intrusive_ptr_add_ref(Arbiter*);
    friend void intrusive_ptr_release(Arbiter*);

    void incref();
    void decref();

    std::atomic<Task*> m_task;
    std::atomic<size_t> m_refcount;
  };

  void run() override;

  OneShotTask(Arbiter::Ptr arbiter);

  Arbiter::Ptr m_arbiter;
};

void intrusive_ptr_add_ref(OneShotTask::Arbiter* arbiter);
void intrusive_ptr_release(OneShotTask::Arbiter* arbiter);
} // namespace skip
