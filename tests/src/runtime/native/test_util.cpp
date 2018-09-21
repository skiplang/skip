/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/util.h"

#include "skip/memory.h"

#include "testutil.h"

#include <gtest/gtest.h>

using skip::roundDown;
using skip::roundUp;

TEST(UtilTest, testRoundUp) {
  EXPECT_EQ(roundUp(3, 4), 4);
  EXPECT_EQ(roundUp(3, 8), 8);
  EXPECT_EQ(roundUp(12U, 2), 12U);
  EXPECT_EQ(roundUp(14U, 4), 16U);

  // Make sure we can align integers at compile time.
  static_assert(roundUp(19, 4) == 20, "Should have worked at compile time.");

  static std::array<int, roundUp(13, 8)> q;
  static_assert(sizeof(q) == sizeof(int) * 16, "Bad size.");

  // Try aligning some pointers.
  std::array<char, 100> x;
  for (int a = 0; a < 5; ++a) {
    const size_t align = 1ULL << a;

    for (size_t i = 0; i < sizeof(x); ++i) {
      char* p = &x[i];

      uintptr_t r = reinterpret_cast<uintptr_t>(roundUp(p, align));
      EXPECT_EQ(r % align, 0U);
      EXPECT_GE(r, reinterpret_cast<uintptr_t>(p));
      EXPECT_LT(r - reinterpret_cast<uintptr_t>(p), align);
    }
  }
}

TEST(UtilTest, testRoundDown) {
  EXPECT_EQ(roundDown(3, 4), 0);
  EXPECT_EQ(roundDown(5, 4), 4);
  EXPECT_EQ(roundDown(3, 8), 0);
  EXPECT_EQ(roundDown(9, 8), 8);
  EXPECT_EQ(roundDown(12U, 2), 12U);
  EXPECT_EQ(roundDown(14U, 4), 12U);

  // Make sure we can align integers at compile time.
  static_assert(roundDown(19, 4) == 16, "Should have worked at compile time.");

  static std::array<int, roundDown(13, 8)> q;
  static_assert(sizeof(q) == sizeof(int) * 8, "Bad size.");

  // Try aligning some pointers.
  std::array<char, 100> x;
  for (int a = 0; a < 5; ++a) {
    const size_t align = 1ULL << a;

    for (size_t i = 0; i < sizeof(x); ++i) {
      char* p = &x[i];

      uintptr_t r = reinterpret_cast<uintptr_t>(roundDown(p, align));
      EXPECT_EQ(r % align, 0U);
      EXPECT_LE(r, reinterpret_cast<uintptr_t>(p));
      EXPECT_LT(reinterpret_cast<uintptr_t>(p) - r, align);
    }
  }
}

#if OBSTACK_WP_FROZEN
TEST(UtilTest, testMemory) {
  using namespace skip;
  using namespace skip::test;

  char* p = (char*)mem::allocReadOnlyMirror(
      mem::kReadOnlyMirrorSize, 2 * mem::kReadOnlyMirrorSize);
  EXPECT_WRITABLE(p + 0);
  EXPECT_NOT_WRITABLE(p + mem::kReadOnlyMirrorSize);

  {
    LameRandom random(123);
    random.fill(p, mem::kReadOnlyMirrorSize);
  }

  {
    LameRandom random(123);
    EXPECT_TRUE(random.compareFill(
        mem::add(p, mem::kReadOnlyMirrorSize), mem::kReadOnlyMirrorSize));
  }

  mem::freeReadOnlyMirror(p, mem::kReadOnlyMirrorSize);
}
#endif
