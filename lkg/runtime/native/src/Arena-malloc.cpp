/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Arena.h"

#include "skip/memory.h"
#include "skip/Obstack.h"
#include "skip/Refcount.h"
#include "skip/SmallTaggedPtr.h"
#include "skip/util.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

#if !USE_JEMALLOC
// ----------------------------------------------------------------------

#include <sys/mman.h>
#include <mutex>
#include <sanitizer/asan_interface.h>

namespace skip {

// This version is only used for ASAN builds where JeMalloc fails us.

namespace {

struct MemInfo final {
  MemInfo(size_t sz, size_t off, Arena::Kind k)
      : size(sz), offset(off), kind(k) {}

  bool isArenaAllocated() const {
    return offset != 0;
  }

  size_t size;
  size_t offset;
  Arena::Kind kind;
};

struct State final {
  std::map<const void*, MemInfo> m_known;

  ~State() {
    for (auto& i : m_known) {
      if (i.second.isArenaAllocated()) {
        auto p = mem::sub(i.first, i.second.offset);
        ::free(const_cast<void*>(p));
      }
    }
  }
};

static State g_state;
static std::mutex g_stateMutex;

std::map<const void*, MemInfo>::iterator _findMemBlock(
    State& lockedState,
    void* p) {
  auto i = lockedState.m_known.find(p);
  if (i != lockedState.m_known.end())
    return i;

  // ERROR - We don't know about this memory pointer.

  // One of three cases here:
  // 1. This is a pointer into the middle of a block we allocated.
  // 2. This is a pointer allocated with 'malloc'.
  // 3. This is a legitimate 'bad' pointer.
  //
  // So we free the pointer twice.  If it's case 1 or 3 then the first free
  // will catch it.  If it's case 2 then the second free will catch it.
  ::free(p);
  ::free(p);
  abort();
}

MemInfo _clearMemoryKind(const void* p) {
  std::lock_guard<std::mutex> lock(g_stateMutex);
  auto i = _findMemBlock(g_state, const_cast<void*>(p));
  auto res = i->second;
  g_state.m_known.erase(i);
  return res;
}

void _setMemoryKind(
    const void* p,
    size_t sz,
    bool managed,
    int offset,
    Arena::Kind kind) {
  std::lock_guard<std::mutex> lock(g_stateMutex);
  g_state.m_known.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(p),
      std::forward_as_tuple(sz, offset, kind));
}
} // namespace

void* Arena::allocAligned(size_t sz, size_t align, Kind kind) {
  size_t offset = std::max<size_t>(align, 8);
  void* p =
      ((align == 1) ? ::malloc(sz + 2 * offset)
                    : skip::allocAligned(sz + 2 * offset, align));
  if (!p)
    throw std::bad_alloc();
  ASAN_POISON_MEMORY_REGION(p, offset);
  p = mem::add(p, offset);
  ASAN_POISON_MEMORY_REGION(mem::add(p, sz), offset);

  _setMemoryKind(p, sz, true, offset, kind);
  return p;
}

void Arena::free(void* p) {
  auto info = _clearMemoryKind(p);
  p = mem::sub(p, info.offset);
  ::free(p);
}

void Arena::free(void* p, Kind /*kind*/) {
  free(p);
}

Arena::Kind Arena::rawMemoryKind(const void* p) {
  std::lock_guard<std::mutex> lock(g_stateMutex);
  if (g_state.m_known.empty())
    return Arena::Kind::unknown;
  auto i = g_state.m_known.upper_bound(p);
  if (i == g_state.m_known.begin()) {
    return Arena::Kind::unknown;
  }
  --i;
  if (!mem::within(p, i->first, i->second.size)) {
    // If this fires then they asked for a pointer inside the pre-memory fence
    assert(!mem::within(
        p, mem::sub(i->first, i->second.offset), i->second.offset));
    // If this fires then they asked for a pointer inside the post-memory fence
    assert(
        !mem::within(p, mem::add(i->first, i->second.size), i->second.offset));
    return Arena::Kind::unknown;
  }

  return i->second.kind;
}

void Arena::setMemoryKind(const void* start, const void* end, Kind kind) {
  if (kind == Arena::Kind::unknown) {
    auto info ATTR_UNUSED = _clearMemoryKind(start);
    assert(!info.isArenaAllocated() && info.size == mem::sub(end, start));
  } else {
    _setMemoryKind(start, mem::sub(end, start), false, 0, kind);
  }
}

void Arena::init() {}
} // namespace skip

// ----------------------------------------------------------------------
#endif // !USE_JEMALLOC
