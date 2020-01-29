/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "StringRep.h"
#include "objects.h"
#include "Type.h"

#include <atomic>

namespace skip {

struct LongString final : RObj {
  WithVTable(LongStringMetadata);

  // variable length member - according to gcc-6 must be 0 or you get
  // the error "flexible array member in an otherwise empty struct"
  const char m_data[0];

  arraysize_t byteSize() const {
    return metadata().m_byteSize;
  }
  arraysize_t paddedSize() const;

  void verifyInvariants() const;

  static Type& static_type();

 private:
  // In opt mode the compiler complains about m_data[] if we don't have a
  // constructor.
  LongString() = default;
};

/**
 * An immutable string.  Strings are represented as a value-class which can
 * contain short strings directly or can point to LongString objects which
 * contain longer strings.
 *
 * Note that it is a requirement that if a string can fit into the short string
 * format (7 bytes or less) then it MUST be contained in a short string.
 *
 * Short String format:
 *   The MSB is a tag byte:
 *     1 1 s2 s1 s0 x x x
 *   Where:
 *     s2 s1 s0 - A 3 bit length to indicate how long the string is.
 *     x - unused - must be zero for hashing/equality to work. Separately,
 *                  at least one must be zero to guarantee that SentinelOption
 *                  can treat negative pointers as reserved. If any of this
 *                  changes make sure to adjust kUnusedTagBits.
 *
 *   The highest bit must be a '1' to indicate that this is a fake pointer.
 *   The second highest bit is a '1' to make some operations faster.
 *
 *   The remaining 7 bytes make a utf8 encoded string, zero padded.
 *
 * Long String format:
 *   The 64-bit value is a pointer to a LongString.  The high bit must be clear.
 */
struct String final : StringRep, boost::less_than_comparable<String> {
  String();
  /* implicit */ String(StringRep s) {
    *static_cast<StringRep*>(this) = s;
  }
  String(const char* begin, const char* end);
  explicit String(skip::StringPiece range)
      : String(range.begin(), range.begin() + range.size()) {}
  explicit String(std::string str) : String(str.begin(), str.end()) {}
  explicit String(const char* cstr) : String(cstr, cstr + strlen(cstr)) {}
  explicit String(const LongString& p) {
    m_longString = &p;
  }
  explicit String(int64_t sbits) {
    m_sbits = sbits;
  }

  // When using fbstring as a string replacement this aliases the (const char*,
  // const char*) constructor.
  template <
      typename = std::enable_if<
          std::is_same<const char*, std::string::const_iterator>::value>>
  String(std::string::const_iterator begin, std::string::const_iterator end)
      : String(&*begin, &*end) {}

  String(
      std::vector<char>::const_iterator begin,
      std::vector<char>::const_iterator end)
      : String(&*begin, &*end) {}

  String(const String& o) = default;
  String& operator=(const String& o) = default;

  int64_t sbits() const {
    return m_sbits;
  }
  uint64_t bits() const {
    return m_bits;
  }
  static String fromSBits(int64_t bits) {
    return String(bits);
  }

  // Prevent implicit conversion to bool through SkipString.
  operator bool() const = delete;

  // NOTE: These are for simple comparison and set operations - not for proper
  // unicode operations.
  bool operator==(const String& o) const;
  bool operator!=(const String& o) const {
    return !(*this == o);
  }
  bool operator<(const String& o) const {
    return cmp(o) < 0;
  }
  ssize_t cmp(const String& o) const;

  // The size of a buffer used to hold temporary c_str() values.  Strings this
  // long or longer are padded to include an extra NUL so they will be
  // NUL-terminated.
  //
  // WARNING: This must equal cstr_buffer_size in the compiler.
  //
  // JeMalloc has block sizes of (64, 80, 96) so pick it so we fill one of those
  // before forcing an internal NUL.
  static constexpr size_t CSTR_BUFFER_SIZE =
      80 - sizeof(LongStringMetadata) + 1;

  using CStrBuffer = std::array<char, CSTR_BUFFER_SIZE>;

  // Return a NUL-terminated utf8 string.  The passed in buffer should be at
  // least CSTR_BUFFER_SIZE bytes and may or may not be used.
  // NOTE: This just returns the utf8 stream - it doesn't convert to another
  // encoding.
  const char* c_str(char buffer[CSTR_BUFFER_SIZE]) const;
  const char* c_str(CStrBuffer& buffer) const {
    return c_str(buffer.data());
  }

  std::string toCppString() const;

  // Return a pointer to the raw data.  Does not guarantee NUL termination.  The
  // passed in buffer should be at least DATA_BUFFER_SIZE bytes and may or may
  // not be used.
  const char* data(DataBuffer& buffer) const;

  skip::StringPiece slice(DataBuffer& buffer) const;

  void clear();

  size_t byteSize() const;
  size_t hash() const;
  size_t countCharacters() const;

  const LongString* asLongString() const {
    return asFakePtr().asPtr();
  }

  String sliceByteOffsets(int64_t start, int64_t end);

  static String concat2(String a, String b);
  static String concat(const String* strings, size_t size);

  static uint32_t computeStringHash(const void* ptr, arraysize_t byteSize);
  static size_t computePaddedSize(size_t byteCount);

 private:
  friend struct StringIterator;

  static_assert(
      sizeof(ssize_t) == 8,
      "If this fails you've got some work to do");

  detail::TOrFakePtr<const LongString> asFakePtr() const {
    return m_longString;
  }

  bool isLongString() const {
    return repIsLong(*this);
  }
  bool isShortString() const {
    return !repIsLong(*this);
  }

  // This monstrosity (with the const& and const&& below) ensures that
  // unsafeData() can't be called on a temporary.
  const char* unsafeData() const& {
    if (auto p = asLongString()) {
      return p->m_data;
    } else {
      return m_data.buf;
    }
  }
  const char* unsafeData() const&& = delete;
};

struct OptString final {
  String m_str;

  enum NullInit {};
  OptString(NullInit) : m_str{String::fromSBits(0)} {}
  OptString(String s) : m_str(s) {}

  bool isSome() const {
    return m_str.sbits() != 0;
  }
  String str() const {
    return m_str;
  }
};

// StringPtr is a smart pointer which manages the lifetimes of its owned string.
// We can't use boost::intrusive_ptr<String> because that would then be a
// String* - which we don't want.  The owned string MUST be interned.
struct StringPtr final {
  ~StringPtr();
  StringPtr() = default;
  StringPtr(const StringPtr& o);
  StringPtr& operator=(const StringPtr& o);
  StringPtr(StringPtr&& o) noexcept;
  StringPtr& operator=(StringPtr&& o) noexcept;

  StringPtr(const char* begin, const char* end);
  explicit StringPtr(String s, bool incref = true);
  explicit StringPtr(const LongString& s, bool incref = true);

  String get() const {
    return m_string;
  }
  String release() noexcept;
  void reset();

  String operator*() const {
    return m_string;
  }
  const String* operator->() const {
    return &m_string;
  }

 private:
  String m_string;

  void incref();
  void decref();
};

AObj<String>* createStringVector(size_t sz);
} // namespace skip
