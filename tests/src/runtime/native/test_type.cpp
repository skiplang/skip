/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Type.h"

#include "testutil.h"

#include <gtest/gtest.h>

using namespace skip;
using namespace skip::test;

namespace {

std::vector<size_t> absOffsets(const std::vector<ssize_t>& offsets) {
  std::vector<size_t> res(offsets.size());
  for (size_t i = 0; i < offsets.size(); ++i)
    res[i] = std::abs(offsets[i]);
  return res;
}

void skipNegativeOffsets(const std::vector<ssize_t>& offsets, size_t& index) {
  while ((index < offsets.size()) && (offsets[index] < 0)) {
    ++index;
  }
}

std::vector<ssize_t> unrollOffsets(
    const std::vector<ssize_t>& offsets,
    size_t userByteSize,
    arraysize_t arraySize) {
  std::vector<ssize_t> unrolledOffsets;
  for (arraysize_t i = 0; i < arraySize; ++i) {
    for (auto offset : offsets) {
      bool neg = offset < 0;
      unrolledOffsets.push_back(
          (std::abs(offset) + userByteSize * i) * (neg ? -1 : 1));
    }
  }
  return unrolledOffsets;
}

void verifyRefs(
    Type& t,
    const std::vector<ssize_t>& offsets,
    arraysize_t arraySize = 1) {
  // Purposely create an RObj without a VTable in the hope that if Type
  // incorrectly tries to access the VTable (incorrect since we already know the
  // Type) it will SEGV.
  // A negative offset indicates an offset that should be filled with a fakeptr.
  const size_t objUserSize = t.userByteSize() * arraySize;
  auto raw = std::unique_ptr<void, decltype(::free)*>(
      ::calloc(t.uninternedMetadataByteSize() + objUserSize, 1), ::free);
  auto robj =
      static_cast<RObj*>(mem::add(raw.get(), t.uninternedMetadataByteSize()));
  static_cast<AObjMetadata*>(raw.get())->m_arraySize = arraySize;

  std::vector<ssize_t> unrolledOffsets =
      unrollOffsets(offsets, t.userByteSize(), arraySize);

  // Fake up some data that looks sort of like pointers (and not nil)
  for (auto offset : unrolledOffsets) {
    assert(std::abs(offset) + sizeof(void*) <= objUserSize);
    const ssize_t value = (offset + 1) * sizeof(void*);
    *((ssize_t*)mem::add(robj, std::abs(offset))) = value;
  }

  size_t nextOffset = 0;
  skipNegativeOffsets(unrolledOffsets, nextOffset);
  t.eachValidRef(*robj, [&](RObj*& ref) {
    const ssize_t off = unrolledOffsets[nextOffset];
    EXPECT_EQ(&ref, mem::add(robj, off));
    EXPECT_EQ((size_t)ref, (off + 1) * sizeof(void*));
    ++nextOffset;
    skipNegativeOffsets(unrolledOffsets, nextOffset);
  });
  EXPECT_EQ(nextOffset, unrolledOffsets.size());

  nextOffset = 0;
  skipNegativeOffsets(unrolledOffsets, nextOffset);
  t.forEachRef(*robj, [&](RObjOrFakePtr& ref) {
    if (ref.isPtr()) {
      const ssize_t off = unrolledOffsets[nextOffset];
      EXPECT_EQ(&ref, mem::add(robj, off));
      EXPECT_EQ((size_t)ref.asPtr(), (off + 1) * sizeof(void*));
      ++nextOffset;
      skipNegativeOffsets(unrolledOffsets, nextOffset);
    }
  });
  EXPECT_EQ(nextOffset, unrolledOffsets.size());

  nextOffset = 0;
  t.forEachRef(*robj, [&](RObjOrFakePtr& ref) {
    const ssize_t off = unrolledOffsets[nextOffset];
    EXPECT_EQ(&ref, mem::add(robj, std::abs(off)));
    EXPECT_EQ((size_t)ref.sbits(), (off + 1) * sizeof(void*));
    ++nextOffset;
  });
  EXPECT_EQ(nextOffset, unrolledOffsets.size());
}

TEST(TypeTest, testType1) {
  // Test a simple no-ref structure.
  std::vector<ssize_t> refs = {};
  auto t = Type::classFactory("TypeTest.testType1", 42, absOffsets(refs));
  // classFactory() rounds up the byte size
  EXPECT_EQ(t->userByteSize(), (size_t)48);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(RObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::refClass);
  EXPECT_FALSE(t->hasRefs());
  verifyRefs(*t, refs);
}

TEST(TypeTest, testType2) {
  // Test a simple structure with refs.
  std::vector<ssize_t> refs = {8 * 0, 8 * 1, -8 * 5, 8 * 8};
  auto t = Type::classFactory("TypeTest.testType2", 100, absOffsets(refs));
  // classFactory() rounds up the byte size
  EXPECT_EQ(t->userByteSize(), (size_t)104);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(RObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::refClass);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs);
}

TEST(TypeTest, testType3) {
  // Test a really big structure with ref gaps big enough to skip mask entries.
  std::vector<ssize_t> refs = {8 * 0,
                               8 * 17,
                               -8 * 67,
                               -8 * 82,
                               8 * 199,
                               8 * 200,
                               8 * 201,
                               8 * 254,
                               8 * 255,
                               8 * 256,
                               8 * 257};
  auto t = Type::classFactory("TypeTest.testType3", 8 * 260, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)8 * 260);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(RObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::refClass);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs);
}

TEST(TypeTest, testType4) {
  // Test a really big structure with ref gaps big enough to skip mask entries.
  std::vector<ssize_t> refs = {2648};
  auto t = Type::classFactory("TypeTest.testType4", 6656, absOffsets(refs));
  verifyRefs(*t, refs);
}

TEST(TypeTest, testType5) {
  std::vector<ssize_t> refs = {-2648};
  auto t = Type::classFactory("TypeTest.testType5", 6620, absOffsets(refs));
  verifyRefs(*t, refs);
}

// TODO: If we add another GC mask stripe this is a reasonable test:
// TEST(TypeTest, testType6) {
//   // Test freeze references
//   std::vector<ssize_t> refs = {8*0, 8*8, -8*17, 8*67, 8*70, 8*82, 8*98};
//   std::vector<ssize_t> freezeRefs = {8*0, -8*17, 8*70, 8*98};
//   auto t = Type::classFactory("TypeTest.testType6", 8*100, absOffsets(refs));
//   // Muck with the freeze masks so we can test the freeze slots
//   SkipRefMaskType* masks = const_cast<SkipRefMaskType*>(t->refMask());
//   masks[0].freezeMask = 0x0000000000020001;
//   masks[1].freezeMask = 0x0000000400000040;
//   verifyRefs(*t, refs, freezeRefs);
// }

TEST(TypeTest, testArray1) {
  // Test an array with no pointers.
  std::vector<ssize_t> refs = {};
  auto t = Type::arrayFactory("TypeTest.testArray1", 32, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)32);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(AObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::array);
  EXPECT_FALSE(t->hasRefs());
  verifyRefs(*t, refs, 3);
}

TEST(TypeTest, testArray2) {
  // Test an array with simple pointers.
  std::vector<ssize_t> refs = {8 * 1, 8 * 2};
  auto t = Type::arrayFactory("TypeTest.testArray2", 32, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)32);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(AObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::array);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs, 4);
}

TEST(TypeTest, testArray3) {
  // Test an array of single pointers.
  std::vector<ssize_t> refs = {0};
  auto t = Type::arrayFactory("TypeTest.testArray3", 8, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)8);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(AObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::array);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs, 5);
  verifyRefs(*t, refs, 63);
  verifyRefs(*t, refs, 64);
  verifyRefs(*t, refs, 65);
  verifyRefs(*t, refs, 127);
  verifyRefs(*t, refs, 128);
  verifyRefs(*t, refs, 129);
}

TEST(TypeTest, testArray4) {
  // Test an array of single pointers.
  std::vector<ssize_t> refs = {8 * 0, 8 * 1, 8 * 2};
  auto t = Type::arrayFactory("TypeTest.testArray4", 8 * 3, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)8 * 3);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(AObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::array);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs, 5);
  verifyRefs(*t, refs, 63);
  verifyRefs(*t, refs, 64);
  verifyRefs(*t, refs, 65);
  verifyRefs(*t, refs, 127);
  verifyRefs(*t, refs, 128);
  verifyRefs(*t, refs, 129);
}

TEST(TypeTest, testArray5) {
  // Test an array of single pointers.
  std::vector<ssize_t> refs = {8 * 0, 8 * 32};
  auto t = Type::arrayFactory("TypeTest.testArray5", 8 * 33, absOffsets(refs));
  EXPECT_EQ(t->userByteSize(), (size_t)8 * 33);
  EXPECT_EQ(t->uninternedMetadataByteSize(), sizeof(AObjMetadata));
  EXPECT_EQ(t->internedMetadataByteSize(), sizeof(IObjMetadata));
  EXPECT_EQ(t->kind(), Type::Kind::array);
  EXPECT_TRUE(t->hasRefs());
  verifyRefs(*t, refs, 1);
  verifyRefs(*t, refs, 2);
  verifyRefs(*t, refs, 3);
  verifyRefs(*t, refs, 4);
  verifyRefs(*t, refs, 5);
}

namespace _testBruteForce {

bool incr(std::vector<size_t>& loc, size_t index, size_t userPtrSize) {
  ++loc[index];
  if (loc[index] == userPtrSize) {
    if (index == 0)
      return false;
    if (!incr(loc, index - 1, userPtrSize - 1))
      return false;
    loc[index] = loc[index - 1] + 1;
  }
  return true;
}

TEST(TypeTest, testBruteForce) {
  const size_t maxBits = 3;
  const size_t userPtrSize = 192;
  for (size_t bitCount = 0; bitCount <= maxBits; ++bitCount) {
    std::vector<size_t> loc(maxBits);
    for (size_t i = 0; i < maxBits; ++i)
      loc[i] = i;

    std::vector<ssize_t> offsets(maxBits);
    while (true) {
      for (size_t i = 0; i < maxBits; ++i)
        offsets[i] = loc[i] * sizeof(void*);
      auto t = Type::classFactory(
          "TypeTest.testBruteForce",
          userPtrSize * sizeof(void*),
          absOffsets(offsets));
      verifyRefs(*t, offsets);
      if (!incr(loc, maxBits - 1, userPtrSize))
        break;
    }
  }
}
} // namespace _testBruteForce

TEST(TypeTest, testTorture) {
  LameRandom random(42);

  const auto duration = std::chrono::seconds(3);

  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <= duration) {
    size_t userByteSize = random.next(8192) + 1;

    std::vector<ssize_t> refs;
    while (true) {
      size_t skip = (random.next(8192 / sizeof(void*)) + 1) * sizeof(void*);
      size_t last = std::abs(refs.empty() ? 0 : refs.back());
      if (last + skip + sizeof(void*) > userByteSize)
        break;
      bool fake = random.next(2) != 0;
      refs.push_back((last + skip) * (fake ? -1 : 1));
    }

    bool array = random.next(2) != 0;
    if (array) {
      auto t = Type::arrayFactory(
          "TypeTest.testTorture(array)",
          roundUp(userByteSize, sizeof(void*)),
          absOffsets(refs));
      verifyRefs(*t, refs, random.next(50));
    } else {
      auto t = Type::classFactory(
          "TypeTest.testTorture(class)", userByteSize, absOffsets(refs));
      verifyRefs(*t, refs);
    }
  }
}
} // anonymous namespace
