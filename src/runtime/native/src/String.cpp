/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define __GNU_SOURCE
#include <string.h>

#include "skip/String.h"
#include "skip/String-extc.h"
#include "skip/external.h"
#include "skip/intern.h"
#include "skip/Obstack.h"
#include "skip/objects.h"
#include "skip/SmallTaggedPtr.h"

#include <cassert>
#ifdef __x86_64__
#include <nmmintrin.h>
#endif

#include <iterator>

#include <unicode/utypes.h>
#include <unicode/stringpiece.h>
#include <unicode/utf8.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>
#include <unicode/ustring.h>

#include <boost/format.hpp>

namespace skip {

static_assert(
    StringRep::SHORT_LENGTH_SHIFT >= detail::kMaxPtrBits,
    "repByteSize implementation will not work for this pointer size");

constexpr uint64_t EMPTY_STRING_BITS = 0xC000000000000000ULL;

String::String() {
  // An "empty" short string.
  m_bits = EMPTY_STRING_BITS;
}

namespace {

using StringPaddingUnit = uintptr_t;
constexpr size_t StringPaddedAlign = sizeof(StringPaddingUnit);

LongString* allocLongString(size_t byteSize) {
  const size_t metadataSize = sizeof(LongStringMetadata);
  size_t paddedSize = String::computePaddedSize(byteSize);

  const auto mem =
      static_cast<char*>(Obstack::cur().alloc(metadataSize + paddedSize));

  // Since we know we always round up to a word we can just force the entire
  // last pad word to 0 and it will NUL out our pad bytes.
  reinterpret_cast<StringPaddingUnit*>(mem + metadataSize + paddedSize)[-1] = 0;

  auto longString = reinterpret_cast<LongString*>(mem + metadataSize);
  longString->metadata().m_byteSize = byteSize;

  return longString;
}
} // namespace

String::String(const char* begin, const char* end) {
  const size_t byteSize = end - begin;
  if (byteSize <= MAX_SHORT_LENGTH) {
    // Short string
    // Zero out the pad data before setting it.  We could just zero out the
    // actual pad bytes but that probably wouldn't save anything.
    auto data = m_data.buf;
    memset(data, 0, 8);
    memcpy(data, begin, byteSize);
    data[7] = 0xC0 | (byteSize << kUnusedTagBits);
  } else {
    // Long string
    auto longString = allocLongString(byteSize);
    m_longString = longString;

    // TODO: compute this hash while we're copying the data into the string.
    memcpy(const_cast<char*>(longString->m_data), begin, end - begin);
    longString->metadata().m_hash =
        computeStringHash(longString->m_data, byteSize);
  }
}

bool String::operator==(const String& o) const {
  // If all the bits are the same they MUST be equal.
  auto word1 = sbits();
  auto word2 = o.sbits();
  if (word1 == word2)
    return true;
  // If either of them is short then they must not match.
  if ((word1 | word2) < 0)
    return false;
  // They must both be long - check the length and hash.
  if (m_longString->metadata().m_bits != o.m_longString->metadata().m_bits) {
    return false;
  }
  // Check the data
  return equalBytesExpectingYes(
      m_longString->m_data, o.m_longString->m_data, m_longString->paddedSize());
}

ssize_t String::cmp(const String& o) const {
  const size_t len1 = byteSize();
  const size_t len2 = o.byteSize();
  DataBuffer data1;
  DataBuffer data2;
  int res = memcmp(data(data1), o.data(data2), std::min(len1, len2));
  if (res == 0)
    res = static_cast<ssize_t>(len1) - static_cast<ssize_t>(len2);
  return res;
}

const char* String::c_str(char buffer[CSTR_BUFFER_SIZE]) const {

  const auto bytes = byteSize();
  if (isShortString()) {
    // For short strings always copy - we copy an extra byte but that way the
    // copy optimizes to a single word copy.
    memcpy(buffer, m_data.buf, sizeof(m_data.buf));
    buffer[7] = '\0';
    return buffer;
  } else {
    // If we're not exactly a multiple of StringPaddedAlign bytes or we're
    // larger than CSTR_BUFFER_SIZE then we'll have padding NULs.
    if (((bytes & (StringPaddedAlign - 1)) == 0) &&
        (bytes < CSTR_BUFFER_SIZE)) {
      assert(bytes == computePaddedSize(bytes));
      memcpy(buffer, m_longString->m_data, bytes);
      buffer[bytes] = '\0';
      return buffer;
    } else {
      assert(m_longString->m_data[bytes] == '\0');
      return m_longString->m_data;
    }
  }
}

std::string String::toCppString() const {
  auto text = unsafeData();
  return std::string(text, text + byteSize());
}

skip::StringPiece String::slice(DataBuffer& buffer) const {
  auto p = data(buffer);
  return StringPiece(p, byteSize());
}

void String::clear() {
  m_bits = EMPTY_STRING_BITS;
}

const char* String::data(DataBuffer& buffer) const {
  return repData(*this, buffer);
}

size_t String::byteSize() const {
  return repByteSize(*this);
}

size_t String::hash() const {
  return SKIP_String_hash_inl(*this);
}

size_t String::countCharacters() const {
  const auto text = unsafeData();
  const auto bs = byteSize();
  size_t total = 0;
  for (size_t offset = 0; offset < bs;) {
    ++total;
    U8_FWD_1(text, offset, bs);
  }
  return total;
}

Type& LongString::static_type() {
  static auto singleton = Type::factory(
      "String",
      Type::Kind::string,
      0,
      {},
      nullptr,
      sizeof(LongStringMetadata),
      sizeof(IObjMetadata));
  return *singleton;
}

void LongString::verifyInvariants() const {
  // Check hash
  size_t byteSize = metadata().m_byteSize;
  size_t paddedSize = String::computePaddedSize(byteSize);
  assert(metadata().m_hash == String::computeStringHash(m_data, byteSize));

  // Check pad
  for (size_t i = byteSize; i < paddedSize; ++i) {
    assert(m_data[i] == '\0');
  }
}

arraysize_t LongString::paddedSize() const {
  return String::computePaddedSize(byteSize());
}

String String::concat2(String a, String b) {
  // Sick hack - the high 5 bits of a String are (length - 8).  So adding two
  // string sbits will give us (a.length + b.length - 16).  Note that if we ever
  // use the lower bits of the high byte we'll need some masking.
  int64_t hack = a.sbits() + b.sbits();
  if (hack < -(8LL << SHORT_LENGTH_SHIFT)) {
    // The result is going to fit in a short string.
    auto abits = a.bits();
    auto bbits = b.bits();
    hack =
        ((hack & 0xFF00000000000000ULL) | (abits & 0xC0FFFFFFFFFFFFFFULL) |
         (bbits
          << ((abits >> (SHORT_LENGTH_SHIFT - 3)) & (MAX_SHORT_LENGTH << 3))));
    return String(hack);

  } else {
    // Guaranteed to need a long string.
    size_t alen = a.byteSize();
    size_t blen = b.byteSize();

    auto longString = allocLongString(alen + blen);

    // TODO: compute this hash while we're copying the data into the string.
    memcpy(const_cast<char*>(longString->m_data), a.unsafeData(), alen);
    memcpy(const_cast<char*>(longString->m_data) + alen, b.unsafeData(), blen);

    longString->metadata().m_hash =
        computeStringHash(longString->m_data, alen + blen);

    return String(*longString);
  }
}

String String::concat(const String* strings, size_t size) {
  if (size == 0)
    return String();

  int64_t res = strings->sbits();
  if (res < 0) {
    // The first string is short.  Keep pulling in short strings until we run
    // out of space.
    for (++strings; /* no cond */; ++strings) {
      if (--size == 0)
        return String(res);

      int64_t hack = res + strings->sbits();
      if (hack >= -(8LL << SHORT_LENGTH_SHIFT)) {
        // The result no longer fits in a short string
        break;
      }

      // This uses the same hack as described above in concat2().
      res =
          ((hack & 0xFF00000000000000ULL) | (res & 0xC0FFFFFFFFFFFFFFULL) |
           (strings->bits()
            << ((res >> (SHORT_LENGTH_SHIFT - 3)) & (MAX_SHORT_LENGTH << 3))));
    }
  } else {
    // The first string is long.  Start with an empty string.
    res = EMPTY_STRING_BITS;
  }

  // Need a long string.
  size_t used = (res >> SHORT_LENGTH_SHIFT) + 8;
  size_t total = used;
  for (size_t i = 0; i < size; ++i) {
    total += strings[i].byteSize();
  }

  LongString* longString = allocLongString(total);
  // TODO: compute this hash while we're copying the data into the string.
  auto buf = const_cast<char*>(longString->m_data);

  reinterpret_cast<int64_t*>(buf)[0] = res;
  for (; size-- > 0; ++strings) {
    size_t len = strings->byteSize();
    memcpy(buf + used, strings->unsafeData(), len);
    used += len;
  }

  longString->metadata().m_hash = computeStringHash(longString->m_data, total);

  return String(*longString);
}

#ifndef __x86_64__
namespace {

uint64_t crc32_u64(uint64_t a, uint64_t b) {
  constexpr uint32_t POLY = 0x82f63b78;
  for (int j = 0; j < 64; ++j, b >>= 1) {
    a = (a >> 1) ^ (((a ^ b) & 1) * POLY);
  }
  return a;
}
} // namespace

#define _mm_crc32_u64 crc32_u64
#endif

// computeStringHash() matches HHVM's string CRC including making the hash ascii
// case-insensitive (and worse because it's masking off 'interesting' utf8
// bits).
#ifdef __x86_64__
__attribute__((__target__("sse4.2")))
#endif
uint32_t
String::computeStringHash(const void* ptr, arraysize_t byteSize) {
  uint64_t sum = static_cast<uint32_t>(-1);
  if (byteSize > 0) {
    constexpr uint64_t CASE_MASK = 0xdfdfdfdfdfdfdfdfULL;
    auto data = static_cast<const uint64_t*>(ptr);
    for (; byteSize > 8; byteSize -= 8) {
      sum = _mm_crc32_u64(sum, *data++ & CASE_MASK);
    }

    // Handle the final bytes of the string.  Note that because this is shifting
    // the bytes it makes strings hash as if they have embedded NULs.
    // i.e. "Hello, World" hashes as "Hello, W\0\0\0\0orld" and not
    // "Hello, World\0\0\0\0".
    sum = _mm_crc32_u64(sum, (*data & CASE_MASK) << (64 - 8 * byteSize));
  }
  return (uint32_t)(sum >> 1) | LongStringMetadata::kTagMask;
}

size_t String::computePaddedSize(size_t byteCount) {
  size_t n = byteCount;

  // For large strings ensure that the string always ends with a NUL
  if (n >= String::CSTR_BUFFER_SIZE)
    ++n;

  // We always pad out to a multiple of StringPaddedAlign bytes since the memory
  // allocator will do that anyway. This way string code can safely process
  // StringPaddedAlign bytes at a time.
  n = roundUp(n, StringPaddedAlign);

  return n;
}

String String::sliceByteOffsets(int64_t start, int64_t end) {
  if (start > end || start < 0 || end < 0 || end > byteSize()) {
    SKIP_throwInvariantViolation(String("Invalid bounds"));
  }

  auto basePtr = unsafeData();
  return String(basePtr + start, basePtr + end);
}

// -----------------------------------------------------------------------------

StringPtr::~StringPtr() {
  decref();
}

StringPtr::StringPtr(const StringPtr& o) : m_string(*o) {
  incref();
}

StringPtr& StringPtr::operator=(const StringPtr& o) {
  if (this != &o) {
    StringPtr discardOld(std::move(*this));
    m_string = *o;
    incref();
  }
  return *this;
}

StringPtr::StringPtr(StringPtr&& o) noexcept : m_string(o.release()) {}

StringPtr& StringPtr::operator=(StringPtr&& o) noexcept {
  std::swap(m_string, o.m_string);
  return *this;
}

StringPtr::StringPtr(const char* begin, const char* end) {
  size_t byteSize = end - begin;
  if (byteSize <= String::MAX_SHORT_LENGTH) {
    m_string = String(begin, end);
  } else {
    Obstack::PosScope p;
    *this = intern(String(begin, end));
  }
}

StringPtr::StringPtr(String s, bool incref) : m_string(s) {
  assert(!s.asLongString() || s.asLongString()->isInterned());
  if (incref)
    this->incref();
}

StringPtr::StringPtr(const LongString& s, bool incref) : m_string(s) {
  assert(s.isInterned());
  if (incref)
    this->incref();
}

String StringPtr::release() noexcept {
  String res = m_string;
  m_string.clear();
  return res;
}

void StringPtr::reset() {
  *this = StringPtr();
}

void StringPtr::incref() {
  if (auto longString = m_string.asLongString()) {
    skip::incref(&longString->cast<IObj>());
  }
}

void StringPtr::decref() {
  if (auto longString = m_string.asLongString()) {
    skip::decref(&longString->cast<IObj>());
  }
}

struct StringIterator final : RObj {
  String substring(StringIterator& end) const {
    if (m_string.sbits() != end.m_string.sbits()) {
      SKIP_throwInvariantViolation(
          String("Called StringIterator::substring on a different string"));
    }
    if (m_byteOffset > end.m_byteOffset) {
      SKIP_throwInvariantViolation(
          String("Called StringIterator::substring with end before start"));
    }

    // Must index both offsets off of the same pointer as
    // this.m_string.unsafeData() != end.m_string.unsafeData() for inlined
    // strings.
    auto basePtr = m_string.unsafeData();
    return String(basePtr + m_byteOffset, basePtr + end.m_byteOffset);
  }

  int64_t rawCurrent() const {
    int64_t offset = m_byteOffset;
    return rawCurrentImpl(offset);
  }

  int64_t rawNext() {
    return rawCurrentImpl(m_byteOffset);
  }

  int64_t rawPrev() {
    if (m_byteOffset == 0) {
      return -1;
    }
    UChar32 ch = 0;
    int32_t offset = m_byteOffset;
    U8_PREV(m_string.unsafeData(), 0, offset, ch);
    m_byteOffset = offset;
    return ch;
  }

  void rawDrop(int64_t n) {
    if (n < 0) {
      SKIP_throwInvariantViolation(
          String("Called StringIterator::drop with a negative number"));
    }
    const auto byteSize = m_string.byteSize();
    U8_FWD_N(m_string.unsafeData(), m_byteOffset, byteSize, n);
  }

 private:
  // WARNING: These fields MUST be kept in sync with StringIterator in
  // prelude/native/StringIterator.sk.

  // Keep a copy of the string.  This is necessary to make sure that the base
  // string isn't collected out from under us.  You might be tempted to save a
  // pointer to the inner data of the string - but don't do that because the GC
  // might move the string (or 'this') and wouldn't update those inner pointers
  // properly.
  String m_string;
  int64_t m_byteOffset;

  StringIterator() = delete;

  const char* getChars() const {
    return m_string.unsafeData() + m_byteOffset;
  }

  int64_t rawCurrentImpl(int64_t& offset) const {
    const auto byteSize = m_string.byteSize();

    if (offset >= byteSize) {
      return -1;
    }

    UChar32 ch = 0;
    int32_t endOffset = offset;
    U8_NEXT(m_string.unsafeData(), endOffset, byteSize, ch);
    offset = endOffset;
    return ch;
  }
};

AObj<String>* createStringVector(size_t sz) {
  return reinterpret_cast<AObj<String>*>(SKIP_createStringVector(sz));
}

// -----------------------------------------------------------------------------
} // namespace skip

using namespace skip;

// --------------------------------------------------------------------------
// String

String SKIP_String_concat2(String a, String b) {
  return String::concat2(a, b);
}

String SKIP_String_concat3(String a, String b, String c) {
  std::array<String, 3> buf = {{a, b, c}};
  return SKIP_String_concat(buf.data(), buf.size());
}

String SKIP_String_concat4(String a, String b, String c, String d) {
  std::array<String, 4> buf = {{a, b, c, d}};
  return SKIP_String_concat(buf.data(), buf.size());
}

String SKIP_String_concat(const String* strings, size_t size) {
  return String::concat(reinterpret_cast<const String*>(strings), size);
}

String SKIP_Array_concatStringArray(const RObj* array) {
  auto& vec = *reinterpret_cast<const AObj<String>*>(array);
  return String::concat(&vec.unsafe_at(0), vec.arraySize());
}

uint32_t SKIP_String__unsafe_get(String str, size_t x) {
  String::DataBuffer buf;
  const auto data = str.data(buf);
  size_t start = 0;
  U8_FWD_N_UNSAFE(data, start, x);
  uint32_t c;
  U8_GET_UNSAFE(data, start, c);
  return c;
}

bool SKIP_String_eq(String a, String b) {
  return String(a) == String(b);
}

bool SKIP_String_longStringEq(const void* a, const void* b, size_t byteSize) {
  size_t paddedSize = String::computePaddedSize(byteSize);
  return equalBytesExpectingYes(a, b, paddedSize);
}

ssize_t SKIP_String_cmp(String a, String b) {
  return String(a).cmp(b);
}

SkipInt SKIP_String__length(String raw) {
  return raw.countCharacters();
}

String SKIP_String__fromChars(const void*, const RObj* _src) {
  const auto& src = *reinterpret_cast<const AObj<uint32_t>*>(_src);

  // Precompute the utf8 length
  size_t u8length = 0;
  for (auto c : src) {
    u8length += U8_LENGTH(c);
  }

  auto copyTo = [&](char* buf) {
    size_t index = 0;
    for (auto c : src) {
      U8_APPEND_UNSAFE(buf, index, c);
    }
    assert(index == u8length);
  };

  if (u8length <= String::MAX_SHORT_LENGTH) {
    String::DataBuffer data;
    copyTo(data.buf);
    return String(data.buf, data.buf + u8length);
  }

  // allocate long string then fill it in with no temp buffer
  const auto longStr = allocLongString(u8length);
  copyTo(const_cast<char*>(longStr->m_data));
  // TODO: compute this hash while we're copying the data into the string.
  longStr->metadata().m_hash =
      String::computeStringHash(longStr->m_data, u8length);
  return String(*longStr);
}

String SKIP_String__fromUtf8(const void*, const RObj* src_) {
  const auto& src = *reinterpret_cast<const AObj<uint8_t>*>(src_);

  const char* u8string = (const char*)&src;
  size_t u8length = src.arraySize();

  UErrorCode ec = U_ZERO_ERROR;
  u_strFromUTF8(nullptr, 0, nullptr, u8string, u8length, &ec);
  if (!U_SUCCESS(ec) && (ec != U_BUFFER_OVERFLOW_ERROR)) {
    throwRuntimeError("Invalid utf8 sequence");
  }

  return String(u8string, u8string + u8length);
}

double SKIP_String__toFloat_raw(String s) {
  String::DataBuffer buf;
  std::string str = s.data(buf);
  std::string::size_type sz;
  return std::stod(str, &sz);
}

struct toIntOptionHelperRet_t SKIP_String_toIntOptionHelper(String s) {
  String::DataBuffer buf;
  auto sp = s.slice(buf);
  if (sp.empty())
    return {false, 0};

  bool negative = false;
  if (sp.front() == '-') {
    negative = true;

    sp.pop_front();
    if (sp.empty())
      return {false, 0};
  }

  if (sp.front() == '0') {
    if (negative || (sp.size() > 1))
      return {false, 0};
    return {true, 0};
  }

  // more than 19 digits and we're guaranteed to overflow
  if (sp.size() > 19)
    return {false, 0};

  uint64_t res = 0;
  while (!sp.empty()) {
    uint8_t ch = sp.front() - '0';
    if (ch > 9)
      return {false, 0};
    res = res * 10 + ch;
    sp.pop_front();
  }

  auto ires = (int64_t)res;
  if (negative) {
    if ((ires < 0) && (ires != std::numeric_limits<int64_t>::min())) {
      return {false, 0};
    }
    return {true, -ires};
  } else {
    if (ires < 0)
      return {false, 0};
    return {true, ires};
  }
}

String SKIP_String__sliceByteOffsets(String str, SkipInt start, SkipInt end) {
  return str.sliceByteOffsets(start, end);
}

// --------------------------------------------------------------------------
// StringIterator

String SKIP_String_StringIterator__substring(RObj* start, RObj* end) {
  return start->cast<StringIterator>().substring(end->cast<StringIterator>());
}

SkipInt SKIP_String_StringIterator__rawCurrent(RObj* i) {
  return i->cast<StringIterator>().rawCurrent();
}

SkipInt SKIP_String_StringIterator__rawNext(RObj* i) {
  return i->cast<StringIterator>().rawNext();
}

SkipInt SKIP_String_StringIterator__rawPrev(RObj* i) {
  return i->cast<StringIterator>().rawPrev();
}

void SKIP_String_StringIterator__rawDrop(RObj* i, SkipInt n) {
  i->cast<StringIterator>().rawDrop(n);
}

// --------------------------------------------------------------------------
// ConvertBuffer

namespace {
struct ConvertBuffer : skip::RObj {
  skip::AObj<uint8_t>* m_input;
  skip::AObj<uint8_t>* m_output;
  skip::AObj<uint8_t>* m_pivot;
  UConverter* m_sourceEncoder;
  UConverter* m_targetEncoder;
  SkipInt m_inputUsed;
  SkipInt m_outputUsed;
  SkipInt m_pivotSource;
  SkipInt m_pivotTarget;

  void convert(bool reset, bool flush) {
    const char* const input = (char*)m_input;
    const char* inputUsed = input + m_inputUsed;
    const char* const inputEnd = input + m_input->userByteSize();

    char* const output = (char*)m_output;
    char* outputUsed = output;
    char* const outputEnd = output + m_output->userByteSize();

    UErrorCode ec = U_ZERO_ERROR;

    UChar* const pivot = (UChar*)m_pivot;
    UChar* pivotSource = pivot + m_pivotSource;
    UChar* pivotTarget = pivot + m_pivotTarget;
    ucnv_convertEx(
        m_targetEncoder,
        m_sourceEncoder,
        &outputUsed,
        outputEnd,
        &inputUsed,
        inputEnd,
        pivot,
        &pivotSource,
        &pivotTarget,
        (const UChar*)mem::add(m_pivot, m_pivot->userByteSize()),
        reset,
        flush,
        &ec);

    m_pivotSource = pivotSource - pivot;
    m_pivotTarget = pivotTarget - pivot;
    m_inputUsed = inputUsed - input;
    m_outputUsed = outputUsed - output;

    if (ec != U_ZERO_ERROR) {
      switch (ec) {
        case U_INVALID_CHAR_FOUND:
          throwRuntimeError("invalid character found");
          break;
        default:
          throwRuntimeError("unknown error: %d", ec);
          break;
      }
    }
  }
};

enum class BadCharCode : SkipInt {
  default_ = 0,
  question = 1,
  throw_ = 2,
};
} // namespace

void SKIP_String_ConvertBuffer__setup(
    SkipRObj* buf_,
    skip::String inputEncoding,
    skip::String outputEncoding,
    SkipBool throwOnBadCharacter) {
  auto& buf = *static_cast<ConvertBuffer*>(buf_);
  UErrorCode ec = U_ZERO_ERROR;

  skip::String::CStrBuffer cbuf;
  buf.m_sourceEncoder = ucnv_open(inputEncoding.c_str(cbuf), &ec);
  if (!buf.m_sourceEncoder) {
    throwRuntimeError(
        "Error creating converter '%s'", inputEncoding.c_str(cbuf));
  }

  buf.m_targetEncoder = ucnv_open(outputEncoding.c_str(cbuf), &ec);
  if (!buf.m_targetEncoder) {
    throwRuntimeError(
        "Error creating converter '%s'", outputEncoding.c_str(cbuf));
  }

  if (throwOnBadCharacter) {
    ucnv_setFromUCallBack(
        buf.m_targetEncoder,
        UCNV_FROM_U_CALLBACK_STOP,
        nullptr,
        nullptr,
        nullptr,
        &ec);
    if (ec != U_ZERO_ERROR) {
      throwRuntimeError("Error setting subst callback");
    }
  }

  buf.convert(true, false);
}

void SKIP_String_ConvertBuffer__teardown(SkipRObj* buf_) {
  auto& buf = *static_cast<ConvertBuffer*>(buf_);
  if (buf.m_targetEncoder) {
    ucnv_close(buf.m_targetEncoder);
    buf.m_targetEncoder = nullptr;
  }
  if (buf.m_sourceEncoder) {
    ucnv_close(buf.m_sourceEncoder);
    buf.m_sourceEncoder = nullptr;
  }
}

void SKIP_String_ConvertBuffer__convert(SkipRObj* buf_) {
  auto& buf = *static_cast<ConvertBuffer*>(buf_);
  buf.convert(false, false);
}

void SKIP_String_ConvertBuffer__flush(SkipRObj* buf_) {
  auto& buf = *static_cast<ConvertBuffer*>(buf_);
  buf.convert(false, true);
}
