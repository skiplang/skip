/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// local unwinding only; may improve perf.
// See libunwind(3) man page for details
// https://www.nongnu.org/libunwind/man/libunwind(3).html
#define UNW_LOCAL_ONLY

#include "skip/System.h"
#include "skip/util.h"
#include "skip/map.h"
#include "skip/set.h"

#include <functional>
#include <cstdlib>
#include <mutex>
#include <iostream>
#include <fstream>
#include <libunwind.h>
#include <boost/functional/hash.hpp>

/*
 * Simple heap profiling, based on logging all calls to ObStack::alloc.
 *
 * The heap profiler is controlled by the environment variable
 * SKIP_HEAP_PROFILE
 *
 * If not set, logging is disabled.
 * If set to 1, will perform heap profile logging for all Skip programs
 * other than the Skip compiler.
 * If set to 2, will log for all programs AND the compiler itself.
 *
 * If heap profiler logging is enabled, a diagnostic is printed
 * to stderr at startup indicating that logging is enabled, and at
 * exit giving the path name to the tmp file with the CSV of the
 * aggregated allocation log data.
 *
 */

namespace skip {

/* We'll identify allocation sites by their top N stack addresses and allocation
 * size.
 */
using CallStack = std::vector<unw_word_t>;

/*
 * Impl note: Originally just used CallStack instead of SymbolicCallStack
 * to represent an allocation site, but distinct return addresses
 * within the same function resulted in more fine grained allocation
 * sites that didn't seem super helpful for analysis.
 *
 * impl note: Tried using boost::flyweight<std::string> instead of string to
 * have an intern'ed string for each entry.
 * Somewhat surprisingly that turned out to be a touch slower
 * than just using std::string directly.
 */
using Symbol = std::string;
using SymbolicCallStack = std::vector<Symbol>;

template <typename K>
using AllocSite = std::pair<CallStack, K>;

template <typename K>
using SymbolicAllocSite = std::pair<SymbolicCallStack, K>;

// For each AllocSite, we'll simply log number of calls:
template <typename K, typename V>
using AllocLog = skip::fast_map<AllocSite<K>, V, boost::hash<AllocSite<K>>>;

template <typename K, typename V>
using SymbolicAllocLog =
    skip::fast_map<SymbolicAllocSite<K>, V, boost::hash<SymbolicAllocSite<K>>>;

using ObstackAllocLog = AllocLog<size_t, uint64_t>;
using ObstackSymbolicAllocLog = SymbolicAllocLog<size_t, uint64_t>;

void dumpSymbolicCallStack(std::ostream& os, const SymbolicCallStack& symStack);

template <typename K, typename V>
void dumpFileLog(
    SymbolicAllocLog<K, V> const& reportLog,
    std::string const& fnamePrefix,
    std::string const& logInfoHeader,
    void dumpLogInfo(std::ofstream&, K, V)) {
  std::string tmps = "/tmp/" + fnamePrefix + "-XXXXXX.csv";
  std::vector<char> tmpNameChars(tmps.c_str(), tmps.c_str() + tmps.size() + 1u);
  int fd = mkstemps(&tmpNameChars[0], 4);
  if (fd < 0) {
    // something went wrong, bail...
    std::cerr << "dump_allocLog: could not open output log file (ignoring)\n";
    return;
  }
  std::string tmpName(&tmpNameChars[0]);
  std::ofstream of(tmpName); // re-opens file, but should be fine
  close(fd);
  if (!of) {
    std::cerr << "Could not create output stream for tmp file " << tmpName
              << '\n';
    return;
  }
  of << "pc0,pc1,pc2,pc3" << logInfoHeader << '\n';

  std::cerr << "Dumping log of size " << reportLog.size() << '\n';
  for (const auto& report : reportLog) {
    auto& site = report.first;
    dumpSymbolicCallStack(of, site.first);
    dumpLogInfo(of, site.second, report.second);
    of << '\n';
  }
  std::cerr << "heap profiler: wrote allocation log to " << tmpName << '\n';
  of.close();
}

void symbolizeCallStack(
    SymbolicCallStack& symStack,
    const CallStack& callStack);

// Symbolize allocation sites in allocLog, accumulate into reportLog:
template <class K, class V>
void accumReportLog(
    SymbolicAllocLog<K, V>& reportLog,
    const AllocLog<K, V>& allocLog) {
  for (const auto& alloc : allocLog) {
    auto& allocSite = alloc.first;
    SymbolicCallStack scs;
    symbolizeCallStack(scs, allocSite.first);
    auto symbolicAllocSite = SymbolicAllocSite<K>(scs, allocSite.second);
    reportLog[symbolicAllocSite] += alloc.second;
  }
}

const int kDropFrames = 2; // drop this many frames from callstack
const int kKeepFrames = 4; // keep this many frames

CallStack getCallStack(
    int skipFrames = kDropFrames,
    int maxFrames = kKeepFrames,
    bool collapseRecursion = true);

struct AllocProfiler {
  AllocProfiler();
  ~AllocProfiler();

  inline static bool enabled() {
    return s_profilerEnabled;
  }

  void logAllocation(size_t sz);

  static void init(bool isSkipCompiler);

 private:
  ObstackAllocLog m_allocLog; // allocation log for a specific Obstack instance

  // merge contents of this into shared log data:
  // pre: locked(s_sharedStateMutex)
  void mergeShared();

  static void dumpSharedLog();
  static void onExit();

  static bool s_profilerEnabled;

  static std::mutex s_sharedStateMutex;
  static skip::fast_set<AllocProfiler*> s_instances;
  static ObstackAllocLog s_sharedLog;
};
} // namespace skip
