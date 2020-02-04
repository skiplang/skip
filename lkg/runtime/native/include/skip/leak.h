/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <atomic>
#include <typeinfo>

namespace skip {

#if SKIP_PARANOID

// Each type of LeakChecker creates a static instance of this counter.
// Their constructors chain them together into a global list checked by
// assertLeakCountersZero().
struct LeakCounter : private skip::noncopyable {
  explicit LeakCounter(const char* className);

  size_t count() const {
    return m_count.load(std::memory_order_relaxed);
  }

  std::atomic<size_t> m_count;
  const char* const m_className;
  LeakCounter* const m_next;

  static LeakCounter* s_allLeakCounters;
};

#endif

template <typename Derived>
struct LeakChecker {
  LeakChecker(const LeakChecker&) = delete;
  LeakChecker(LeakChecker&&) = delete;
  LeakChecker& operator=(const LeakChecker&) = delete;
  LeakChecker& operator=(LeakChecker&&) = delete;

  LeakChecker() {
#if SKIP_PARANOID
    adjustCounter(1);
#endif
  }

#if SKIP_PARANOID
  ~LeakChecker() {
    adjustCounter(-1);
  }

  static void adjustCounter(int delta) {
    s_counter.m_count.fetch_add(delta, std::memory_order_relaxed);
  }

 private:
  static LeakCounter s_counter;
#endif
};

#if SKIP_PARANOID
template <typename Derived>
LeakCounter LeakChecker<Derived>::s_counter{typeid(Derived).name()};
#endif

void assertLeakCountersZero();
} // namespace skip
