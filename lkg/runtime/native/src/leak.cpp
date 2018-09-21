/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/leak.h"
#include "skip/String.h"
#include "skip/System-extc.h"

#include <boost/version.hpp>
#if BOOST_VERSION >= 105600
#include <boost/core/demangle.hpp>
#endif

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
#if BOOST_VERSION >= 105600
      boost::core::scoped_demangled_name dname{c->m_className};
      std::string name{dname.get() ? dname.get() : "<unknown>"};
#else
      std::string name{c->m_className};
#endif

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
