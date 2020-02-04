/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/leak.h"
#include "skip/String.h"
#include "skip/System-extc.h"

// @lint-ignore HOWTOEVEN1
#include <iostream>

namespace skip {

#if SKIP_PARANOID
LeakCounter::LeakCounter(const char* className)
    : m_count(0), m_className(className), m_next(s_allLeakCounters) {
  s_allLeakCounters = this;
}

LeakCounter* LeakCounter::s_allLeakCounters;
#endif

void assertLeakCountersZero() {
#if SKIP_PARANOID
  bool success = true;

  for (auto c = LeakCounter::s_allLeakCounters; c; c = c->m_next) {
    if (auto count = c->count()) {
      std::string name{c->m_className};

      std::cerr << "Detected memory leak of " << count << " instance"
                << ((count != 1) ? "s" : "") << " of type " << name
                << std::endl;
      success = false;
    }
  }

  if (!success) {
    abort();
  }
#endif
}
} // namespace skip

using namespace skip;

SkipInt SKIP_Debug_getLeakCounter(String classname) {
#if SKIP_PARANOID
  auto needle = classname.toCppString();
  for (auto c = LeakCounter::s_allLeakCounters; c; c = c->m_next) {
    // counter classnames are mangled, hence strstr for now.
    if (strstr(c->m_className, needle.c_str())) {
      return c->m_count.load();
    }
  }
#endif
  return 0;
}
