/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/memory.h"
#include "skip/util.h"

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>

#if USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#if ENABLE_VALGRIND
#include <valgrind/memcheck.h>
#endif

#if !defined(__APPLE__)
// On Darwin we can't use shm_open() for a file mapping.
#define USE_SHM_MAPPING
#endif

namespace skip {

namespace {

template <typename T>
constexpr bool isPageAligned(T p) {
  return (reinterpret_cast<uintptr_t>(p) & (mem::kPageSize - 1)) == 0;
}
} // namespace

void mem::decommit(void* ptr, size_t size) {
  assert(isPageAligned(ptr));
  assert(isPageAligned(size));

  int res = ::mprotect(ptr, size, PROT_NONE);
  if (res)
    errnoFatal("mprotect() failed");

  res = ::madvise(ptr, size, MADV_DONTNEED);
  if (res)
    errnoFatal("madvise() failed");
}

void mem::commit(void* ptr, size_t size) {
  assert(isPageAligned(ptr));
  assert(isPageAligned(size));

  int res = ::mprotect(ptr, size, PROT_READ | PROT_WRITE);
  if (res)
    errnoFatal("mprotect() failed");

  // Not sure if this is necessary or not.
  res = ::madvise(ptr, size, MADV_NORMAL);
  if (res)
    errnoFatal("madvise() failed");
}

void mem::purge_lazy(void* ptr, size_t size) {
  assert(isPageAligned(ptr));
  assert(isPageAligned(size));

  // Should be MADV_FREE - but that's not fully supported yet.
  int res = ::madvise(ptr, size, MADV_DONTNEED);
  if (res)
    errnoFatal("madvise() failed");
}

void mem::purge_forced(void* ptr, size_t size) {
  assert(isPageAligned(ptr));
  assert(isPageAligned(size));

  int res = ::madvise(ptr, size, MADV_DONTNEED);
  if (res)
    errnoFatal("madvise() failed");
}

void mem::free(void* p, size_t size) {
  assert(isPageAligned(p));
  assert(isPageAligned(size));
  int res = ::munmap(p, size);
  if (res)
    errnoFatal("munmap() failed");
}

void mem::protect(void* p, size_t size, Access access) {
  assert(isPageAligned(p));
  assert(isPageAligned(size));
  int prot = 0;
  switch (access) {
    case Access::none:
      prot = 0;
      break;
    case Access::readonly:
      prot = PROT_READ;
      break;
    case Access::writeonly:
      prot = PROT_WRITE;
      break;
    case Access::readwrite:
      prot = PROT_READ | PROT_WRITE;
      break;
  }
  int res = ::mprotect(p, size, prot);
  if (res)
    errnoFatal("mprotect() failed");
}

namespace {

void* allocHelper(size_t size, size_t alignment, bool commit) {
  if (alignment < mem::kPageSize)
    alignment = mem::kPageSize;

  assert(isPageAligned(size));

  size_t padding = alignment - mem::kPageSize;

  void* p = ::mmap(
      nullptr,
      size + padding,
      commit ? PROT_READ | PROT_WRITE : PROT_NONE,
      MAP_PRIVATE | MAP_ANONYMOUS,
      -1,
      0);
  // In the future we might want to actually try to handle this better
  // (by freeing up caches, etc) but right now just fail.
  if (p == MAP_FAILED)
    errnoFatal("mmap() failed");

  if (auto underflow = (-reinterpret_cast<size_t>(p)) & (alignment - 1)) {
    // release the unaligned stuff from the front
    int res = ::munmap(p, underflow);
    if (res != 0)
      errnoFatal("munmap() failed");
    p = mem::add(p, underflow);
    padding -= underflow;
  }

  if (padding > 0) {
    // release the unused stuff from the back
    int res = ::munmap(mem::add(p, size), padding);
    if (res != 0)
      errnoFatal("munmap() failed");
  }

  return p;
}
} // anonymous namespace

void* mem::alloc(size_t size, size_t alignment) {
  return allocHelper(size, alignment, true);
}

void* mem::reserve(size_t size, size_t alignment) {
  return allocHelper(size, alignment, false);
}
} // namespace skip
