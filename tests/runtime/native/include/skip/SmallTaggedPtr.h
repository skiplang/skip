/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "util.h"

#include <cassert>
#include <cstdint>
#include <cstring>

#include <algorithm>

namespace skip {
namespace detail {

/**
 * A wrapper around an unaligned unsigned integer that supports simple
 * assignment and comparison operations.
 *
 * Do not use this class directly; use UIntTypeSelector to choose
 * the right class.
 */
template <typename T>
struct __attribute__((packed)) UnalignedValue {
  /* implicit */ operator T() const {
    return m_bits;
  }

  UnalignedValue& operator=(T n) {
    m_bits = n;
    return *this;
  }

  UnalignedValue& operator=(const UnalignedValue& other) = default;

  bool operator==(const UnalignedValue& other) {
    return m_bits == other;
  }

  bool operator<(const UnalignedValue& other) {
    return m_bits < other;
  }

 private:
  static_assert(sizeof(T) > 1, "No need to pack one-byte value.");

  T m_bits;
};

/**
 * Originally we were using boost::static_log2 and boost::uint_t,
 * but it's silly to depend on boost just for that, so I replaced
 * them with the following section. Added a bunch of assertions to
 * make sure the assumption I made on sizes were correct. Of course
 * we will have to adjust once support for new architectures is added.
 */

template <size_t>
class StaticLog2;

template <>
class StaticLog2<8> {
 public:
  static const size_t value = 4;
};

template <>
class StaticLog2<64> {
 public:
  static const size_t value = 6;
};

template <size_t>
class UInt;

template <>
class UInt<1> {
 public:
  typedef unsigned char fast;
  static_assert(sizeof(fast) == 1);
};

template <>
class UInt<5> {
 public:
  typedef unsigned char fast;
  static_assert(sizeof(fast) == 1);
};

template <>
class UInt<7> {
 public:
  typedef unsigned char fast;
  static_assert(sizeof(fast) == 1);
};

template <>
class UInt<16> {
 public:
  typedef unsigned short exact;
  static_assert(sizeof(exact) == 2);
};

template <>
class UInt<19> {
 public:
  typedef unsigned int fast;
  static_assert(sizeof(fast) == 4);
};

template <>
class UInt<31> {
 public:
  typedef unsigned int fast;
  static_assert(sizeof(fast) == 4);
};

template <>
class UInt<32> {
 public:
  typedef unsigned int exact;
  static_assert(sizeof(exact) == 4);
};

template <>
class UInt<41> {
 public:
  typedef unsigned long least;
  static_assert(sizeof(least) == 8);
};

template <>
class UInt<43> {
 public:
  typedef unsigned long least;
  static_assert(sizeof(least) == 8);
};

template <>
class UInt<44> {
 public:
  typedef unsigned long least;
  static_assert(sizeof(least) == 8);
};

template <>
class UInt<47> {
 public:
  typedef unsigned long least;
  static_assert(sizeof(least) == 8);
};

template <>
class UInt<48> {
 public:
  typedef unsigned long least;
  static_assert(sizeof(least) == 8);
};

template <>
class UInt<64> {
 public:
  typedef unsigned long least;
  typedef unsigned long exact;
  static_assert(sizeof(least) == 8);
  static_assert(sizeof(exact) == 8);
};

/**
 * Holds an unsigned integer that is guaranteed to fit in "size" bytes,
 * rather than rounding up to the next power of two size like normal ints.
 *
 * Do not use this class directly; use UIntTypeSelector to choose
 * the most efficient implementation.
 *
 * @param Lo The type holding the low bits of the number.
 *
 * @param Hi The type holding the high bits of the number.
 *
 * @param safeToLoadBefore See documentation for SmallTaggedPtr.
 *
 * @param safeToLoadAfter See documentation for SmallTaggedPtr.
 */
template <typename Lo, typename Hi, bool safeToLoadBefore, bool safeToLoadAfter>
struct PackedUInt {
  // Smallest unsigned integer type large enough to hold this value.
  using UIntType = typename skip::detail::UInt<(sizeof(Lo) + sizeof(Hi)) * 8>::least;

  /* implicit */ operator UIntType() const {
    // Combine the unaligned pieces into a single number.
    const UIntType loBits = m_lo;
    const UIntType hiBits = m_hi;
    return loBits | (hiBits << (sizeof(m_lo) * 8));
  }

  PackedUInt& operator=(UIntType n) {
    m_lo = n;
    m_hi = n >> (sizeof(m_lo) * 8);
    return *this;
  }

  PackedUInt& operator=(const PackedUInt& other) = default;

  bool operator==(const PackedUInt& other) {
    return m_lo == other.m_lo && m_hi == other.m_hi;
  }

  bool operator<(const PackedUInt& other) {
    return static_cast<UIntType>(*this) < static_cast<UIntType>(other);
  }

 private:
  Lo m_lo;
  Hi m_hi;
};

/**
 * Converts a bit count to the number of bytes needed to hold those bits.
 */
constexpr size_t bitsToBytes(size_t bits) {
  return (bits + 7) >> 3;
}

#ifdef __x86_64__
// x86_64 only has 48-bit virtual addresses, and addresses
// with bit 47 set are reserved for kernel space in Linux, OS X and
// Windows. So userspace pointers are really just 47 bits,
// zero-extended to 64.
constexpr int kMaxPtrBits = 47;
#elif __aarch64__
// Officially aarch64 user addresses have bits 63:48 set to 0 but the
// runtime makes some assumptions about bit sizes - so hopefully this
// isn't too broken :|
constexpr int kMaxPtrBits = 47;
#else
constexpr int kMaxPtrBits = sizeof(void*) * 8;
#endif

// Modifying this to accept more sizes is fine, but will slow the test.
constexpr int kMinPtrBits = kMaxPtrBits;

/**
 * Selects the type to use for a size-byte unsigned integer.
 *
 * @param size The number of bytes for the value.
 *
 * @param safeToLoadBefore See documentation for SmallTaggedPtr.
 *
 * @param safeToLoadAfter See documentation for SmallTaggedPtr.
 *
 * @param pack If true, this value will be unaligned and take the smallest
 *        possible number of bytes because it needs no alignment padding.
 */
template <
    size_t size,
    bool safeToLoadBefore = false,
    bool safeToLoadAfter = false,
    bool pack = true,
    typename Enable = void>
struct UIntTypeSelector {
  // Default case: sizes 3, 5, 6, 7 require multiple unaligned fields.
  using type = detail::PackedUInt<
      typename UIntTypeSelector<size&(size - 1)>::type,
      typename UIntTypeSelector<size & -size>::type,
      safeToLoadBefore,
      safeToLoadAfter>;

  static_assert(sizeof(type) == size, "Incorrect size!");
};

// Specialization for 2, 4, 8 byte packed values.
template <size_t size, bool safeToLoadBefore, bool safeToLoadAfter, bool pack>
struct UIntTypeSelector<
    size,
    safeToLoadBefore,
    safeToLoadAfter,
    pack,
    typename std::enable_if<pack && skip::isPowTwo(size) && size != 1>::type> {
  using type = detail::UnalignedValue<typename skip::detail::UInt<size * 8>::exact>;
};

// Specialization for 1, 2, 4, 8 byte unpacked (aligned) values.
template <size_t size, bool safeToLoadBefore, bool safeToLoadAfter, bool pack>
struct UIntTypeSelector<
    size,
    safeToLoadBefore,
    safeToLoadAfter,
    pack,
    typename std::enable_if<
        skip::isPowTwo(size) && (!pack || size == 1)>::type> {
  // Just use a simple C++ scalar type, not one of our structs.
  using type = typename skip::detail::UInt<size * 8>::exact;
};

// Hack to provide a legal return value for operator* for SmallTaggedPtr<void>.
// You can't use std::enable_if to accomplish this. boost::shared_ptr uses
// this same approach.
template <typename T>
struct DerefType {
  using type = T&;
};

template <>
struct DerefType<void> {
  using type = void;
};
template <>
struct DerefType<const void> {
  using type = void;
};
template <>
struct DerefType<volatile void> {
  using type = void;
};
template <>
struct DerefType<const volatile void> {
  using type = void;
};
} // namespace detail

/*
 * A pair of a pointer and tag, reduced to take as few bytes as possible.
 *
 * On x86_64, C++ pointers take 8 bytes, but they don't strictly need to.
 * Userspace pointers on Linux, OS X and Win64 and are only 47 bits, with
 * the high 17 bits always zero. Furthermore, pointer alignment often yields
 * some number of low bits known to be zero, requiring even fewer bits.
 *
 * Because the packed bits can be inefficient to work with, usually
 * call unpack() to extract a pair of a pointer and the tag bits and work
 * with those.
 *
 * @param T The type of object pointed to.
 *
 * @param numTagBits_ The number of tag bits to be recorded along with
 *        the pointer. If -1, this is set to the maximum value such that
 *        sizeof(*this) == sizeof(void*). The real computed value can be
 *        acquired from kNumTagBits.
 *
 * @param safeToLoadBefore If true, enables a technically illegal optimization
 *        that is safe in practice. "true" means that the bytes preceding
 *        this object can be safely read, as when this is not the first field
 *        in an object. This can allow the use of a more efficient code
 *        sequence to read the value from memory, where the entire containing
 *        word is loaded with one instruction and then excess "garbage bits"
 *        are shifted out. Specifically if this is true the aligned word
 *        ENDING at the same address as this object must be loadable from
 *        memory (where "word" means the smallest integer type at least
 *        as large as this SmallTaggedPtr object). This value is unused if
 *        this object takes a power of two bytes.
 *
 * @param safeToLoadAfter Analogous to safeToLoadBefore, but indicates that
 *        the bytes AFTER this object can be safely dereferenced. Useful for
 *        fields that are not the last field in the object. This value is
 *        unused if this object takes a power of two bytes.
 *
 * @param pack If true (the default), will remove any alignment requirements
 *        from this value and use the fewest number of bytes possible to
 *        represent the pointer and tag with possible performance overhead.  If
 *        false will always be aligned on a log2 boundary and take log2 bytes.
 *
 * @param numAlignBits The number of low bits of the pointer value known to
 *        be zero. Defaults to the value for the known C++ alignment.
 *
 * @param numPtrBits The number of bits required for the pointer. Usually
 *        the default value is appropriate, but if you know (for example) that
 *        the pointer will only point into, say, the first 2**32 or 2**40 bytes,
 *        this can be specified as an even smaller value.
 */
template <
    typename T,
    int numTagBits_ = 0,
    bool safeToLoadBefore = false,
    bool safeToLoadAfter = false,
    bool pack = true,
    int numAlignBits = skip::detail::StaticLog2<alignof(T)>::value,
    int numPtrBits = detail::kMaxPtrBits,
    typename TPtr = T*>
struct SmallTaggedPtr {
  static_assert(
      sizeof(TPtr) == sizeof(void*),
      "TPtr must be the same size as a pointer.");

  static constexpr int kNumPtrBits = numPtrBits;

  static constexpr int kNumTagBits =
      (numTagBits_ >= 0 ? numTagBits_
                        : (sizeof(TPtr) * 8 - numPtrBits + numAlignBits));

  static constexpr int kNumAlignBits = numAlignBits;

  // For test efficiency we do not test all possible pointer sizes.
  static_assert(
      numPtrBits >= detail::kMinPtrBits && numPtrBits <= detail::kMaxPtrBits,
      "Unhandled number of pointer bits. If you really want this "
      "case then modify the limits and make sure the test works.");

  /// Number of bytes taken by this pointer type.
  static constexpr unsigned int numBytes =
      (pack ? detail::bitsToBytes(numPtrBits - kNumAlignBits + kNumTagBits)
            : sizeof(typename skip::detail::UInt<
                     numPtrBits - kNumAlignBits + kNumTagBits>::least));

  static constexpr bool kPack = pack;

  // Right now we assume everything fits in a uintptr_t.
  static_assert(numBytes <= sizeof(void*), "Too many tag bits.");

  static constexpr uintptr_t kTagMask = ((uintptr_t)1 << kNumTagBits) - 1;

  // Defines the layout used to pack together the pointer and tag bits.
  // We need at least (numPtrBits - kNumAlignBits) + kNumTagBits to store the
  // value, but there are usually multiple legal encodings with different
  // efficiency characteristics. The two requirements are that (1) the
  // encoding must fit in numBytes (the theoretical minimum) and (2) the
  // tag bits must be in the low bits.
  enum Layout {
    // Left shift the pointer and its known-zero alignment bits to make room
    // for the tag. Extracting the pointer requires only right-shifting
    // out the tag bits.
    //
    // [PTR, ALIGN, TAG]
    LAYOUT_TAG_APPENDED,

    // Overlap the tag bits inside the alignment bits. Extracting the pointer
    // requires masking out the tag bits.
    // [PTR, ( ALIGN | TAG )]
    LAYOUT_TAG_OVERLAPPED,

    // Right shift away at least some alignment bits then left shift to
    // make room for the tag bits. This layout always works, but is less
    // efficient than LAYOUT_TAG_APPENDED and LAYOUT_TAG_OVERLAPPED, so
    // it is only used when necessary.
    //
    // [PTR, TAG]
    LAYOUT_ALIGNMENT_REMOVED
  };

  // TODO: Should we prefer LAYOUT_TAG_OVERLAPPED to LAYOUT_TAG_APPENDED,
  // since its "assign" is more efficient, even though the bit-AND mask
  // will probably compile to larger code than LAYOUT_TAG_APPENDED's
  // right shift, and we will do more reads than writes?

  static constexpr Layout kLayout =
      (numBytes >= detail::bitsToBytes(numPtrBits + kNumTagBits)
           ? ( // LAYOUT_TAG_APPENDED fits. Prefer it to LAYOUT_TAG_OVERLAPPED
               // if LAYOUT_TAG_OVERLAPPED does not fit, or if the right shift
               // used by LAYOUT_TAG_APPENDED should compile well with the
               // right shift used by the safeToLoadBefore hack.
               // safeToLoadAfter's bitwise AND should compile better with
               // LAYOUT_TAG_OVERLAPPED.
                 (kNumTagBits > kNumAlignBits || safeToLoadBefore ||
                  !safeToLoadAfter)
                     ? LAYOUT_TAG_APPENDED
                     : LAYOUT_TAG_OVERLAPPED)
           : ( // LAYOUT_TAG_APPENDED does not fit. Prefer
               // LAYOUT_TAG_OVERLAPPED.
                 (kNumTagBits <= kNumAlignBits &&
                  numBytes == detail::bitsToBytes(numPtrBits))
                     ? LAYOUT_TAG_OVERLAPPED
                     : LAYOUT_ALIGNMENT_REMOVED));

  using Rep = typename detail::UIntTypeSelector<
      numBytes,
      // Given a choice, prefer "safeToLoadAfter" to "safeToLoadBefore" if our
      // Layout requires a bit-AND to extract the pointer bits anyway, since the
      // compiler can combine that bit-AND with the bit-AND that safeToLoadAfter
      // uses to discard garbage high bits.
      safeToLoadBefore &&
          (!safeToLoadAfter || kLayout != LAYOUT_TAG_OVERLAPPED),
      safeToLoadAfter &&
          (!safeToLoadBefore || kLayout == LAYOUT_TAG_OVERLAPPED),
      kPack>::type;

 public:
  using TagBits = typename skip::detail::UInt<kNumTagBits ? kNumTagBits : 1>::fast;

  /// A pair of a pointer and tag bits.
  struct PointerTagPair {
    explicit PointerTagPair(TPtr p, TagBits t = 0) : m_ptr(p), m_tag(t) {}

    TPtr m_ptr;
    TagBits m_tag;
  };

  /// Efficiently extract the pointer and tag.
  const PointerTagPair unpack() const {
    uintptr_t b = m_bits;

    // The tag is ALWAYS in the low bits of the raw representation.
    const auto tag = static_cast<TagBits>(b & kTagMask);

    switch (kLayout) {
      case LAYOUT_TAG_APPENDED:
        b >>= kNumTagBits;
        break;
      case LAYOUT_TAG_OVERLAPPED:
        b &= ~kTagMask;
        break;
      case LAYOUT_ALIGNMENT_REMOVED:
        b >>= kNumTagBits;
        b <<= kNumAlignBits;
        break;
    }

    return PointerTagPair(typepunned_cast<TPtr>(b), tag);
  }

  /// Extracts just the pointer, discarding the tag. Consider unpack() instead.
  TPtr ptr() const {
    return unpack().m_ptr;
  }

  /// Extracts just the tag, discarding the pointer. Consider unpack() instead.
  TagBits tag() const {
    return unpack().m_tag;
  }

  uintptr_t bits() const {
    return m_bits;
  }

  SmallTaggedPtr() = default;
  SmallTaggedPtr(TPtr ptr, TagBits tag) {
    assign(ptr, tag);
  }
  SmallTaggedPtr(std::nullptr_t) {
    assign(nullptr);
  }
  SmallTaggedPtr(uintptr_t bits) {
    assign(bits);
  }
  SmallTaggedPtr(PointerTagPair p) {
    assign(p);
  }

  void assign(TPtr ptr, TagBits tag) {
    auto b = typepunned_cast<uintptr_t>(ptr);

    assert((b & (((uintptr_t)1 << kNumAlignBits) - 1)) == 0);
    assert(tag <= kTagMask);

    switch (kLayout) {
      case LAYOUT_TAG_APPENDED:
        b <<= kNumTagBits;
        break;
      case LAYOUT_TAG_OVERLAPPED:
        break;
      case LAYOUT_ALIGNMENT_REMOVED:
        // This will compile to either a right shift or left shift, but not
        // both.
        b >>= std::max(kNumAlignBits - kNumTagBits, 0);
        b <<= std::max(kNumTagBits - kNumAlignBits, 0);
        break;
    }

    // NOTE: Using + instead of | is equivalent because the low bits are
    // zero, and + sometimes generates more efficient code because it can
    // use a "load effective address" instruction.
    m_bits = b + tag;
  }

  void assign(std::nullptr_t) {
    m_bits = 0;
  }

  void assign(uintptr_t bits) {
    m_bits = bits;
  }

  void assign(PointerTagPair p) {
    assign(p.ptr, p.tag);
  }

  void reset() {
    assign(0);
  }

  explicit operator bool() const {
    return ptr() != nullptr;
  }

  bool operator==(std::nullptr_t) const {
    return ptr() == nullptr;
  }

  bool operator!=(std::nullptr_t) const {
    return ptr() != nullptr;
  }

  SmallTaggedPtr& operator=(TPtr ptr) {
    assign(ptr, 0);
    return *this;
  }

  SmallTaggedPtr& operator=(std::nullptr_t) {
    m_bits = 0;
    return *this;
  }

  typename detail::DerefType<TPtr>::type operator*() const {
    return *ptr();
  }

  TPtr operator->() const {
    return ptr();
  }

 private:
  Rep m_bits;

  template <typename A, typename B>
  static A typepunned_cast(B _b) {
    union {
      A a;
      B b;
    };
    static_assert(sizeof(A) == sizeof(B), "Mismatched type");
    b = _b;
    return a;
  }
};

// Like SmallTaggedPtr<> but allows you to have a union of pointer values.
template <
    typename T,
    int numTagBits = 0,
    bool safeToLoadBefore = false,
    bool safeToLoadAfter = false,
    bool pack = true,
    int numAlignBits = skip::detail::StaticLog2<alignof(T)>::value,
    int numPtrBits = detail::kMaxPtrBits>
using SmallTaggedUnion = SmallTaggedPtr<
    void,
    numTagBits,
    safeToLoadBefore,
    safeToLoadAfter,
    pack,
    numAlignBits,
    numPtrBits,
    T>;

// A SmallTaggedUnion<> that uses a full void* of size (instead of being
// minimally sized).
template <typename T, int numTagBits>
using TaggedUnion = SmallTaggedUnion<
    T,
    numTagBits,
    false, // safeToLoadBefore
    false, // safeToLoadAfter
    false // pack
    >;
} // namespace skip
