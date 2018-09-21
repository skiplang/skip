/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef GEN_PREAMBLE
// we compile the preamble with very little config, so including
// this stuff will break who-knows-what.

// For FOLLY_SANITIZE_*
#include <folly/CPortability.h>

#if FOLLY_SANITIZE || FOLLY_SANITIZE_ADDRESS || FOLLY_SANITIZE_THREAD
#define SKIP_SANITIZE 1
#endif

#if !SKIP_SANITIZE
// For JEMALLOC_VERSION_*
#include <algorithm>
#include <jemalloc/jemalloc.h>
#endif

#endif // GEN_PREAMBLE

#ifndef SKIP_PARANOID
#ifndef NDEBUG
#define SKIP_PARANOID 2
#else
#define SKIP_PARANOID 0
#endif
#endif

// When OBSTACK_VERIFY_NOTE is true we assert that all collect() positions came
// from actual note() calls.  This changes the data layout slightly (all note()
// calls increment the heap by sizeof(uintptr_t)).
#ifndef OBSTACK_VERIFY_NOTE
#define OBSTACK_VERIFY_NOTE SKIP_PARANOID
#endif

// If this is '1' then it forces all IObj nodes to be treated as if they contain
// a cycle.  This can be useful to flush out places where we should be checking
// for a refcountDelegate() and aren't.
#define FORCE_IOBJ_DELEGATE 0

// Do extra lock consistency checking, at some performance cost.
#ifndef SKIP_DEBUG_LOCKS
#define SKIP_DEBUG_LOCKS SKIP_PARANOID
#endif

// Under normal circumstances we reserve a block of memory for the Arena and use
// jemalloc to do memory management within that block.  Unfortunately this won't
// work under ASAN or TSAN (jemalloc crashes).  So under [AT]SAN we have our own
// junky allocator which just does the bare minimum to support arenas.
//
// This mode can also be forced by defining USE_JEMALLOC to 0.
#if !defined(USE_JEMALLOC)
#define USE_JEMALLOC !SKIP_SANITIZE

#if USE_JEMALLOC && (JEMALLOC_VERSION_MAJOR == 5) && \
    (JEMALLOC_VERSION_MINOR == 0) && (JEMALLOC_VERSION_BUGFIX == 0)
// JeMalloc 5.0.0 is pretty broken.
// https://github.com/jemalloc/jemalloc/issues/923
// Also this error - https://fburl.com/chu3xszk
#undef USE_JEMALLOC
#define USE_JEMALLOC 0
#endif
#endif

// When OBSTACK_VERIFY_PARANOID is true then we check the obstack sanity after
// every collect and every freeze.  The cost of this is relative to the size of
// allocated memory (i.e. expensive).
#ifndef OBSTACK_VERIFY_PARANOID
#define OBSTACK_VERIFY_PARANOID 0
#endif

// We do some stuff which doesn't play nice with valgrind.  Turning this on
// makes us behave better (but is less optimal for non-valgrind).
#ifndef ENABLE_VALGRIND
#define ENABLE_VALGRIND 0
#endif
