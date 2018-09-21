/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "detail/extc.h"
#include "objects-extc.h"

extern "C" {

using SkipRefMaskType = size_t;

// The GC mask consist of interleaved SkipRefMaskTypes (currently only
// 1 - so not really interleaved).

enum ESkipGcConsts {
  // Number of stripes interleaved in the mask
  kSkipGcStripeCount = 2,

  // '1' bits in this element represent pointer slots which need to be checked
  // during gc.
  kSkipGcStripeIndex = 0,

  // '1' bits in this element represent pointer slots which need to be checked
  // during freeze.
  kSkipFreezeStripeIndex = 1,
};

enum ESkipGcKind {
  kSkipGcKindClass = 0,
  kSkipGcKindArray = 1,
  kSkipGcKindInvocation = 2,
  kSkipGcKindString = 3,
  kSkipGcKindCycleHandle = 4,
};

enum ESkipGcStateChangeType {
  kSkipGcStateChangeInitialize = 0,
  kSkipGcStateChangeFinalize = 1,
};

// Bits for m_refsHint
enum ESkipGcRefsHint {
  // The object contains a mix of references and non-references.  If this bit is
  // not set then m_refMask[] will be ignored.  NOTE: If this bit is clear then
  // kSkipGcRefsHintAllFrozenRefs MUST be set.
  kSkipGcRefsHintMixedRefs = 1,

  // The object contains only guaranteed frozen references.  If this bit is set
  // then freezing is a simple non-recursive copy.
  kSkipGcRefsHintAllFrozenRefs = 2,

  // This root object is guaranteed to not have any mutable aliased references
  // and thus when freezing sub nodes do not have to be remembered.
  kSkipGcRefsHintNoMutableAliases = 4,

  // If set then this class should not be inserted into the intern table when
  // "interned".
  kSkipGcRefsAvoidInternTable = 8,

};

struct SkipGcType {
  // This should be a mask of ESkipGcRefsHint
  uint8_t m_refsHintMask;

  // This should be one of ESkipGcKind.
  uint8_t m_kind;

  // For refs: Should be 1.
  // For arrays: Should be the number of times the refMask is tiled into the
  // m_refMask array.
  uint8_t m_tilesPerMask;

  // If true, a "char m_name[]" field immediately follows m_refMask.
  uint8_t m_hasName;

  // Number of bytes of uninterned metadata, i.e. between the start of raw
  // allocated storage and the RObj's pointed-to address.
  uint16_t m_uninternedMetadataByteSize;

  // Number of bytes of metadata for the interned copy of this object.
  uint16_t m_internedMetadataByteSize;

  // For refs: The size (in bytes) of an object of this size and
  //           must be a multiple of sizeof(void*).
  // For arrays: The size (in bytes) of a single slot. Can be zero.
  size_t m_userByteSize;

  // A callback which is called when an IObj of this type changes state.
  void (*m_onStateChange)(SkipIObj*, ESkipGcStateChangeType type);

  // This is a mask indicating which values contain pointers.
  // For refs: Starting with the LSB this array should contain a '1' for every
  // slot which contains a pointer. Unused bits should be '0'.
  // For arrays: This array should be the same as for refs but if it contains
  // kBitsPerRefMask/2 or less slots the mask should be repeated to fill out
  // the first entry.
  SkipRefMaskType m_refMask[0];

  // If m_hasName is true, a zero-terminated string appears here, after
  // the variable number of words in m_refMask.
  // char m_name[};
};

extern void SKIP_invocationOnStateChange(SkipIObj* obj, SkipInt changeType);
} // extern "C"
