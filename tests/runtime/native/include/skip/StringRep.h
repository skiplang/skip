/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string.h>

#include "fwd.h"

#include "detail/extc.h"

// This is a skip::String passed as a POD type.
DEFINE_PTR_TYPE(SkipString, skip::String);

namespace skip {

// Single-word header for long strings.
struct LongStringMetadata final {
  static constexpr uint32_t kTagMask = 0x80000000;
  union {
    struct {
      arraysize_t m_byteSize;
      // NOTE: The hash MUST be or'd with kTagMask to indicate a long string.
      uint32_t m_hash;
    };
    uint64_t m_bits;
    int64_t m_sbits;
  };
};

static_assert(sizeof(LongStringMetadata) == sizeof(VTable*), "Invalid size");

// Short string POD type. Layout is documented in struct skip::String.
struct StringRep {
  static constexpr size_t DATA_BUFFER_SIZE = 8;

  struct alignas(DATA_BUFFER_SIZE) DataBuffer {
    char buf[DATA_BUFFER_SIZE];
  };

  static constexpr size_t kUnusedTagBits = 3;

  static constexpr size_t MAX_SHORT_LENGTH = 7;

  // Amount to signed-right-shift short strings to get (size - 8).
  static constexpr size_t SHORT_LENGTH_SHIFT = 7 * 8 + kUnusedTagBits;

  union {
#ifdef GEN_PREAMBLE
    // Make this the first field for easy conversions from SkipString.
    SkipString m_podString;
#endif
    const LongStringMetadata* m_longMetaEnd; // points to end of metadata
    const skip::LongString* m_longString;
    const char* m_longData;
    DataBuffer m_data;
    uint64_t m_bits;
    int64_t m_sbits;
  };

  // NOTE: These are static to keep StringRep a POD type.

  static SKIP_INLINE SkipBool repIsLong(StringRep s) {
    return s.m_sbits > 0;
  }

  static SKIP_INLINE const char* repData(StringRep s, DataBuffer& buffer) {
    // Write this is a branchless way using a cmov.
    buffer = s.m_data;
    return repIsLong(s) ? s.m_longData : buffer.buf;
  };

  // Compute the size of either a short or long string in a branch-free manner.
  //
  // This always does one load from what should be a "hot" address, where a
  // branchy version would only do a load for a long string.
  //
  // This compiles to five instructions vs. six for a branchy version.
  static SKIP_INLINE uint32_t repByteSize(StringRep s) {
    // Magic size value to counteract the size offset for short strings, below.
    static const LongStringMetadata offsetIfShort = {{{8, 0}}};

    // Use a conditional move to choose which pointer to dereference,
    // either a dummy 8 value or the actual size.
    const auto endIfShort = &offsetIfShort + 1;
    const auto endIfLong = s.m_longMetaEnd;
    const auto end = repIsLong(s) ? endIfLong : endIfShort;

    // If short, this is the size minus 8, due to sign-extended tag bits.
    // If long, it's zero, because we assume the high 5 bits of any pointer
    // are zero (currently true on x86_64 and aarch64, verified with
    // a static_assert in String.cpp).
    //
    // NOTE: technically the behavior of arithmetic right-shift of negative
    // numbers is implementation-defined, but we are comfortable assuming
    // implementations that do the obvious twos-complement sign extension
    // on right shifts.
    const auto sizeMinus8IfShortElseZero = s.m_sbits >> SHORT_LENGTH_SHIFT;

    // If short, we are adding (size - 8) to 8 to get the right answer.
    // If long, we are adding 0 to the LongMetadata's m_byteSize field.
    return (uint32_t)sizeMinus8IfShortElseZero + end[-1].m_byteSize;
  }
};
} // namespace skip
