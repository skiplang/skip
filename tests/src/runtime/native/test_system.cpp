/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "testutil.h"

#include "skip/String.h"
#include "skip/System.h"
#include "skip/System-extc.h"

using namespace skip;
using namespace skip::test;

namespace {

TEST(SystemTest, test_itoa) {
  auto interestingNumbers = generateInterestingInt64s();
  for (int64_t i : interestingNumbers) {
    std::string s1 = std::to_string(i);
    String s2 = SKIP_Int_toString(i);
    EXPECT_EQ(String(s1.begin(), s1.end()), s2);
  }
}

TEST(SystemTest, test_ftoa) {
  EXPECT_EQ(String("0.0"), (String)SKIP_Float_toString(0));
  EXPECT_EQ(String("3.1400000000000001"), (String)SKIP_Float_toString(3.14));
  EXPECT_EQ(String("-3.1400000000000001"), (String)SKIP_Float_toString(-3.14));
  EXPECT_EQ(
      String("1.7976931348623157e+308"),
      (String)SKIP_Float_toString(std::numeric_limits<double>::max()));
  EXPECT_EQ(
      String("2.2250738585072014e-308"),
      (String)SKIP_Float_toString(std::numeric_limits<double>::min()));
  EXPECT_EQ(
      String("4.9406564584124654e-324"),
      (String)SKIP_Float_toString(std::numeric_limits<double>::denorm_min()));
  EXPECT_EQ(
      String("nan"),
      (String)SKIP_Float_toString(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_EQ(
      String("inf"),
      (String)SKIP_Float_toString(std::numeric_limits<double>::infinity()));
  EXPECT_EQ(
      String("-inf"),
      (String)SKIP_Float_toString(-std::numeric_limits<double>::infinity()));
}

template <typename FN>
void retry(FN fn) {
  for (int i = 0; i < 5; ++i) {
    if (fn())
      return;
  }

  EXPECT_TRUE(fn());
}

TEST(SystemTest, test_profile) {
  // Try each test a few times, just in case something jumps in (page swap, etc)
  // and delays us for a really long time during the profile.
  retry([]() {
    SKIP_profile_start();
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    auto v = SKIP_profile_stop();
    // It should have taken at least 50ms but if it took more than 100ms
    // something is off...
    return ((v >= 50) && (v < 100));
  });
}
} // anonymous namespace
