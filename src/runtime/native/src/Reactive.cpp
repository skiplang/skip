/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Reactive-extc.h"
#include "skip/Reactive.h"
#include "skip/util.h"
#include "skip/memoize.h"

#include <folly/experimental/FunctionScheduler.h>
#include <folly/Synchronized.h>

using namespace skip;

namespace {

struct ReactiveTimer {
  static ReactiveTimer& get() {
    static ReactiveTimer singleton;
    return singleton;
  }

  int64_t getReactiveValue(String id_, double intervalInSeconds) {
    std::string id = id_.toCppString();

    auto it = m_cells.find(id);
    if (it == m_cells.end()) {
      auto unit = std::make_shared<TimeUnit>();
      it = m_cells.emplace(id, unit).first;

      m_scheduler.addFunction(
          [unit]() {
            ++unit->m_value;
            Transaction txn;
            txn.assign(unit->m_cell, MemoValue(unit->m_value));
          },
          std::chrono::milliseconds(int64_t(intervalInSeconds * 1000)),
          id);
    }

    return it->second->m_cell.invocation()
        ->asyncEvaluate()
        .get()
        .m_value.asInt64();
  }

 private:
  folly::FunctionScheduler m_scheduler;

  struct TimeUnit {
    int64_t m_value = 0;
    Cell m_cell{MemoValue(0)};
  };

  skip::fast_map<std::string, std::shared_ptr<TimeUnit>> m_cells;

  ReactiveTimer() {
    m_scheduler.start();
  }
};
} // anonymous namespace

SkipInt SKIP_Reactive_reactiveTimer(String id, SkipFloat intervalInSeconds) {
  return ReactiveTimer::get().getReactiveValue(id, intervalInSeconds);
}

namespace {

struct ReactiveGlobalCache {
  int64_t m_nextID = 0;

  void set(Transaction& txn, int64_t id, String keyStr, RObj* value) {
    KeyType key{id, keyStr.toCppString()};
    auto ivalue = Obstack::cur().intern(value).asPtr();

    auto it = m_cells.find(key);
    if (it == m_cells.end()) {
      it = constructEmptyCell(key);
    }

    txn.assign(it->second, MemoValue(ivalue, MemoValue::Type::kIObj));
  }

  IObj* get(int64_t id, String keyStr) {
    KeyType key{id, keyStr.toCppString()};
    auto it = m_cells.find(key);
    if (it == m_cells.end()) {
      it = constructEmptyCell(key);
    }

    return it->second.invocation()->asyncEvaluate().get().m_value.asIObj();
  }

  void cleanup() {
    m_cells.clear();
  }

 private:
  // using node_map because Cell is noncopyable.
  using KeyType = std::pair<int64_t, std::string>;
  skip::node_map<KeyType, Cell> m_cells;

  skip::node_map<KeyType, Cell>::iterator constructEmptyCell(KeyType key) {
    return m_cells
        .emplace(
            std::piecewise_construct,
            std::make_tuple(key),
            std::make_tuple(MemoValue(nullptr)))
        .first;
  }
};

folly::Synchronized<ReactiveGlobalCache, std::mutex>::LockedPtr
getReactiveGlobalCache() {
  static folly::Synchronized<ReactiveGlobalCache, std::mutex> s_global;
  return s_global.lock();
}

thread_local Transaction* t_currentTransaction;
} // anonymous namespace

SkipInt SKIP_Reactive_nextReactiveGlobalCacheID() {
  auto locked = getReactiveGlobalCache();
  return locked->m_nextID++;
}

void SKIP_Reactive_reactiveGlobalCacheSet(SkipInt id, String key, RObj* value) {
  auto locked = getReactiveGlobalCache();
  if (auto txn = t_currentTransaction) {
    locked->set(*txn, id, key, value);
  } else {
    Transaction localTxn;
    locked->set(localTxn, id, key, value);
  }
}

IObj* SKIP_Reactive_reactiveGlobalCacheGet(SkipInt id, String key) {
  auto locked = getReactiveGlobalCache();
  return locked->get(id, key);
}

void SKIP_Reactive_withTransaction(RObj* callback) {
  auto code = callback->vtable().getFunctionPtr();

  if (t_currentTransaction) {
    code(callback);
  } else {
    Transaction txn;
    t_currentTransaction = &txn;
    try {
      code(callback);
    } catch (...) {
      t_currentTransaction = nullptr;
      throw;
    }
    t_currentTransaction = nullptr;
  }
}

void Reactive::shutdown() {
  getReactiveGlobalCache()->cleanup();
}
