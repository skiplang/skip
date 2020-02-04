/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "util.h"

#include <atomic>
#include <deque>
#include <memory>
#include <cstdlib>

namespace skip {

struct RObj;
struct LargeObjHeader;

// An Arena is a memory allocator which can quickly report if a pointer was
// within a range allocated by it.
struct Arena final {
  static void init();

  ~Arena() = delete;
  Arena() = delete;

  enum class Kind {
    unknown,
    iobj,
    large,
    obstack,
  };

  // Allocate sz bytes of memory.
  static void* alloc(size_t sz, Kind kind) _MALLOC_ATTR(1) {
    // Passing a '1' indicates to use the standard malloc alignment behavior.
    return allocAligned(sz, 1, kind);
  }

  // Allocate size bytes of memory.  The returned address is guaranteed to be a
  // multiple of align.
  static void* allocAligned(size_t sz, size_t align, Kind kind)
      _MALLOC_ALIGN_ATTR(1, 2);

  // Allocate and zero sz bytes of memory.
  static void* calloc(size_t sz, Kind kind) _MALLOC_ATTR(1);

  // Free memory previously allocated by alloc() or calloc().
  static void free(void* p);
  static void free(void* p, Kind kind);

  // Return the Kind of memory the given pointer was allocated as.
  // Passing a pointer that is not managed by an Arena will return
  // Kind::unknown.
  static Kind rawMemoryKind(const void* p);
  static Kind getMemoryKind(const LargeObjHeader* p);
  static Kind getMemoryKind(const RObj* p);
  static void setMemoryKind(const void* start, const void* end, Kind kind);

  // Map pointers to Kind
  struct KindMapper : private skip::noncopyable {
    // This is the smallest block that the KindMapper can relate to.
    static constexpr size_t kKmSlabSizeLog2 = 21; // 2MiB
    static constexpr size_t kKmSlabSize = (size_t)1 << kKmSlabSizeLog2;

    static KindMapper& singleton();
    Kind get(const void* p) const;
    void erase(const void* start, const void* end);
    void set(const void* start, const void* end, Kind kind);

   private:
    std::atomic<size_t>* m_data;
    ~KindMapper();
    KindMapper();

    static_assert(
        sizeof(std::atomic<size_t>) == sizeof(size_t),
        "Invalid atomic");

    static constexpr size_t dataSize();

    // Return the index and offset for the given pointer
    static std::pair<size_t, size_t> split(const void* p);
  };
};
} // namespace skip
