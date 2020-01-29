/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <cstddef>

#include <boost/functional/hash.hpp>

#include <gtest/gtest.h>

#include "skip/String.h"
#include "skip/String-extc.h"
#include "skip/Type.h"
#include "skip/leak.h"
#include "skip/objects.h"
#include "skip/Obstack.h"
#include "testutil.h"

namespace skip {
namespace {

using namespace skip::test;

TEST(StringTest, testMetadata) {
  static_assert(sizeof(LongStringMetadata) == 8, "incorrect size");
  union {
    LongStringMetadata metadata;
    size_t word;
  };

  metadata.m_hash = (uint32_t)-1;
  metadata.m_byteSize = 0;
  EXPECT_EQ(word, 0xFFFFFFFF00000000ULL);

  metadata.m_hash = 0;
  metadata.m_byteSize = (arraysize_t)-1;
  EXPECT_EQ(word, 0x00000000FFFFFFFFULL);
}

TEST(StringTest, testShortString) {
  Obstack::PosScope pos(Obstack::cur());

  auto s1 = String("Str1");
  auto s2 = String("Str1");
  auto s3 = String("Str2");

  EXPECT_EQ(s1, s2);
  EXPECT_NE(s1, s3);
  String::CStrBuffer buf;
  EXPECT_EQ(0, strcmp(s1.c_str(buf), "Str1"));
  EXPECT_EQ(s1.byteSize(), 4U);
  EXPECT_EQ(0xe000000031727453ULL, s1.hash());
  EXPECT_EQ(0xe000000032727453ULL, s3.hash());
  EXPECT_EQ(s1.asLongString(), nullptr);
  EXPECT_EQ(s2.asLongString(), nullptr);
  EXPECT_EQ(s3.asLongString(), nullptr);

  auto s4 = String();
  EXPECT_EQ(s4.byteSize(), 0U);
}

TEST(StringTest, testLongString) {
  Obstack::PosScope pos(Obstack::cur());

  auto s1 = String("The quick brown fox jumped over the lazy dogs");
  auto s2 = String("The quick brown fox jumped over the lazy dogs");
  auto s3 = String("Str1");
  auto s4 = String("The quick brown foxx jumped over the lazy dogs");

  EXPECT_EQ(s1.byteSize(), 45U);
  EXPECT_EQ(s1, s2);
  EXPECT_NE(s1, s3);
  EXPECT_NE(s1, s4);
  String::CStrBuffer buf;
  EXPECT_EQ(
      0,
      strcmp(s1.c_str(buf), "The quick brown fox jumped over the lazy dogs"));
  EXPECT_EQ(0xf826ad620000002dULL, s1.hash());
  EXPECT_EQ(0xffd5e0290000002eULL, s4.hash());
  EXPECT_NE(s1.asLongString(), nullptr);
  EXPECT_NE(s2.asLongString(), nullptr);
  EXPECT_EQ(s3.asLongString(), nullptr);
  EXPECT_NE(s4.asLongString(), nullptr);
  EXPECT_TRUE(s1.asLongString()->vtable().isFrozen());
  EXPECT_TRUE(s2.asLongString()->vtable().isFrozen());
  EXPECT_TRUE(s4.asLongString()->vtable().isFrozen());
}

TEST(StringTest, testcstr) {
  LameRandom random(42);

  for (size_t i = 0; i < 5000; ++i) {
    Obstack::PosScope pos(Obstack::cur());
    std::string s0(i, 'x');
    for (size_t j = 0; j < i; ++j) {
      s0[j] = static_cast<char>(random.next(126) + 1);
    }
    auto s1 = String(s0.begin(), s0.end());
    EXPECT_EQ(s0.size(), s1.byteSize());
    String::DataBuffer buf1;
    EXPECT_EQ(0, memcmp(s0.data(), s1.data(buf1), s0.size()));
    String::CStrBuffer buf2;
    auto pc = s1.c_str(buf2);
    EXPECT_EQ(0, memcmp(s0.data(), pc, s0.size()));
    EXPECT_EQ('\0', *(pc + s0.size()));
  }
}

TEST(StringTest, testCompare) {
  Obstack::PosScope pos(Obstack::cur());

  auto s1 = String("abcd");
  auto s2 = String("abce");
  auto s3 = String("abcdefgh");
  auto s4 = String("abcdefgi");
  EXPECT_GE(s2, s1);
  EXPECT_GE(s2, s3);
  EXPECT_GE(s2, s4);
  EXPECT_GE(s3, s1);
  EXPECT_GE(s4, s1);
  EXPECT_GE(s4, s3);
  EXPECT_LT(s1, s2);
  EXPECT_LT(s1, s3);
  EXPECT_LT(s1, s4);
  EXPECT_LT(s3, s2);
  EXPECT_LT(s3, s4);
  EXPECT_LT(s4, s2);
}

TEST(StringTest, testCompareTorture) {
  Obstack::PosScope pos(Obstack::cur());

  std::vector<String> strings;
  for (auto i : generateInterestingStrings()) {
    strings.push_back(String(i.begin(), i.end()));
  }

  for (size_t i = 0; i < strings.size(); ++i) {
    for (size_t j = 0; j < strings.size(); ++j) {
      auto cmp = strings[i].cmp(strings[j]);
      bool eq = strings[i] == strings[j];
      if (i == j) {
        EXPECT_EQ(cmp, 0);
        EXPECT_TRUE(eq);
      } else if (i < j) {
        EXPECT_LT(cmp, 0);
        EXPECT_FALSE(eq);
      } else {
        EXPECT_GT(cmp, 0);
        EXPECT_FALSE(eq);
      }
    }
  }
}

TEST(StringTest, test_crc32) {
  LameRandom rand(42);
  std::array<char, 1024> data;
  rand.fill(data.data(), data.size());

  EXPECT_EQ(String::computeStringHash(data.data(), 0), (uint32_t)-1);

  static const std::vector<std::pair<std::pair<size_t, size_t>, uint32_t>>
      tests = {
          {{0, 1}, 0x9ecd5b1f},   {{0, 2}, 0xc76232bd},
          {{0, 3}, 0x9f34c8c6},   {{0, 4}, 0xd32abd9a},
          {{0, 5}, 0xcd0b6e01},   {{0, 6}, 0xe3193db6},
          {{0, 7}, 0xba3d9681},   {{1, 1}, 0xab3c4f03},
          {{1, 2}, 0x81355b55},   {{1, 3}, 0xd03a8892},
          {{1, 4}, 0xbc12dc02},   {{1, 5}, 0xf038d7f0},
          {{1, 6}, 0xddf44d3f},   {{1, 7}, 0xce897251},
          {{0, 10}, 0xa5fd7561},  {{0, 20}, 0x9d4e08c0},
          {{0, 40}, 0x876ffdf8},  {{0, 80}, 0xe2404679},
          {{0, 160}, 0xa5d3ce89}, {{0, 320}, 0x9af5a304},
          {{0, 640}, 0x811aacb8}, {{0, 1024}, 0xcf6ea63c},
      };

  for (auto i : tests) {
    auto computed =
        String::computeStringHash(data.data() + i.first.first, i.first.second);
    EXPECT_EQ(i.second, computed);
  }

  // Check that our hash is case-insensitive.
  EXPECT_EQ(
      String::computeStringHash("ThIs iS A TeSt.", 5),
      String::computeStringHash("tHiS Is a tEsT.", 5));
}

TEST(NativeStringTest, test_unsafe_get) {
  static const char* testChars[] = {"a", "b", "\u20AC", "c", "d"};
  static const int testExpect[] = {'a', 'b', 0x20AC, 'c', 'd'};

  std::string src;
  for (size_t len = 0; len <= sizeof(testChars) / sizeof(testChars[0]); ++len) {
    if (len > 0)
      src.append(testChars[len - 1]);
    String s(src);
    EXPECT_EQ(src.size(), s.byteSize());
    for (size_t k = 0; k < len; ++k) {
      EXPECT_EQ(testExpect[k], SKIP_String__unsafe_get(s, k));
    }
  }
}

TEST(NativeStringTest, test_concat) {
  const auto stringset = generateInterestingStrings();

  for (auto a : stringset) {
    for (auto b : stringset) {
      auto cat = a + b;
      String sa(a.begin(), a.end());
      String sb(b.begin(), b.end());
      String sc(cat.begin(), cat.end());
      EXPECT_EQ(sc, (String)SKIP_String_concat2(sa, sb));
    }
  }

  const std::vector<std::string> strings(stringset.begin(), stringset.end());

  LameRandom rand(314);
  for (size_t count = 1; count < 10; ++count) {
    for (size_t run = 0; run < 10; ++run) {
      std::string cat;
      std::vector<skip::String> skVec;

      for (size_t i = 0; i < count; ++i) {
        size_t idx = rand.next(strings.size());
        cat += strings[idx];
        skVec.push_back(String(strings[idx].begin(), strings[idx].end()));
      }

      String skCat = SKIP_String_concat(skVec.data(), count);
      EXPECT_EQ(String(cat.begin(), cat.end()), skCat);
    }
  }
}
} // anonymous namespace
} // namespace skip
