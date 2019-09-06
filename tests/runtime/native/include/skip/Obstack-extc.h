/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Type-extc.h"
#include "config.h"
#include "detail/extc.h"
#include "objects-extc.h"

#include <cstddef>
#include <cstdint>

// SkipObstackPos encapsulates a raw pointer into a Chunk, used as a note.
struct SkipObstackPos {
  void* ptr;
};

extern "C" {

extern SkipObstackPos SKIP_Obstack_note_inl(void);

// manual collect
extern void SKIP_Obstack_collect0(SkipObstackPos note);
extern SkipRObjOrFakePtr SKIP_Obstack_collect1(
    SkipObstackPos note,
    SkipRObjOrFakePtr root);
extern void SKIP_Obstack_collect(
    SkipObstackPos note,
    SkipRObjOrFakePtr* roots,
    size_t rootSize);

// auto collect
extern void SKIP_Obstack_auto_collect0(SkipObstackPos note);
extern SkipRObjOrFakePtr SKIP_Obstack_auto_collect1(
    SkipObstackPos note,
    SkipRObjOrFakePtr root);
extern void SKIP_Obstack_auto_collect(
    SkipObstackPos note,
    SkipRObjOrFakePtr* roots,
    size_t rootSize);

// inlined collect
extern void SKIP_Obstack_inl_collect0(SkipObstackPos note);
extern SkipRObjOrFakePtr SKIP_Obstack_inl_collect1(
    SkipObstackPos note,
    SkipRObjOrFakePtr root);
extern void SKIP_Obstack_inl_collect(
    SkipObstackPos note,
    SkipRObjOrFakePtr* roots,
    size_t rootSize);

extern void* SKIP_Obstack_alloc(size_t sz);
extern void* SKIP_Obstack_calloc(size_t sz);

extern void* SKIP_Obstack_alloc_pinned(size_t sz);
extern void* SKIP_Obstack_calloc_pinned(size_t sz);

extern SkipRObj* SKIP_Obstack_shallowClone(const SkipRObj* obj);
extern SkipRObjOrFakePtr SKIP_Obstack_freeze(SkipRObjOrFakePtr obj);

// Return Obstack bytes used since the given note
extern SkipInt SKIP_Obstack_usage(SkipObstackPos note);

// write barriers
extern void SKIP_Obstack_verifyStore(void* field);
extern void SKIP_Obstack_store(void** addr, void* ptr);
extern void SKIP_Obstack_vectorUnsafeSet(void** addr, void* ptr);
} // extern "C"

// Base class containing Obstack state exposed to inline extc functions.
namespace skip {
struct ObstackDetail;

struct ObstackBase {
  // We always allocate Chunks of this size.
  static constexpr size_t kChunkSizeLog2 = 14; // 16KiB
  static constexpr size_t kChunkSize = (size_t)1 << kChunkSizeLog2;
  static constexpr size_t kAllocAlign = 8; // enough for double and 64 bit ptrs

  // The allocation cursor. Points into the current chunk, avoiding the need
  // to also store the current Chunk.
  void* m_nextAlloc;

  ObstackDetail* m_detail;

  // Return true if m_nextAlloc and note are on different chunks
  SKIP_INLINE SkipBool isNoteChunkFull(SkipObstackPos note) const {
    return (uintptr_t(note.ptr) ^ uintptr_t(m_nextAlloc)) >= kChunkSize;
  }
};

SKIP_EXPORT(_ZN4skip10tl_obstackE)
extern __thread ObstackBase tl_obstack;
} // namespace skip

// inline functions go here

#if !OBSTACK_VERIFY_NOTE
SKIP_INLINE SkipObstackPos SKIP_Obstack_note_inl() {
  using namespace skip;
  return {tl_obstack.m_nextAlloc};
}
#endif

SKIP_INLINE void SKIP_Obstack_inl_collect0(SkipObstackPos note) {
  using namespace skip;
  if (tl_obstack.isNoteChunkFull(note)) {
    SKIP_Obstack_auto_collect0(note);
  }
}

SKIP_INLINE SkipRObjOrFakePtr
SKIP_Obstack_inl_collect1(SkipObstackPos note, SkipRObjOrFakePtr root) {
  using namespace skip;
  return !tl_obstack.isNoteChunkFull(note)
      ? root
      : SKIP_Obstack_auto_collect1(note, root);
}

SKIP_INLINE void SKIP_Obstack_inl_collect(
    SkipObstackPos note,
    SkipRObjOrFakePtr* roots,
    size_t rootSize) {
  using namespace skip;
  if (tl_obstack.isNoteChunkFull(note)) {
    SKIP_Obstack_auto_collect(note, roots, rootSize);
  }
}

// field is the (interior) address of a pointer field in some object
SKIP_INLINE void SKIP_Obstack_store(void** field, void* ptr) {
  *field = ptr;
}

// field is the (interior) address of a pointer field in some vector element
SKIP_INLINE void SKIP_Obstack_vectorUnsafeSet(void** field, void* ptr) {
  *field = ptr;
}
