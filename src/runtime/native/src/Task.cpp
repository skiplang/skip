/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Task.h"

namespace skip {

OneShotTask::Arbiter::Arbiter(std::unique_ptr<Task> task)
    : m_task(task.release()), m_refcount(1) {}

OneShotTask::Arbiter::~Arbiter() {
  delete m_task.load(std::memory_order_acquire);
}

OneShotTask::Arbiter::Ptr OneShotTask::Arbiter::make(
    std::unique_ptr<Task> task) {
  return Ptr(new Arbiter(std::move(task)), false);
}

void OneShotTask::Arbiter::runIfFirst() {
  if (auto task = m_task.exchange(nullptr, std::memory_order_acquire)) {
    std::unique_ptr<Task> utask{task};
    utask->run();
  }
}

bool OneShotTask::Arbiter::done() {
  return m_task.load(std::memory_order_relaxed) != nullptr;
}

void OneShotTask::Arbiter::incref() {
  m_refcount.fetch_add(1, std::memory_order_relaxed);
}

void OneShotTask::Arbiter::decref() {
  if (m_refcount.fetch_sub(1, std::memory_order_release) == 1) {
    delete this;
  }
}

void intrusive_ptr_add_ref(OneShotTask::Arbiter* arbiter) {
  arbiter->incref();
}

void intrusive_ptr_release(OneShotTask::Arbiter* arbiter) {
  arbiter->decref();
}

OneShotTask::OneShotTask(OneShotTask::Arbiter::Ptr arbiter)
    : m_arbiter(std::move(arbiter)) {}

void OneShotTask::run() {
  m_arbiter->runIfFirst();
};
} // namespace skip
