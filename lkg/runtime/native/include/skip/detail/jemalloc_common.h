/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef ARENA_PRIVATE
#error Only include this file from within Arena.cpp
#endif

#ifndef __APPLE__
#define je_mallctlnametomib mallctlnametomib
#define je_mallctlbymib mallctlbymib
#define je_dallocx dallocx
#define je_mallocx mallocx
#endif

#include <array>
#include <stdio.h>

#define posix_memalign posix_memalign2
#include "jemalloc/jemalloc.h"

#if (JEMALLOC_VERSION_MAJOR < 4) ||                                    \
    ((JEMALLOC_VERSION_MAJOR == 4) && (JEMALLOC_VERSION_MINOR < 5)) || \
    (JEMALLOC_VERSION_MAJOR > 5)
#error Bad JeMalloc version - Skip only compiles against JeMalloc 4.5 - 5.x
#endif

namespace skip {
namespace {
namespace je {
// Everything in the 'je' namespace is just c++ wrappers for jemalloc.
// See jemalloc for documentation.

// The Mib stuff ("Management Information Base") is a way of looking up the
// jemalloc command strings a single time in advance rather than doing string
// compares every time.
struct Mib {
  explicit Mib(const char* name) : m_name(name) {
    count_parts(name);
    assert(m_size <= MAX_MIB_SIZE);
    int res = ::je_mallctlnametomib(name, &m_parts[0], &m_size);
    if (res) {
      std::string nameStr(name);
      throw std::runtime_error("mallctlnametomib(" + nameStr + ") failed");
    }
  }

  Mib(const Mib& o) = default;
  Mib(Mib&& o) = default;

  // Return a new Mib with the index element replaced with the given value.
  Mib operator()(size_t n) const {
    assert(m_indexer != -1);
    Mib mib{*this};
    mib.m_parts[m_indexer] = n;
    return mib;
  }

  static constexpr size_t MAX_MIB_SIZE = 4;
  std::array<size_t, MAX_MIB_SIZE> m_parts;
  size_t m_size;
  ptrdiff_t m_indexer;
  const char* m_name;

 private:
  void count_parts(const char* name) {
    m_size = 1;
    m_indexer = -1;
    for (char ch; (ch = *name++);) {
      if (ch == '.') {
        if (name[0] == '0' && (name[1] == '.' || name[1] == '\0')) {
          m_indexer = m_size;
        }
        ++m_size;
      }
    }
  }
};

void mallctl_by_mib(
    const Mib& mibx,
    void* oldp,
    size_t* oldlenp,
    const void* newp,
    size_t newlen) {
  int res = ::je_mallctlbymib(
      mibx.m_parts.data(), mibx.m_size, oldp, oldlenp, (void*)newp, newlen);
  if (res) {
    std::string nameStr(mibx.m_name);
    std::string s =
        "mallctlbymib(" + nameStr + ") failed, errno = " + std::to_string(res);
    throw std::runtime_error(s);
  }
}

template <typename T>
T mallctl_by_mib_R(const Mib& mib) {
  T old;
  size_t oldsz = sizeof(old);
  mallctl_by_mib(mib, &old, &oldsz, nullptr, 0);
  return old;
}

template <typename T>
void mallctl_by_mib_W(const Mib& mib, T& newp) {
  mallctl_by_mib(mib, nullptr, nullptr, &newp, sizeof(newp));
}

void mallctl_by_mib_V(const Mib& mib) {
  mallctl_by_mib(mib, nullptr, nullptr, nullptr, 0);
}

void tcache_delete(unsigned tc) {
  static const Mib mib{"tcache.destroy"};
  return mallctl_by_mib_W(mib, tc);
}

unsigned tcache_create() {
  static const Mib mib{"tcache.create"};
  return mallctl_by_mib_R<unsigned>(mib);
}

void arena_purge(unsigned arena) {
  static const Mib mib{"arena.0.purge"};
  mallctl_by_mib_V(mib(arena));
}

#if JEMALLOC_VERSION_MAJOR == 5
void arena_destroy(unsigned arena) {
  static const Mib mib{"arena.0.destroy"};
  mallctl_by_mib_V(mib(arena));
}
#endif

#if JEMALLOC_VERSION_MAJOR == 4
void arena_reset(unsigned arena) {
  static const Mib mib{"arena.0.reset"};
  mallctl_by_mib_V(mib(arena));
}
#endif
} // namespace je
} // anonymous namespace
} // namespace skip
