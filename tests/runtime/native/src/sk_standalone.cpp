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

#include <folly/Demangle.h>
#include <folly/init/Init.h>
#ifndef __APPLE__
#if FOLLY_USE_SYMBOLIZER
#include <folly/experimental/symbolizer/SignalHandler.h>
#endif
#else
#include <exception>
#include <cstdlib>
#include <libunwind.h>
#include <cxxabi.h>
#endif

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

#include <folly/Format.h>

#include <boost/filesystem.hpp>

#if FACEBOOK
#include "common/init/Init.h"
#endif

#ifdef __APPLE__
namespace {
void osxTerminate() {
  try {
    std::exception_ptr eptr = std::current_exception();
    if (eptr) {
      std::rethrow_exception(eptr);
    }
  } catch (std::exception& e) {
    std::cerr << "*** Uncaught exception of type "
              << folly::demangle(typeid(e).name()) << ": " << e.what()
              << std::endl;
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

namespace {

std::string getFieldType(const svmi::TypeTable& table, const svmi::Desc& desc) {
  switch (desc.paramType) {
    case svmi::ParamType::voidType:
      return "void";
    case svmi::ParamType::boolean:
      return "bool";
    case svmi::ParamType::int64:
      return "int64";
    case svmi::ParamType::float64:
      return "float64";
    case svmi::ParamType::string:
      return "string";
    case svmi::ParamType::mixed:
      return "mixed";
    case svmi::ParamType::nullable_boolean:
      return "?bool";
    case svmi::ParamType::nullable_int64:
      return "?int64";
    case svmi::ParamType::nullable_float64:
      return "?float64";
    case svmi::ParamType::nullable_string:
      return "?string";
    case svmi::ParamType::nullableMask:
      return "???";

    case svmi::ParamType::object:
    case svmi::ParamType::nullable_object: {
      auto const opt = (desc.paramType == svmi::ParamType::array) ? "" : "?";

      std::string className;
      if ((uint64_t)desc.classId < table.classes->arraySize()) {
        className = table.classes->at(desc.classId)->name.toCppString();
      } else {
        className = folly::sformat("BAD #{}", desc.classId);
      }

      return folly::sformat("{}object {}", opt, className);
    }

    case svmi::ParamType::array:
    case svmi::ParamType::nullable_array: {
      // vec, dict, keyset or shape
      auto const opt = (desc.paramType == svmi::ParamType::array) ? "" : "?";

      if ((uint64_t)desc.classId < table.classes->arraySize()) {
        // shape
        std::string className =
            table.classes->at(desc.classId)->name.toCppString();
        return folly::sformat("shape {}{}", opt, className);
      }

      if (desc.classId == (int)svmi::ClassIdMagic::vec) {
        return folly::sformat(
            "{}vec<{}>", opt, getFieldType(table, *desc.targs->at(0)));
      }

      if (desc.classId == (int)svmi::ClassIdMagic::dict) {
        return folly::sformat(
            "{}dict<{}, {}>",
            opt,
            getFieldType(table, *desc.targs->at(0)),
            getFieldType(table, *desc.targs->at(1)));
      }

      if (desc.classId == (int)svmi::ClassIdMagic::keyset) {
        return folly::sformat(
            "{}keyset<{}>", opt, getFieldType(table, *desc.targs->at(0)));
      }

      return folly::sformat("{}array unknown #{}", opt, desc.classId);
    }
  }

  return folly::sformat("(no case {})", (int)desc.paramType);
}

void dumpFieldEntry(
    const svmi::TypeTable& table,
    const std::string& clsName,
    const svmi::Field& e,
    size_t fieldIdx) {
  printf(
      "    %3zd: %s.%s: %s\n",
      fieldIdx,
      clsName.c_str(),
      e.name.toCppString().c_str(),
      getFieldType(table, *e.typ).c_str());
}

void dumpTypeTable() {
  auto& table = *SKIPC_hhvmTypeTable();

  printf("Classes:\n");
  size_t classFieldIdx = 0;
  size_t shapeFieldIdx = 0;
  for (size_t i = 0; i < table.classes->arraySize(); ++i) {
    printf("  %3zd: ", i);
    auto& e = *table.classes->at(i);
    switch (e.kind) {
      case svmi::ClassKind::base:
        printf("base");
        break;
      case svmi::ClassKind::copyClass:
        printf("class (copy)");
        break;
      case svmi::ClassKind::proxyClass:
        printf("class (proxy)");
        break;
      case svmi::ClassKind::copyShape:
        printf("shape (copy)");
        break;
      case svmi::ClassKind::proxyShape:
        printf("shape (proxy)");
        break;
    }
    auto clsName = e.name.toCppString();
    printf(" %s\n", clsName.c_str());
    if (e.kind == svmi::ClassKind::base)
      continue;
    for (size_t j = 0; j < e.fields->arraySize(); ++j) {
      const size_t idx =
          ((e.kind == svmi::ClassKind::copyClass ||
            e.kind == svmi::ClassKind::proxyClass)
               ? classFieldIdx++
               : shapeFieldIdx++);
      dumpFieldEntry(table, clsName, *e.fields->at(j), idx);
    }
  }
}

std::string computeMemocacheName(std::string argv0) {
  using path = boost::filesystem::path;
  path p = boost::filesystem::absolute(path(argv0));
  path dir = p.parent_path();
  path name = "." + p.filename().string() + ".cache";
  return (dir / name).string();
}
} // namespace

int main(int argc, char** argv) {
#if FACEBOOK
  int fbArgc = 1;
  std::array<char*, 1> fbArgv{{argv[0]}};
  char** pfbArgv = fbArgv.data();
  facebook::initFacebook(&fbArgc, &pfbArgv);
#else
  int follyArgc = 1;
  std::array<char*, 1> follyArgv{{argv[0]}};
  char** pfollyArgv = follyArgv.data();
  folly::init(&follyArgc, &pfollyArgv, false);

#ifndef __APPLE__
#if FOLLY_USE_SYMBOLIZER
  folly::symbolizer::installFatalSignalHandler();
#endif
#else
  std::set_terminate(osxTerminate);
#endif
#endif

  std::string memocacheName = computeMemocacheName(argv[0]);

  const bool watch = (argc > 1 && strcmp(argv[1], "--watch") == 0);
  if (watch) {
    // Slide over arguments. Note that this slides over the final nullptr.
    --argc;
    memmove(&argv[1], &argv[2], argc * sizeof(argv[0]));
  }

  skip::initializeSkip(argc, argv, watch);

  if (false)
    dumpTypeTable();

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

    // For now memocache deserialization is opt-in
    if (getenv("SKIP_MEMO_READ") != nullptr) {
      try {
        skip::MemoSerde::deserializeMemoCache(memocacheName);
      } catch (std::exception& e) {
        // catch errors during serialization and just continue
        fprintf(
            stderr, "Ignoring error during deserialization: %s\n", e.what());
      }
    }

    if (!watch) {
      skip::Obstack::PosScope P;
      skip_main();
      process->drainEverythingSleepingIfNecessary();

      // For now memocache serialization is opt-in
      if (getenv("SKIP_MEMO_WRITE") != nullptr) {
        skip::MemoSerde::serializeMemoCache(memocacheName + ".tmp");
        boost::filesystem::rename(memocacheName + ".tmp", memocacheName);
      }
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
