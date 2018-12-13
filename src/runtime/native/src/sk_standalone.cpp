/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iostream>

// <algorithm> is necessary to solve a jemalloc #include ordering problem
// where posix_memalign does or doesn't have a throw declaration.
#include <algorithm>

#include <exception>
#include <cstdlib>
#include <libunwind.h>
#include <cxxabi.h>

#include "skip/Exception.h"
#include "skip/Obstack.h"
#include "skip/Process.h"
#include "skip/Reactive.h"
#include "skip/String.h"
#include "skip/System.h"
#include "skip/external.h"
#include "skip/leak.h"
#include "skip/memoize.h"
#include "skip/parallel.h"
#include "skip/plugin-extc.h"

#ifdef __APPLE__
namespace {
void osxTerminate() {
  try {
    std::exception_ptr eptr = std::current_exception();
    if (eptr) {
      std::rethrow_exception(eptr);
    }
  } catch (std::exception& e) {
    std::cerr << "*** Uncaught exception of type " << typeid(e).name() << ": "
              << e.what() << std::endl;
  }
  std::cerr << "*** Stack trace:" << std::endl;
  skip::printStackTrace();
  // We used to call abort(), but Folly installs the Google glog fatal handler,
  // which dumps a less-than-useful stack trace on OS/X.
  // Let's just exit explicitly with a failure exit code here to reduce
  // confusion.
  std::cerr << "*** PID " << getpid() << " exiting" << std::endl;
  exit(1);
}
} // namespace
#endif // __APPLE__

namespace skiptest {
// Make these available to tests...
const svmi::FunctionSignature* globals;
} // namespace skiptest

extern "C" {
extern void skip_main(void);

// Link these stubs weakly so tests can override them.
#define WEAK_LINKAGE __attribute__((weak))

skip::String WEAK_LINKAGE SKIP_string_extractData(HhvmString /*handle*/) {
  abort();
}
void WEAK_LINKAGE SKIP_HHVM_incref(skip::HhvmHandle* /*wrapper*/) {
  abort();
}
void WEAK_LINKAGE SKIP_HHVM_decref(skip::HhvmHandle* /*wrapper*/) {
  abort();
}
skip::String WEAK_LINKAGE
SKIP_HHVM_Object_getType(skip::HhvmHandle* /*wrapper*/) {
  abort();
}

SkipRetValue WEAK_LINKAGE SKIP_HHVM_callFunction(
    skip::HhvmHandle* /*object*/,
    skip::String /*function*/,
    skip::String /*paramTypes*/,
    ...) {
  abort();
}

size_t WEAK_LINKAGE SKIPC_buildHash(void) {
  return 1;
}
} // extern "C"

int main(int argc, char** argv) {
#ifdef __APPLE__
  std::set_terminate(osxTerminate);
#endif

  const bool watch = (argc > 1 && strcmp(argv[1], "--watch") == 0);
  if (watch) {
    // Slide over arguments. Note that this slides over the final nullptr.
    --argc;
    memmove(&argv[1], &argv[2], argc * sizeof(argv[0]));
  }

  skip::initializeSkip(argc, argv, watch);

  auto process = skip::Process::make();
  skip::ProcessContextSwitcher guard{process};

  int status = 0;

  try {
    {
      skip::Obstack::PosScope P;
      skiptest::globals = reinterpret_cast<const svmi::FunctionSignature*>(
          SKIP_initializeSkip());
    }

    skip::setNumThreads(skip::computeCpuCount());

    if (!watch) {
      skip::Obstack::PosScope P;
      skip_main();
      process->drainEverythingSleepingIfNecessary();

    } else {
      // TODO: Should we continue even in the face of uncaught exceptions?
      while (true) {
        skip::Obstack::PosScope p2;

        // Invoke the Skip main() and collect any reactive dependencies.
        auto watcher = skip::watchDependencies([]() { skip_main(); });

        process->drainEverythingSleepingIfNecessary();

        if (!watcher) {
          // No reactive dependencies found.
          fprintf(
              stderr, "(Exiting watch mode: no reactive dependencies found)\n");
          break;
        }

        // Wait for something relevant to change then re-run main.
        watcher->getFuture().wait();
      }
    }
  } catch (skip::SkipExitException& e) {
    // Because we were handling an exception the PosScopes above didn't clear
    // the Obstack - so we need to force clear to ensure the Obstack is cleared
    // before we exit.
    skip::Obstack::cur().collect();
    status = e.m_status;
  }

  if (skip::kEnableInternStats) {
    skip::dumpInternStats();
  }

  skip::Reactive::shutdown();
  while (skip::discardLeastRecentlyUsedInvocation()) {
    // no-op
  }
  skip::assertLeakCountersZero();

  return status;
}
