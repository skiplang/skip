/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/AllocProfiler.h"

#include <algorithm>
#include <iostream>
#include <exception>
#include <cstdlib>
#include <cxxabi.h>
#include <fstream>
#include <sstream>

namespace skip {

bool AllocProfiler::s_profilerEnabled = false;

namespace {

std::string demangle(const char* name) {
  int status = -4; // some arbitrary value to eliminate the compiler warning

  std::unique_ptr<char, void (*)(void*)> res{
      abi::__cxa_demangle(name, nullptr, nullptr, &status), free};

  return (status == 0) ? res.get() : name;
}

std::string getStackSym(unw_word_t addr) {
  unw_cursor_t cursor;
  unw_context_t context;
  unw_word_t offset;
  std::string pcName;

  // Initialize cursor to current frame for local unwinding.
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  unw_set_reg(&cursor, UNW_REG_IP, addr);
  char sym[512];
  if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
    pcName = std::string(sym);
  } else {
    std::cerr << " -- error: unable to obtain symbol name for PC 0x" << std::hex
              << addr << std::dec << std::endl;
  }
  return demangle(pcName.c_str());
}

// function to dump log info for an Obstack allocation site:
// arguments are they key and value in the allocation log:
// Note that this function is responsible for inserting comma
// separators and ensuring that numbers of columns matches
// header provided to dumpFileLog
void dumpObstackSiteLogInfo(
    std::ofstream& of,
    size_t bytesAllocated,
    uint64_t callCount) {
  of << "," << std::dec << bytesAllocated;
  of << "," << callCount;
  of << "," << callCount * bytesAllocated; // total bytes allocated at call site
}
} // namespace

void AllocProfiler::dumpSharedLog() {
  ObstackSymbolicAllocLog reportLog;
  accumReportLog(reportLog, s_sharedLog);

  std::string logHeader(",bytes allocated,call count,total bytes");
  dumpFileLog<size_t, uint64_t>(
      reportLog, "skip-alloc-log", logHeader, dumpObstackSiteLogInfo);
}

void AllocProfiler::logAllocation(size_t sz) {
  AllocSite<size_t> allocSite(getCallStack(), sz);
  ++m_allocLog[allocSite];
}

/*
 * We minimize lock contention by keeping allocation profiler state per
 * ObStack, which is thread-local, and lock / merge in to s_sharedLog
 * in ~AllocProfiler().
 *
 * Unfortunately, Linux and macOS are inconsistent about cleanup of
 * thread-local state for the main thread -- thread locals are
 * cleaned up before exit on macOS, but not on Linux.
 * As a workaround, we keep track of all live AllocProfiler instances,
 * and merge any remaining live instances at exit time.
 */
std::mutex AllocProfiler::s_sharedStateMutex;
skip::fast_set<AllocProfiler*> AllocProfiler::s_instances;
ObstackAllocLog AllocProfiler::s_sharedLog;

AllocProfiler::AllocProfiler() : m_allocLog() {
  std::lock_guard<std::mutex> lock{s_sharedStateMutex};
  s_instances.insert(this);
}

// pre: locked(s_sharedStateMutex)
void AllocProfiler::mergeShared() {
  // merge this allocation log into shared log:
  for (auto& v : m_allocLog) {
    s_sharedLog[v.first] += v.second;
  }
}

AllocProfiler::~AllocProfiler() {
  std::lock_guard<std::mutex> lock{s_sharedStateMutex};
  if (s_instances.erase(this) == 0) {
    // the exit handler already ran somehow, so just bail
    return;
  }
  mergeShared();
}

void AllocProfiler::onExit() {
  // first: merge any remaining live instances to shared log:
  for (auto profiler : s_instances) {
    profiler->mergeShared();
  }
  s_instances.clear();
  dumpSharedLog();
}

void AllocProfiler::init(bool isSkipCompiler) {
  uint64_t level = skip::parseEnv("SKIP_HEAP_PROFILE", 0);
  uint64_t threshold = isSkipCompiler ? 2 : 1;
  s_profilerEnabled = level >= threshold;
  if (s_profilerEnabled) {
    std::cerr << "heap profiler: enabled!" << std::endl;
    atexit(onExit);
  }
}

CallStack getCallStack(int skipFrames, int maxFrames, bool collapseRecursion) {
  CallStack callStack;
  unw_cursor_t cursor;
  unw_context_t context;

  // Initialize cursor to current frame for local unwinding.
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  int frameCount = 0;

  unw_word_t prevPC = 0;

  // Unwind frames one by one, going up the frame stack.
  while (unw_step(&cursor) > 0) {
    unw_word_t pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0) {
      break;
    }

    frameCount++;
    if (frameCount > skipFrames) {
      if (callStack.size() >= maxFrames) {
        // already have enough frames
        break;
      }
      if ((pc != prevPC) || !collapseRecursion) {
        callStack.push_back(pc);
        prevPC = pc;
      }
    }
  }
  callStack.shrink_to_fit();
  return callStack;
}

void dumpSymbolicCallStack(
    std::ostream& os,
    const SymbolicCallStack& symStack) {
  auto it = symStack.begin();
  for (int i = 0; i < skip::kKeepFrames; ++i) {
    if (it != symStack.end()) {
      os << '"' << it->c_str() << '"';
      ++it;
    } else {
      os << "-";
    }
    if (i < (skip::kKeepFrames - 1)) {
      os << ",";
    }
  }
}

void symbolizeCallStack(
    SymbolicCallStack& symStack,
    const CallStack& callStack) {
  using namespace std::placeholders;
  symStack.resize(callStack.size());

  std::transform(
      callStack.begin(), callStack.end(), symStack.begin(), getStackSym);
}
} // namespace skip
