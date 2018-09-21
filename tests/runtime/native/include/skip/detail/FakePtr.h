/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace skip {
namespace detail {

// We use the high bit of a pointer as a tag to indicate that it is data, not a
// pointer.  Mainly we do this for String and LongString (see StringRep.h).
// We consider both nullptr and these "fake" pointers to be invalid and do
// not trace them during GC and interning.
//
template <typename T>
struct TOrFakePtr {
  TOrFakePtr() = default;
  /*implicit*/ TOrFakePtr(T* ptr) : m_ptr(ptr) {}
  explicit TOrFakePtr(uintptr_t word) : m_word(word) {}

  bool operator==(const TOrFakePtr& o) const {
    return m_word == o.m_word;
  }
  bool operator!=(const TOrFakePtr& o) const {
    return !(*this == o);
  }

  bool isFakePtr() const {
    return m_word <= 0;
  }
  uintptr_t bits() const {
    return static_cast<uintptr_t>(m_word);
  }
  intptr_t sbits() const {
    return m_word;
  }
  void setSBits(intptr_t value) {
    m_word = value;
  }

  bool isPtr() const {
    return m_word > 0;
  }
  T* asPtr() const {
    return isFakePtr() ? nullptr : m_ptr;
  }
  T*& unsafeAsPtr() {
#if SKIP_PARANOID
    assert(!isFakePtr());
#endif
    return m_ptr;
  }
  T* const& unsafeAsPtr() const {
#if SKIP_PARANOID
    assert(!isFakePtr());
#endif
    return m_ptr;
  }
  void setPtr(T* p) {
    m_ptr = p;
  }

  T* operator->() const {
    return m_ptr;
  }

  static constexpr size_t RESERVED_BITS = 1;

 private:
  union {
    T* m_ptr;
    intptr_t m_word;
  };
};
} // namespace detail
} // namespace skip
