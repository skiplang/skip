/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <cstdlib>
#include <cstddef>
#include <memory>

namespace skip {

namespace mem {

constexpr size_t kPageSize = 4096;

void decommit(void* ptr, size_t size);
void commit(void* ptr, size_t size);
void purge_lazy(void* ptr, size_t size);
void purge_forced(void* ptr, size_t size);

enum class Access { none, readonly, writeonly, readwrite };
void protect(void* ptr, size_t size, Access access);

void free(void*, size_t size);
void* alloc(size_t size, size_t alignment = 1);
void* reserve(size_t size, size_t alignment = 1);

/*
 * helpers for doing pointer math without ugly casting
 */

// Return a - b
inline ptrdiff_t sub(const void* a, const void* b) {
  return static_cast<const char*>(a) - static_cast<const char*>(b);
}

// Return a - b
inline const void* sub(const void* a, size_t b) {
  return static_cast<const char*>(a) - b;
}

// Return a - b
inline void* sub(void* a, size_t b) {
  return static_cast<char*>(a) - b;
}

// Return a + b
inline const void* add(const void* a, size_t b) {
  return static_cast<const char*>(a) + b;
}

// Return a + b
inline void* add(void* a, size_t b) {
  return static_cast<char*>(a) + b;
}

// Return true if p is within the range [begin, begin + size)
inline bool within(const void* p, const void* begin, const size_t size) {
  return static_cast<size_t>(sub(p, begin)) < size;
}

template <typename T>
constexpr bool isAligned(T p, size_t align) {
  return (reinterpret_cast<uintptr_t>(p) & (align - 1)) == 0;
}

template <typename T>
constexpr bool isPageAligned(T p) {
  return mem::isAligned(p, kPageSize);
}
} // namespace mem
} // namespace skip
