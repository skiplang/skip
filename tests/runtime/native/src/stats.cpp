/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/stats.h"

#include <iostream>

#include <folly/json.h>

namespace skip {

void dumpInternSiteLogInfo(
    std::ofstream& of,
    const char* type,
    ObjectStats::Counters c) {
  of << "," << type;
  of << "," << c.m_count;
  of << "," << c.m_size;
}

void ObjectStats::accrue(const RObj& robj) {
  auto& type = robj.type();
  const uint64_t size = type.internedMetadataByteSize() + robj.userByteSize();

  std::lock_guard<std::mutex> lock(m_mutex);

  auto result =
      m_counters.insert(std::make_pair(type.name(), Counters(1, size)));

  if (!result.second) {
    // It was already present, need to update the existing entry.
    auto it = result.first;
    auto old = it->second;
    it->second = Counters(old.m_count + 1, old.m_size + size);
  }

  // A version of above for m_internLog:
  InternSite internSite(getCallStack(), type.name());
  auto internLogResult =
      m_internLog.insert(std::make_pair(internSite, Counters(1, size)));

  if (!internLogResult.second) {
    // It was already present, need to update the existing entry.
    auto it = internLogResult.first;
    it->second += Counters(1, size);
  }
}

void ObjectStats::dump(std::ostream& out, bool sortByCount) {
  using Entry = std::pair<const char*, Counters>;

  std::unique_lock<std::mutex> lock(m_mutex);
  std::vector<Entry> counters(m_counters.begin(), m_counters.end());
  lock.unlock();

  std::sort(
      counters.begin(),
      counters.end(),
      [sortByCount](const Entry& c1, const Entry& c2) {
        const auto count1 = c1.second.m_count;
        const auto count2 = c2.second.m_count;

        if (sortByCount && count1 != count2) {
          return count1 > count2;
        }

        const auto size1 = c1.second.m_size;
        const auto size2 = c2.second.m_size;
        if (size1 != size2) {
          return size1 > size2;
        } else if (count1 != count2) {
          return count1 > count2;
        } else {
          // Break ties by name.
          return strcmp(c1.first, c2.first) < 0;
        }
      });

  // Dump out in JSON format.

  folly::json::serialization_opts opts;

  out << "[\n";
  for (const auto& c : counters) {
    out << "  {";
    if (sortByCount) {
      out << " \"count\": " << c.second.m_count;
      out << " \"size\": " << c.second.m_size;
    } else {
      out << " \"size\": " << c.second.m_size;
      out << " \"count\": " << c.second.m_count;
    }

    std::string typeName;
    folly::json::escapeString(c.first, typeName, opts);
    out << " \"type\": " << typeName;

    out << " }\n";
  }

  out << "]\n";

  InternSymbolicAllocLog reportLog;
  accumReportLog(reportLog, m_internLog);
  std::string logHeader(",type,count,total bytes");
  dumpFileLog(reportLog, "skip-intern-log", logHeader, dumpInternSiteLogInfo);
}
} // namespace skip
