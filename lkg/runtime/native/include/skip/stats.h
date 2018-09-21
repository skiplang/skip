/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "objects.h"
#include "map.h"
#include "AllocProfiler.h"

#include <iostream>
#include <mutex>

namespace skip {

using InternSite = AllocSite<const char*>;

struct ObjectStats {
  // Add robj count and size into the running per-type totals.
  void accrue(const RObj& robj);

  void dump(std::ostream& out, bool sortByCount);

 private:
  struct Counters {
    Counters(uint64_t count, uint64_t size) : m_count(count), m_size(size) {}

    Counters() : m_count(0), m_size(0) {}
    Counters(const Counters& src) = default;
    Counters(Counters&&) = default;
    Counters& operator=(const Counters& other) = default;
    Counters& operator=(Counters&& other) noexcept = default;

    Counters& operator+=(const Counters& rhs) {
      m_count += rhs.m_count;
      m_size += rhs.m_size;
      return *this;
    }
    friend Counters operator+(Counters lhs, const Counters& rhs) {
      lhs += rhs;
      return lhs;
    }

    uint64_t m_count;
    uint64_t m_size;
  };

  friend void dumpInternSiteLogInfo(
      std::ofstream& of,
      const char* type,
      ObjectStats::Counters c);

  // based on AllocSite from AllocProfiler.h:
  // using InternSite = std::pair<CallStack, const char *>;
  using InternAllocLog = AllocLog<const char*, Counters>;
  using InternSymbolicAllocLog = SymbolicAllocLog<const char*, Counters>;

  std::mutex m_mutex;

  // Type name pointers are interned: just hash & compare the raw pointer.
  skip::fast_map<const char*, Counters> m_counters;

  InternAllocLog m_internLog;
};
} // namespace skip
