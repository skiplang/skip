/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#if __SIZEOF_POINTER__ == 4
#define SKIP_32BIT
#elif __SIZEOF_POINTER__ == 8
#define SKIP_64BIT
#else
#error "Unknown bit size."
#endif

namespace skip {

enum { kCacheLineSize = 64 };
}

#define ATTR_UNUSED __attribute__((__unused__))
#ifdef __clang__
#define FIELD_UNUSED ATTR_UNUSED
#else
#define FIELD_UNUSED
#endif

#ifdef NDEBUG
#define DEBUG_ONLY ATTR_UNUSED
#else
#define DEBUG_ONLY /* nop */
#endif

#ifdef __GNUC__
#define SKIP_GCC_VERSION \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#ifdef __GNUC__
#if SKIP_GCC_VERSION >= 40900
#define _MALLOC_ALIGN_ATTR(SIZE_IDX, ALIGN_IDX) \
  __attribute__(                                \
      (malloc, alloc_align(ALIGN_IDX), alloc_size(SIZE_IDX), returns_nonnull))
#elif SKIP_GCC_VERSION >= 40800
#define _MALLOC_ALIGN_ATTR(SIZE_IDX, ALIGN_IDX) \
  __attribute__((malloc, alloc_size(SIZE_IDX)))
#elif defined(__clang__)
#define _MALLOC_ALIGN_ATTR(SIZE_IDX, ALIGN_IDX) \
  __attribute__((malloc, returns_nonnull))
#else
#define _MALLOC_ALIGN_ATTR(SIZE_IDX, ALIGN_IDX) __attribute__((malloc))
#endif
#else
#define _MALLOC_ALIGN_ATTR(SIZE_IDX, ALIGN_IDX)
#endif

#ifdef __GNUC__
#if SKIP_GCC_VERSION >= 40900
#define _MALLOC_ATTR(SIZE_IDX) \
  __attribute__((malloc, alloc_size(SIZE_IDX), returns_nonnull))
#elif SKIP_GCC_VERSION >= 40800
#define _MALLOC_ATTR(SIZE_IDX) __attribute__((malloc, alloc_size(SIZE_IDX)))
#elif defined(__clang__)
#define _MALLOC_ATTR(SIZE_IDX) __attribute__((malloc, returns_nonnull))
#else
#define _MALLOC_ATTR(SIZE_IDX) __attribute__((malloc))
#endif
#else
#define _MALLOC_ATTR(SIZE_IDX)
#endif

#ifdef __GNUC__
#define DLL_EXPORT __attribute__((visibility("default")))
#endif
