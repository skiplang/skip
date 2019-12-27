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

using namespace skip;

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

static std::mutex s_globalLock;
static ReactiveGlobalCache s_globalCache;

thread_local Transaction* t_currentTransaction;
} // anonymous namespace

SkipInt SKIP_Reactive_nextReactiveGlobalCacheID() {
  std::lock_guard<std::mutex> lock(s_globalLock);
  auto locked = &s_globalCache;
  return locked->m_nextID++;
}

void SKIP_Reactive_reactiveGlobalCacheSet(SkipInt id, String key, RObj* value) {
  std::lock_guard<std::mutex> lock(s_globalLock);
  auto locked = &s_globalCache;
  if (auto txn = t_currentTransaction) {
    locked->set(*txn, id, key, value);
  } else {
    Transaction localTxn;
    locked->set(localTxn, id, key, value);
  }
}

IObj* SKIP_Reactive_reactiveGlobalCacheGet(SkipInt id, String key) {
  std::lock_guard<std::mutex> lock(s_globalLock);
  auto locked = &s_globalCache;
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
  std::lock_guard<std::mutex> lock(s_globalLock);
  s_globalCache.cleanup();
}

SkipInt SKIP_Reactive_unsafe(RObj* value) {
  return (SkipInt)value;
}
