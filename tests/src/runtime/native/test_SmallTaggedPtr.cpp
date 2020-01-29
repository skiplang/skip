/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/SmallTaggedPtr.h"

#include <gtest/gtest.h>

using skip::SmallTaggedPtr;
using skip::detail::UIntTypeSelector;

namespace {

//
// Simple tests to make sure UIntTypeSelector returns a matching type.
//

#define CHECK_UINT_TYPE_SELECTOR(SZ)                                   \
  static_assert(                                                       \
      sizeof(UIntTypeSelector<SZ, false, false, false>::type) == (SZ), \
      "Bad size");                                                     \
  static_assert(                                                       \
      alignof(UIntTypeSelector<SZ, false, false, false>::type) ==      \
          (skip::isPowTwo(SZ)                                         \
               ? alignof(typename boost::uint_t<(SZ)*8>::least)        \
               : 1),                                                   \
      "Bad align");                                                    \
  static_assert(                                                       \
      sizeof(UIntTypeSelector<SZ, false, false, true>::type) == (SZ),  \
      "Bad size");                                                     \
  static_assert(                                                       \
      alignof(UIntTypeSelector<SZ, false, false, true>::type) == 1,    \
      "Bad align")

CHECK_UINT_TYPE_SELECTOR(1U);
CHECK_UINT_TYPE_SELECTOR(2U);
CHECK_UINT_TYPE_SELECTOR(3U);
CHECK_UINT_TYPE_SELECTOR(4U);
CHECK_UINT_TYPE_SELECTOR(5U);
CHECK_UINT_TYPE_SELECTOR(6U);
CHECK_UINT_TYPE_SELECTOR(7U);
CHECK_UINT_TYPE_SELECTOR(8U);

#undef CHECK_UINT_TYPE_SELECTOR_FOR_TYPE
#undef CHECK_UINT_TYPE_SELECTOR

//
// Check some specific SmallTaggedPtrs.
//

#define CHECK_SMALL_TAGGED_PTR(TAG_BITS, ALIGN_BITS, PACK_SZ, UNPACK_SZ)       \
  static_assert(                                                               \
      sizeof(SmallTaggedPtr<int, TAG_BITS, false, false, true, ALIGN_BITS>) == \
          PACK_SZ,                                                             \
      "Bad size");                                                             \
  static_assert(                                                               \
      alignof(                                                                 \
          SmallTaggedPtr<int, TAG_BITS, false, false, true, ALIGN_BITS>) == 1, \
      "Bad align");                                                            \
  static_assert(                                                               \
      sizeof(                                                                  \
          SmallTaggedPtr<int, TAG_BITS, false, false, false, ALIGN_BITS>) ==   \
          UNPACK_SZ,                                                           \
      "Bad size");                                                             \
  static_assert(                                                               \
      alignof(                                                                 \
          SmallTaggedPtr<int, TAG_BITS, false, false, false, ALIGN_BITS>) ==   \
          UNPACK_SZ,                                                           \
      "Bad align")

// NOTE: These are probably only true on x86_64...
CHECK_SMALL_TAGGED_PTR(0, 39, 1, 1);
CHECK_SMALL_TAGGED_PTR(1, 39, 2, 2);
CHECK_SMALL_TAGGED_PTR(0, 31, 2, 2);
CHECK_SMALL_TAGGED_PTR(1, 31, 3, 4);
CHECK_SMALL_TAGGED_PTR(0, 23, 3, 4);
CHECK_SMALL_TAGGED_PTR(1, 23, 4, 4);
CHECK_SMALL_TAGGED_PTR(0, 16, 4, 4);
CHECK_SMALL_TAGGED_PTR(1, 16, 4, 4);
CHECK_SMALL_TAGGED_PTR(0, 15, 4, 4);
CHECK_SMALL_TAGGED_PTR(1, 15, 5, 8);
CHECK_SMALL_TAGGED_PTR(0, 7, 5, 8);
CHECK_SMALL_TAGGED_PTR(1, 7, 6, 8);
CHECK_SMALL_TAGGED_PTR(1, 0, 6, 8);
CHECK_SMALL_TAGGED_PTR(2, 0, 7, 8);

#undef CHECK_SMALL_TAGGED_PTR

// Test a SmallTaggedPtr with the given parameters.
template <
    int tagBits,
    bool loadBefore,
    bool loadAfter,
    bool pack,
    int alignBits,
    int ptrBits>
void testSmallTaggedPtr() {
  // Create the pointer type to test.
  using T = SmallTaggedPtr<
      char,
      tagBits,
      loadBefore,
      loadAfter,
      pack,
      alignBits,
      ptrBits>;

  // It should take the minimum number of bytes.
  static_assert(
      pack
          ? (sizeof(T) == (ptrBits + tagBits - alignBits + 7) >> 3)
          : (sizeof(T) ==
             sizeof(
                 typename boost::uint_t<ptrBits + tagBits - alignBits>::least)),
      "Incorrect size.");

  // Packed values must have alignment 1.
  static_assert(!pack || alignof(T) == 1, "pack flag did not work.");

  // Simple (power of two) sizes without packing should be aligned.
  static_assert(
      pack || !skip::isPowTwo(sizeof(T)) ||
          alignof(T) == alignof(typename boost::uint_t<sizeof(T) * 8>::least),
      "alignment unexpectedly low.");

  for (int pattern = 0; pattern <= 0xFF; pattern += 0xFF) {
    for (int ptrBit = alignBits; ptrBit <= ptrBits; ++ptrBit) {
      // Try all pointer values with 1 bit, as well as nullptr.
      uintptr_t ptrVal = ptrBit < ptrBits ? ((uintptr_t)1 << ptrBit) : 0;
      char* ptr = reinterpret_cast<char*>(ptrVal);

      for (int tagBit = 0; tagBit <= tagBits; ++tagBit) {
        // Try all tag values with 1 bit, as well as 0.
        const typename T::TagBits tag = tagBit < tagBits ? 1ull << tagBit : 0;

        // Place the value in a struct starting with known bit patterns, so we
        // can detect any bugs related to accidentally using bits outside
        // the value
        struct {
          char padding1[15];
          T val;
          char padding2[15];
        } x;
        memset(&x, pattern, sizeof(x));

        x.val.assign(ptr, tag);

        auto v = x.val.unpack();
        EXPECT_EQ(v.m_ptr, ptr);
        EXPECT_EQ(v.m_tag, tag);
      }
    }
  }
}

//
// NOTE: The following template junk just iterates through all supported
// template parameter combinations at compile time so we can test them all.
//

// Iterate over all tag bit sizes for a given ptrBits and alignBits.
template <int ptrBits, int alignBits, int tagBits>
struct Test2 {
  static void runTest() {
    // Try all combinations of SmallTaggedPtr parameters.
    testSmallTaggedPtr<tagBits, false, false, false, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, false, false, true, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, false, true, false, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, false, true, true, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, true, false, false, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, true, false, true, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, true, true, false, alignBits, ptrBits>();
    testSmallTaggedPtr<tagBits, true, true, true, alignBits, ptrBits>();

    // Try next smallest number of tag bits.
    Test2<ptrBits, alignBits, tagBits - 1>::runTest();
  }
};

// Recursion base case.
template <int ptrBits, int alignBits>
struct Test2<ptrBits, alignBits, -1> {
  static void runTest() {}
};

// Iterate over all alignments for a given pointer size.
template <int ptrBits, int alignBits>
struct Test1 {
  static void runTest() {
    const int maxTagBits = sizeof(void*) * 8 - ptrBits + alignBits;
    Test2<ptrBits, alignBits, maxTagBits>::runTest();

    // Try smaller alignBits values.
    Test1<ptrBits, alignBits - 1>::runTest();
  }
};

// Recursion base case.
template <int ptrBits>
struct Test1<ptrBits, -1> {
  static void runTest() {}
};

// Iterate over all pointer sizes.
template <int ptrBits = skip::detail::kMaxPtrBits>
struct Test0 {
  static void runTest() {
    // Only bother testing alignments up to 128, for testing speed.
    Test1<ptrBits, 7>::runTest();

    // Recursively try smaller ptrBits values.
    Test0<ptrBits - 1>::runTest();
  }
};

// Recursion base case -- only bother testing certain pointer sizes.
template <>
struct Test0<skip::detail::kMinPtrBits - 1> {
  static void runTest() {}
};
} // namespace

TEST(SmallTaggedPtr, runTest) {
  Test0<>::runTest();
}
