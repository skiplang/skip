/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "StringRep.h"

#ifndef GEN_PREAMBLE
#include "String.h"
#endif

#include <sys/types.h>

extern "C" {

extern uint32_t SKIP_String__unsafe_get(SkipString s, size_t x);
extern SkipString SKIP_String_concat(const SkipString* strings, size_t size);
extern SkipString SKIP_String_concat2(SkipString a, SkipString b);
extern SkipString SKIP_String_concat3(SkipString a, SkipString b, SkipString c);
extern SkipString
SKIP_String_concat4(SkipString a, SkipString b, SkipString c, SkipString d);

extern SkipString SKIP_Array_concatStringArray(const SkipRObj* array);

extern SkipBool SKIP_String_eq(SkipString a, SkipString b);
extern SkipBool SKIP_String__contains(SkipString a, SkipString b);
// SKIP_String_longStringEq() assumes that a and b are const LongString* and
// compares the data (so the assumption is that the hash and size have already
// been compared and are equal).
extern SkipBool
SKIP_String_longStringEq(const void* a, const void* b, size_t byteSize);
extern ssize_t SKIP_String_cmp(SkipString a, SkipString b);
extern SkipInt SKIP_String_hash_inl(SkipString str);

extern SkipString SKIP_String__fromChars(const void*, const SkipRObj* src);
extern SkipString SKIP_String__fromUtf8(const void*, const SkipRObj* src);

extern SkipFloat SKIP_String__toFloat_raw(SkipString s);

extern SkipString SKIP_String_StringIterator__substring(
    SkipRObj* i,
    SkipRObj* end);
extern SkipInt SKIP_String_StringIterator__rawCurrent(SkipRObj* i);
extern SkipInt SKIP_String_StringIterator__rawNext(SkipRObj* i);
extern SkipInt SKIP_String_StringIterator__rawPrev(SkipRObj* i);
extern void SKIP_String_StringIterator__rawDrop(SkipRObj* i, SkipInt n);
extern SkipInt SKIP_String__length(SkipString s);

struct toIntOptionHelperRet_t {
  /* FIXME: bool */ SkipInt valid;
  SkipInt value;
};
extern struct toIntOptionHelperRet_t SKIP_String_toIntOptionHelper(
    SkipString s);

extern SkipString
SKIP_String__sliceByteOffsets(SkipString str, SkipInt start, SkipInt end);
extern SkipInt SKIP_Unsafe_string_utf8_size(SkipString str);
extern uint8_t SKIP_Unsafe_string_utf8_get(SkipString s, SkipInt idx);

extern void SKIP_String_ConvertBuffer__setup(
    SkipRObj*,
    SkipString inputEncoding,
    SkipString outputEncoding,
    SkipBool throwOnBadCharacter);
extern void SKIP_String_ConvertBuffer__teardown(SkipRObj*);
extern void SKIP_String_ConvertBuffer__convert(SkipRObj*);
extern void SKIP_String_ConvertBuffer__flush(SkipRObj*);
} // extern "C"

SKIP_INLINE skip::StringRep stringRepFromString(SkipString ss) {
#ifdef GEN_PREAMBLE
  skip::StringRep s{{ss}};
#else
  skip::StringRep s = ss;
#endif
  return s;
}

inline SkipBool SKIP_String__contains(SkipString sa, SkipString sb) {
  skip::StringRep::DataBuffer bBuf;
  auto b = stringRepFromString(sb);
  const auto bData = skip::StringRep::repData(b, bBuf);
  const auto bSize = skip::StringRep::repByteSize(b);
#ifndef __linux__
  // On Mac, unlike Linux, memmem() returns nullptr for empty second argument.
  if (bSize == 0) {
    return true;
  }
#endif

  skip::StringRep::DataBuffer aBuf;
  auto a = stringRepFromString(sa);
  const auto aData = skip::StringRep::repData(a, aBuf);
  const auto aSize = skip::StringRep::repByteSize(a);

  return memmem(aData, aSize, bData, bSize) != nullptr;
}

SKIP_INLINE SkipInt SKIP_Unsafe_string_utf8_size(SkipString s) {
  return skip::StringRep::repByteSize(stringRepFromString(s));
}

// keep this in sync with peepholeStringHash() in src/native/peephole.sk
SKIP_INLINE SkipInt SKIP_String_hash_inl(SkipString ss) {
  auto s = stringRepFromString(ss);

  // choose a pointer with cmov, then unconditionally load the hash.
  // This anticipates the passed-in string already being in memory.
  return (
      skip::StringRep::repIsLong(s) ? &s.m_longMetaEnd->m_sbits
                                    : &s.m_sbits + 1)[-1];

  // The more obvious implementation compiles to if/then/else:
  // return str.m_sbits < 0 ? str.m_sbits : str.m_long[-1].m_sbits;
}

SKIP_INLINE uint8_t SKIP_Unsafe_string_utf8_get(SkipString ss, SkipInt idx) {
  auto s = stringRepFromString(ss);
  skip::StringRep::DataBuffer buf;
  return skip::StringRep::repData(s, buf)[idx];
}
