/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This test only works in non-NDEBUG where the Obstack DEBUG entrypoints are
// defined.
#ifndef NDEBUG

#include "skip/Obstack.h"
#include "../../../runtime/native/src/ObstackDetail.h"
#include "skip/Obstack-extc.h"
#include "skip/Finalized.h"
#include "skip/Process.h"
#include "skip/Refcount.h"

#include "testutil.h"

#include <gtest/gtest.h>

#include <vector>
#include <chrono>
#include <algorithm>

using namespace skip;
using namespace skip::test;

namespace {

// allocating a large object or iObjRef unconditionally allocates a small
// placeholder on the obstack.
constexpr size_t PLACEHOLDER_SIZE = Obstack::kAllocAlign;
} // namespace

#if !defined(NDEBUG)
#define DEBUG_EXPECT_EQ(a, b) EXPECT_EQ(a, b)
#define DEBUG_EXPECT_TRUE(a) EXPECT_TRUE(a)
#endif

#if OBSTACK_WP_FROZEN
#define EXPECT_FROZEN(addr) EXPECT_NOT_WRITABLE(addr)
#else
#define EXPECT_FROZEN(addr) \
  do {                      \
  } while (false)
#endif
#define EXPECT_NOT_FROZEN(addr) EXPECT_WRITABLE(addr)

TEST(ObstackTest, testSimple) {
  Obstack stack;
  LameRandom random(42);

  // Try some simple allocations
  auto pos = stack.note();

  auto p = stack.alloc(512);
  random.fill(p, 512);

  stack.collect(pos);
}

template <size_t USER_BYTE_SIZE, typename Base = RObj>
struct SimpleObject : Base {
  uint8_t m_data[USER_BYTE_SIZE];

  DEFINE_VTABLE()

  static Type& static_type() {
    static auto singleton =
        Type::classFactory(typeid(SimpleObject).name(), sizeof(m_data), {});
    return *singleton;
  }

  explicit SimpleObject(uint8_t data) {
    memset(m_data, data, USER_BYTE_SIZE);
  }
};

TEST(ObstackTest, testDecRef) {
  Obstack stack;

  auto pos1 = stack.note();
  auto p0 = stack.allocObject<SimpleObject<128>>(0x01);

  auto i0 = stack.intern(p0);
  EXPECT_REFCOUNT(i0, 1U);

  incref(i0.asPtr());
  EXPECT_REFCOUNT(i0, 2U);

  auto pos2 = stack.note();
  stack.allocObject<SimpleObject<128>>(0x01);

  stack.collect(pos2);
  EXPECT_REFCOUNT(i0, 2U);
  stack.collect(pos1);
  EXPECT_REFCOUNT(i0, 1U);
  decref(i0.asPtr());
}

TEST(ObstackTest, testMultiChunk) {
  Obstack stack;

  DEBUG_EXPECT_EQ(1U, stack.DEBUG_allocatedChunks());

  // Do enough small allocations to fault into a second chunk
  std::vector<SkipObstackPos> posStack;

  const size_t totalAlloc = Obstack::kChunkSize * 1024;
  const size_t allocSize = 600 * Obstack::kChunkSize / 4096;

  size_t sum = 0;
  for (size_t index = 0; sum < totalAlloc; ++index) {
    if (index % 500)
      posStack.push_back(stack.note());
    stack.alloc(allocSize);
    sum += allocSize;
  }

  DEBUG_EXPECT_EQ(1166U, stack.DEBUG_allocatedChunks());

  while (!posStack.empty()) {
    stack.collect(posStack.back());
    posStack.pop_back();
  }

  DEBUG_EXPECT_EQ(1U, stack.DEBUG_allocatedChunks());
}

TEST(ObstackTest, testLargeAlloc) {
  Obstack stack;
  constexpr size_t largeAllocSize = 8 * 1024 * 1024;

  DEBUG_EXPECT_EQ(0U, stack.DEBUG_getLargeAllocTotal());

  // An allocation too big to fit in a single chunk.
  {
    auto pos = stack.note();
    stack.alloc(largeAllocSize);
    DEBUG_EXPECT_EQ(largeAllocSize, stack.DEBUG_getLargeAllocTotal());

    stack.collect(pos);
    DEBUG_EXPECT_EQ(0U, stack.DEBUG_getLargeAllocTotal());
  }

  {
    stack.alloc(largeAllocSize);
    DEBUG_EXPECT_EQ(largeAllocSize, stack.DEBUG_getLargeAllocTotal());
    auto pos = stack.note();

    stack.collect(pos);
    DEBUG_EXPECT_EQ(largeAllocSize, stack.DEBUG_getLargeAllocTotal());
  }
}

TEST(ObstackTest, testSplitClear) {
  Obstack stack;
  constexpr size_t largeAllocSize = 8 * 1024 * 1024;

  DEBUG_EXPECT_EQ(0U, stack.DEBUG_getLargeAllocTotal());

  // Check that notes and large objects clear in the right
  // order and count.

  auto pos1 = stack.note();
  stack.alloc(largeAllocSize);
  DEBUG_EXPECT_EQ(largeAllocSize, stack.DEBUG_getLargeAllocTotal());

  auto pos2 = stack.note();
  stack.alloc(largeAllocSize);
  DEBUG_EXPECT_EQ(2 * largeAllocSize, stack.DEBUG_getLargeAllocTotal());

  stack.collect(pos2);
  DEBUG_EXPECT_EQ(largeAllocSize, stack.DEBUG_getLargeAllocTotal());

  stack.collect(pos1);
  DEBUG_EXPECT_EQ(0U, stack.DEBUG_getLargeAllocTotal());
}

TEST(ObstackTest, testTorture) {
  const size_t maxTotalAllocation = 1ULL << 32; // 4GB
  const size_t maxSingleAllocation = 1ULL << 13; // 8K
  const size_t maxNotes = 1000;
  const auto duration = std::chrono::seconds(10);

  Obstack stack;
  LameRandom random(42);
  size_t currentAllocation = 0;
  size_t currentNotes = 0;
  std::vector<std::tuple<SkipObstackPos, size_t, size_t>> posStack;

  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start <= duration) {
    stack.verifyInvariants();

    int op = random.next(10);
    switch (op) {
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7: {
        // alloc
        if (currentAllocation > maxTotalAllocation)
          continue;
        // Bias the allocations toward the smaller end
        size_t sqrtSz =
            random.next((uint64_t)ceil(std::sqrt(maxSingleAllocation + 1)));
        size_t sz = std::min(sqrtSz * sqrtSz, maxSingleAllocation);
        currentAllocation += sz;
        stack.alloc(sz);
      } break;

      case 8: {
        if (posStack.size() >= maxNotes)
          continue;
        posStack.push_back(
            std::make_tuple(stack.note(), currentAllocation, currentNotes));
      } break;

      case 9: {
        // clear
        if (posStack.empty())
          continue;

        if (posStack.size() > 1) {
          // skip up to 9 positions
          ssize_t skip = random.next(std::min<size_t>(posStack.size() - 1, 10));
          while (skip-- > 0)
            posStack.pop_back();
        }

        stack.collect(std::get<0>(posStack.back()));
        currentAllocation = std::get<1>(posStack.back());
        currentNotes = std::get<2>(posStack.back());
        posStack.pop_back();
      } break;
    }
  }
}

TEST(ObstackTest, testC_obstack) {
  auto pos = SKIP_Obstack_note_inl();
  void* p = SKIP_Obstack_alloc(12);
  memset(p, 0, 12);
  void* q = SKIP_Obstack_calloc(12);
  EXPECT_EQ(0, memcmp(p, q, 12));

  auto p2 = Obstack::cur().allocObject<SimpleObject<128>>(0x02);
  auto p3 = (RObj*)SKIP_Obstack_shallowClone(p2);
  EXPECT_NE(p2, p3);
  EXPECT_EQ(p2->vtable(), p3->vtable());
  EXPECT_EQ(0, memcmp(p2, p3, 128));

  auto p5 = Obstack::cur().intern(p2).asPtr();
  auto p6 = SKIP_Obstack_shallowClone(p5);
  EXPECT_NE(p6, p5);
  EXPECT_EQ(Arena::getMemoryKind((RObj*)p5), Arena::Kind::iobj);
  EXPECT_EQ(Arena::getMemoryKind((RObj*)p6), Arena::Kind::obstack);

  SKIP_Obstack_collect0(pos);
}

namespace {

uint64_t pattern(uint8_t data) {
  return mungeBits(data);
}

struct SimpleObj : RObj {
  uint64_t m_a;

  explicit SimpleObj(uint8_t a) : m_a(pattern(a)) {}

  void verify(Obstack& stack, uint8_t a) const {
    EXPECT_EQ(pattern(a), m_a);
    DEBUG_EXPECT_TRUE(stack.DEBUG_isAlive(this));
  }

  DEFINE_VTABLE()

  static Type& static_type() {
    static auto singleton =
        Type::classFactory(typeid(SimpleObj).name(), sizeof(SimpleObj), {});
    return *singleton;
  }
};

constexpr size_t SIMPLEOBJ_SIZE = sizeof(VTable*) + sizeof(SimpleObj);

struct SimpleLargeObj : RObj {
  std::array<char, Obstack::kChunkSize * 3 / 2> m_a;

  explicit SimpleLargeObj(uint8_t a) {
    LameRandom rand(a);
    rand.fill(m_a.data(), m_a.size());
  }

  void verify(uint8_t a) const {
    LameRandom rand(a);
    EXPECT_TRUE(rand.compareFill(m_a.data(), m_a.size()));
  }

  DEFINE_VTABLE()

  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(SimpleLargeObj).name(), sizeof(SimpleLargeObj), {});
    return *singleton;
  }
};

constexpr size_t SIMPLELARGEOBJ_SIZE = sizeof(VTable*) + sizeof(SimpleLargeObj);

template <typename T0, typename T1, size_t n = 8>
struct PtrObj : RObj {
  std::array<char, n> m_a;
  const T0* m_p0;
  std::array<char, n> m_b;
  const T1* m_p1;
  std::array<char, n> m_c;
  const PtrObj* m_p2;
  std::array<char, n> m_d;

  explicit PtrObj(uint8_t a) : m_p0(nullptr), m_p1(nullptr), m_p2(nullptr) {
    LameRandom rand(a);
    rand.fill(m_a.data(), m_a.size());
    rand.fill(m_b.data(), m_b.size());
    rand.fill(m_c.data(), m_c.size());
    rand.fill(m_d.data(), m_d.size());
  }

  void verify(Obstack& stack, uint8_t a) const {
    LameRandom rand(a);
    EXPECT_TRUE(rand.compareFill(m_a.data(), m_a.size()));
    EXPECT_TRUE(rand.compareFill(m_b.data(), m_b.size()));
    EXPECT_TRUE(rand.compareFill(m_c.data(), m_c.size()));
    EXPECT_TRUE(rand.compareFill(m_d.data(), m_d.size()));
    DEBUG_EXPECT_TRUE(stack.DEBUG_isAlive(this));
  }

  DEFINE_VTABLE()

  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(PtrObj).name(),
        sizeof(PtrObj),
        {offsetof(PtrObj, m_p0), offsetof(PtrObj, m_p1)});
    return *singleton;
  }
};

struct CyclicObj : RObj {
  uint64_t m_a;
  const CyclicObj* m_p0;
  uint64_t m_b;
  const CyclicObj* m_p1;
  uint64_t m_c;

  explicit CyclicObj(uint8_t a)
      : m_a(pattern(a)),
        m_p0(nullptr),
        m_b(pattern(a + 16)),
        m_p1(nullptr),
        m_c(pattern(a + 32)) {}

  void verify(uint8_t a) const {
    EXPECT_EQ(pattern(a), m_a);
    EXPECT_EQ(pattern(a + 16), m_b);
    EXPECT_EQ(pattern(a + 32), m_c);
  }

  DEFINE_VTABLE()

  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(CyclicObj).name(),
        sizeof(CyclicObj),
        {
            offsetof(CyclicObj, m_p0),
            offsetof(CyclicObj, m_p1),
        });
    return *singleton;
  }
};

constexpr size_t CYCLICOBJ_SIZE = sizeof(VTable*) + sizeof(CyclicObj);

constexpr size_t FILL_SIZE = 24;

void allocFill(Obstack& stack) {
  void* p = stack.alloc(FILL_SIZE);
  memset(p, 0xFF, FILL_SIZE);
}

TEST(ObstackTest, testClearAndCollectSimple) {
  // Simplest GC test - no pointers, no actual movement
  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto pos = stack.note();
  auto pOrig = stack.allocObject<SimpleObj>(1);
  auto pCopy = pOrig;
  allocFill(stack);
  DEBUG_EXPECT_EQ(
      512U
          // pos
          + 1 * SIMPLEOBJ_SIZE // pOrig
          + 1 * FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&pCopy, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  // Since the allocated objects fit in a single chunk and we're only
  // collecting one then we should end up with the same layout.
  EXPECT_EQ(pOrig, pCopy);
  pCopy->verify(stack, 1);
  DEBUG_EXPECT_EQ(512U + SIMPLEOBJ_SIZE, stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testClearAndCollectSimpleLarge) {
  // Simplest GC test with large object - no pointers, no actual movement
  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto pos = stack.note();
  auto pOrig = stack.allocObject<SimpleLargeObj>(1);
  auto pCopy = pOrig;
  allocFill(stack);
  DEBUG_EXPECT_EQ(
      512U
          // pos
          + PLACEHOLDER_SIZE // pOrig
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(SIMPLELARGEOBJ_SIZE, stack.DEBUG_getLargeAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&pCopy, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  EXPECT_EQ(pOrig, pCopy);
  pOrig->verify(1);
  DEBUG_EXPECT_EQ(512U + PLACEHOLDER_SIZE, stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(SIMPLELARGEOBJ_SIZE, stack.DEBUG_getLargeAllocTotal());
}

TEST(ObstackTest, testClearAndCollectMove) {
  // Minor GC test - no pointers, 1 moved object
  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto pos = stack.note();
  allocFill(stack);
  auto pOrig = stack.allocObject<SimpleObj>(1);
  auto pCopy = pOrig;
  DEBUG_EXPECT_EQ(
      512U
          // pos
          + FILL_SIZE + SIMPLEOBJ_SIZE // pOrig
      ,
      stack.DEBUG_getSmallAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&pCopy, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  // This time we should have actually moved the collected object.
  EXPECT_NE(pOrig, pCopy);
  pCopy->verify(stack, 1);
  DEBUG_EXPECT_EQ(512U + SIMPLEOBJ_SIZE, stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testClearAndCollectSimpleRefs) {
  // GC test - one object with simple pointers
  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  using SimplePtrObj = PtrObj<SimpleObj, SimpleObj>;

  auto pos = stack.note();
  allocFill(stack);
  auto obj = stack.allocObject<SimplePtrObj>(1);
  allocFill(stack);
  obj->m_p0 = stack.allocObject<SimpleObj>(2);
  allocFill(stack);
  obj->m_p1 = stack.allocObject<SimpleObj>(3);
  allocFill(stack);
  DEBUG_EXPECT_EQ(
      512U
          // pos
          + FILL_SIZE + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj
          + FILL_SIZE + SIMPLEOBJ_SIZE // obj->m_p0
          + FILL_SIZE + SIMPLEOBJ_SIZE // obj->m_p1
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&obj, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  // This time we should have actually moved the collected object.
  obj->verify(stack, 1);
  obj->m_p0->verify(stack, 2);
  obj->m_p1->verify(stack, 3);
  DEBUG_EXPECT_EQ(
      512U + (sizeof(VTable*) + sizeof(*obj)) + 2 * SIMPLEOBJ_SIZE,
      stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testClearAndCollectCycle) {
  // GC test - test a cycle

  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto pos = stack.note();
  allocFill(stack);
  auto obj1 = stack.allocObject<CyclicObj>(1);
  allocFill(stack);
  auto obj2 = stack.allocObject<CyclicObj>(2);
  allocFill(stack);
  auto obj3 = stack.allocObject<CyclicObj>(3);
  allocFill(stack);
  auto obj4 = stack.allocObject<CyclicObj>(4);
  allocFill(stack);

  obj1->m_p0 = obj2;
  obj1->m_p1 = obj4;
  obj2->m_p1 = obj4;
  obj4->m_p1 = obj1;

  DEBUG_EXPECT_EQ(
      512U
          // pos
          + FILL_SIZE + CYCLICOBJ_SIZE // obj1
          + FILL_SIZE + CYCLICOBJ_SIZE // obj2
          + FILL_SIZE + CYCLICOBJ_SIZE // obj3
          + FILL_SIZE + CYCLICOBJ_SIZE // obj4
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&obj1, 1);
  // NOTE: obj2, obj3 and obj4 are now invalid!
  obj2 = obj3 = obj4 = nullptr;

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  obj1->verify(1);
  obj1->m_p0->verify(2);
  obj1->m_p1->verify(4);
  obj1->m_p0->m_p1->verify(4);
  EXPECT_EQ(obj1, obj1->m_p1->m_p1);
  EXPECT_EQ(obj1->m_p0->m_p1, obj1->m_p1);
  EXPECT_EQ(obj1, obj1->m_p1->m_p1);

  DEBUG_EXPECT_EQ(512U + 3 * CYCLICOBJ_SIZE, stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testClearAndCollectMixed) {
  // GC test - test mixing older generations with new

  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  allocFill(stack);
  auto obj1 = stack.allocObject<CyclicObj>(1);
  allocFill(stack);
  auto pos = stack.note();
  allocFill(stack);
  auto obj2 = stack.allocObject<CyclicObj>(2);
  allocFill(stack);
  auto obj3 = stack.allocObject<CyclicObj>(3);
  allocFill(stack);

  obj2->m_p0 = obj1;
  obj2->m_p1 = obj3;
  obj3->m_p0 = obj2;
  obj3->m_p1 = obj2;

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + CYCLICOBJ_SIZE // obj1
          + FILL_SIZE
          // pos
          + FILL_SIZE + CYCLICOBJ_SIZE // obj2
          + FILL_SIZE + CYCLICOBJ_SIZE // obj3
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());

  std::array<CyclicObj*, 3> objs{{obj1, obj2, obj3}};
  stack.collect(pos, (RObjOrFakePtr*)objs.data(), objs.size());

  EXPECT_EQ(obj1, objs[0]);
  EXPECT_NE(obj2, objs[1]);
  EXPECT_NE(obj3, objs[2]);

  obj2 = objs[1];
  obj3 = objs[2];

  obj1->verify(1);
  obj2->verify(2);
  obj3->verify(3);

  EXPECT_EQ(obj1->m_p0, nullptr);
  EXPECT_EQ(obj1->m_p1, nullptr);
  EXPECT_EQ(obj2->m_p0, obj1);
  EXPECT_EQ(obj2->m_p1, obj3);
  EXPECT_EQ(obj3->m_p0, obj2);
  EXPECT_EQ(obj3->m_p1, obj2);

  DEBUG_EXPECT_EQ(
      512U + 3 * CYCLICOBJ_SIZE + 2 * FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testClearAndCollectPinned) {
  Obstack stack;
  LameRandom random(314);

  using ObjPtr = PtrObj<RObj, RObj, 24>;

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto obj1 = stack.allocObject<ObjPtr>(1);
  auto obj2 = stack.allocObject<ObjPtr>(2);
  auto pos = stack.note();
  allocFill(stack);
  auto obj3 = stack.allocObject<ObjPtr>(3);
  allocFill(stack);
  auto obj4 = stack.allocObject<ObjPtr>(4);

  obj1->m_p0 = obj2;
  obj1->m_p1 = obj3;
  obj2->m_p0 = obj1;
  obj3->m_p0 = obj4;
  obj4->m_p0 = obj1;
  obj4->m_p1 = obj3;

  std::array<ObjPtr*, 1> objs{{obj2}};
  stack.collect(pos, (RObjOrFakePtr*)objs.data(), objs.size());
  // Careful! obj1, obj3, obj4 are now not valid!
  obj1 = (ObjPtr*)(obj2->m_p0);
  obj3 = (ObjPtr*)(obj1->m_p1);
  obj4 = (ObjPtr*)(obj3->m_p0);

  obj1->verify(stack, 1);
  obj2->verify(stack, 2);
  obj3->verify(stack, 3);
  obj4->verify(stack, 4);

  EXPECT_EQ(obj1->m_p0, obj2);
  EXPECT_EQ(obj1->m_p1, obj3);
  EXPECT_EQ(obj2->m_p0, obj1);
  EXPECT_EQ(obj3->m_p0, obj4);
  EXPECT_EQ(obj4->m_p0, obj1);
  EXPECT_EQ(obj4->m_p1, obj3);
}

TEST(ObstackTest, testClearAndCollectPinnedLarge) {
  Obstack stack;
  LameRandom random(314);

  using LargePtrObj = PtrObj<RObj, RObj, Obstack::kChunkSize * 2 / 3>;
  using SmallPtrObj = PtrObj<RObj, RObj, 24>;

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto obj1 = stack.allocObject<LargePtrObj>(1);
  auto obj2 = stack.allocObject<LargePtrObj>(2);
  auto pos = stack.note();
  allocFill(stack);
  auto obj3 = stack.allocObject<LargePtrObj>(3);
  allocFill(stack);
  auto obj4 = stack.allocObject<SmallPtrObj>(4);

  obj1->m_p0 = obj2;
  obj1->m_p1 = obj3;
  obj2->m_p0 = obj1;
  obj3->m_p0 = obj4;
  obj4->m_p0 = obj1;
  obj4->m_p1 = obj3;

  std::array<LargePtrObj*, 1> objs{{obj2}};
  stack.collect(pos, (RObjOrFakePtr*)objs.data(), objs.size());
  // Careful! obj1, obj3, obj4 are now not valid!
  obj1 = (LargePtrObj*)obj2->m_p0;
  obj3 = (LargePtrObj*)obj1->m_p1;
  obj4 = (SmallPtrObj*)obj3->m_p0;

  obj1->verify(stack, 1);
  obj2->verify(stack, 2);
  obj3->verify(stack, 3);
  obj4->verify(stack, 4);

  EXPECT_EQ(obj1->m_p0, obj2);
  EXPECT_EQ(obj1->m_p1, obj3);
  EXPECT_EQ(obj2->m_p0, obj1);
  EXPECT_EQ(obj3->m_p0, obj4);
  EXPECT_EQ(obj4->m_p0, obj1);
  EXPECT_EQ(obj4->m_p1, obj3);
}

TEST(ObstackTest, testClearAndCollectLarge) {
  // GC test - test that large objects are kept properly and also are
  // updated with new pointers.

  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  using SmallPtrObj = PtrObj<SimpleObj, SimpleObj, 8>;
  using LargePtrObj =
      PtrObj<SmallPtrObj, SimpleObj, Obstack::kChunkSize * 2 / 3>;
  using SmallPtrObj2 = PtrObj<LargePtrObj, SimpleObj, 8>;

  allocFill(stack);
  auto obj1 ATTR_UNUSED = stack.allocObject<SimpleObj>(1);
  auto obj2 = stack.allocObject<SmallPtrObj>(2);
  auto obj3 = stack.allocObject<LargePtrObj>(3);
  obj3->m_p0 = obj2;
  allocFill(stack);

  auto note = stack.note();

  auto obj4 ATTR_UNUSED = stack.allocObject<SimpleObj>(4);
  auto obj5 = stack.allocObject<SmallPtrObj2>(5);
  obj5->m_p0 = obj3;
  allocFill(stack);
  auto obj6 = stack.allocObject<LargePtrObj>(6);
  obj6->m_p0 = obj2;
  obj6->m_p2 = obj3;
  allocFill(stack);
  auto obj7 ATTR_UNUSED = stack.allocObject<LargePtrObj>(7);
  allocFill(stack);

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + SIMPLEOBJ_SIZE // obj1 dead
          + (sizeof(VTable*) + sizeof(SmallPtrObj)) // obj2
          + PLACEHOLDER_SIZE // obj3
          + FILL_SIZE
          // note points here
          + SIMPLEOBJ_SIZE // obj4 dead
          + (sizeof(VTable*) + sizeof(SmallPtrObj2)) // obj5
          + FILL_SIZE + PLACEHOLDER_SIZE // obj6
          + FILL_SIZE + PLACEHOLDER_SIZE // obj7 dead
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(
      3 * (sizeof(VTable*) + sizeof(LargePtrObj)),
      stack.DEBUG_getLargeAllocTotal());

  std::array<RObj*, 2> objs{{obj5, obj6}};
  stack.collect(note, (RObjOrFakePtr*)objs.data(), objs.size());
  EXPECT_NE(obj5, objs[0]);
  EXPECT_EQ(obj6, objs[1]); // Large object should NOT have moved!
  obj4 = nullptr;
  obj5 = (SmallPtrObj2*)objs[0];
  obj6 = (LargePtrObj*)objs[1];
  obj7 = nullptr;

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + SIMPLEOBJ_SIZE // obj1 dead
          + (sizeof(VTable*) + sizeof(SmallPtrObj)) // obj2
          + PLACEHOLDER_SIZE // obj3
          + FILL_SIZE
          // note points here
          + (sizeof(VTable*) + sizeof(SmallPtrObj2)) // obj5
      // no placeholder for obj6 because we had small surviors
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(
      2 * (sizeof(VTable*) + sizeof(LargePtrObj)),
      stack.DEBUG_getLargeAllocTotal());

  obj1->verify(stack, 1);
  obj2->verify(stack, 2);
  obj3->verify(stack, 3);
  obj5->verify(stack, 5);
  obj6->verify(stack, 6);
}

TEST(ObstackTest, testClearAndCollectIObj) {
  // GC test - test that iobjs are properly decremented

  Obstack stack;

  using SimplePtrObj = PtrObj<SimpleObj, IObj>;
  using IObjPtrObj = PtrObj<IObj, IObj>;

  allocFill(stack);
  auto pos0 = stack.note();
  auto obj1 = stack.allocObject<SimpleObj>(1);
  auto iobj1 = stack.intern(obj1);
  auto pos1 = stack.note();
  auto pos2 = stack.note();

  auto obj2 = stack.allocObject<SimplePtrObj>(2);
  obj2->m_p0 = obj1;
  auto iobj2 = stack.intern(obj2);

  auto obj2a = stack.allocObject<SimplePtrObj>(2);
  obj2a->m_p0 = (SimpleObj*)iobj1.asPtr();
  auto iobj2a = stack.intern(obj2a);

  auto obj3 = stack.allocObject<SimplePtrObj>(3);
  obj3->m_p1 = iobj1.asPtr();
  auto iobj3 = stack.intern(obj3);

  auto obj4 = stack.allocObject<IObjPtrObj>(4);
  obj4->m_p0 = iobj3.asPtr();
  obj4->m_p1 = iobj3.asPtr();

  EXPECT_EQ(iobj2, iobj2a);
  incref(iobj1.asPtr());
  incref(iobj2.asPtr());
  incref(iobj3.asPtr());
  EXPECT_REFCOUNT(iobj1, 4U);
  EXPECT_REFCOUNT(iobj2, 2U);
  EXPECT_REFCOUNT(iobj3, 2U);

  DEBUG_EXPECT_EQ(
      0 +
          FILL_SIZE
          // pos0 here
          + SIMPLEOBJ_SIZE // obj1
          + PLACEHOLDER_SIZE // iobj1
          // pos1 here
          // pos2 here
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj2
          + PLACEHOLDER_SIZE // iobj2
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj2a
          //+ PLACEHOLDER_SIZE // iobj2a no new iObjRef
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj3
          + PLACEHOLDER_SIZE // iobj3
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj4
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(3U, stack.DEBUG_getIobjCount());

  stack.collect(pos2, (RObjOrFakePtr*)&obj4, 1);

  DEBUG_EXPECT_EQ(
      0 +
          FILL_SIZE
          // pos0 here
          + SIMPLEOBJ_SIZE // obj1
          + PLACEHOLDER_SIZE // iobj1
          // pos1 here
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj4
      // no placeholder after collect with small survivors
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(2U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 4U);
  EXPECT_REFCOUNT(iobj2, 1U);
  EXPECT_REFCOUNT(iobj3, 2U);

  stack.collect(pos1, (RObjOrFakePtr*)&obj4, 1);

  DEBUG_EXPECT_EQ(
      0 +
          FILL_SIZE
          // pos0 here
          + SIMPLEOBJ_SIZE // obj1
          + PLACEHOLDER_SIZE // iobj1
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj4
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(2U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 4U);
  EXPECT_REFCOUNT(iobj2, 1U);
  EXPECT_REFCOUNT(iobj3, 2U);

  stack.collect(pos0, (RObjOrFakePtr*)&iobj1, 1);

  DEBUG_EXPECT_EQ(
      0 + FILL_SIZE + PLACEHOLDER_SIZE // iobj survivors, but small survivors
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 4U);
  EXPECT_REFCOUNT(iobj2, 1U);
  EXPECT_REFCOUNT(iobj3, 1U);

  decref(iobj1.asPtr());
  decref(iobj2.asPtr());
  decref(iobj3.asPtr());
}

TEST(ObstackTest, testClearAndCollectIObj2) {
  // GC test - test that iobjs are properly refcounted when discovered.

  Obstack stack;

  using IObjPtrObj = PtrObj<IObj, IObj>;

  allocFill(stack);
  auto pos0 = stack.note();
  auto pos1 = stack.note();
  auto obj1 = stack.allocObject<SimpleObj>(1);
  auto iobj1 = stack.intern(obj1);

  auto obj2 = stack.allocObject<IObjPtrObj>(2);
  obj2->m_p0 = iobj1.asPtr();
  auto iobj2 = stack.intern(obj2);

  auto obj3 = stack.allocObject<IObjPtrObj>(3);
  obj3->m_p0 = iobj2.asPtr();
  auto iobj3 = stack.intern(obj3);

  incref(iobj1.asPtr());
  incref(iobj2.asPtr());
  incref(iobj3.asPtr());

  DEBUG_EXPECT_EQ(
      0 +
          FILL_SIZE
          // pos0
          // pos1
          + SIMPLEOBJ_SIZE // obj1
          + PLACEHOLDER_SIZE // iobj1
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj2
          + PLACEHOLDER_SIZE // iobj2
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj3
          + PLACEHOLDER_SIZE // iobj3
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(3U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 3U);
  EXPECT_REFCOUNT(iobj2, 3U);
  EXPECT_REFCOUNT(iobj3, 2U);

  stack.collect(pos1, (RObjOrFakePtr*)&iobj3, 1);

  DEBUG_EXPECT_EQ(
      FILL_SIZE
          // pos0
          + PLACEHOLDER_SIZE // no small survivors
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 2U);
  EXPECT_REFCOUNT(iobj2, 2U);
  EXPECT_REFCOUNT(iobj3, 2U);

  auto obj4 = stack.allocObject<IObjPtrObj>(4);
  obj4->m_p0 = ((IObjPtrObj*)iobj3.asPtr())->m_p0;

  stack.collect(pos0, (RObjOrFakePtr*)&obj4, 1);

  DEBUG_EXPECT_EQ(
      FILL_SIZE + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj4
      // no placeholder because obj4 survived
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 2U);
  EXPECT_REFCOUNT(iobj2, 3U);
  EXPECT_REFCOUNT(iobj3, 1U);
}

TEST(ObstackTest, testClearAndCollectIObj3) {
  Obstack stack;
  using SimplePtrObj = PtrObj<SimpleObj, IObj>;
  auto obj1 = stack.allocObject<SimplePtrObj>(1);
  auto iobj1 = stack.intern(obj1);
  incref(iobj1.asPtr());

  auto pos0 = stack.note();
  auto pos1 = stack.note();

  auto obj2 = stack.allocObject<SimplePtrObj>(2);
  obj2->m_p1 = iobj1.asPtr();

  DEBUG_EXPECT_EQ(
      0 + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj1
          + PLACEHOLDER_SIZE // iobj1
          // pos0
          // pos1
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj2
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 2U);

  stack.collect(pos1, (RObjOrFakePtr*)&obj2, 1);

  DEBUG_EXPECT_EQ(
      0 + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj1
          + PLACEHOLDER_SIZE // iobj1
          // pos0
          + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj2
      // no placeholder because obj2 survived
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 2U);

  stack.collect(pos0, nullptr, 0);

  DEBUG_EXPECT_EQ(
      0 + (sizeof(VTable*) + sizeof(SimplePtrObj)) // obj1
          + PLACEHOLDER_SIZE // iobj1
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_REFCOUNT(iobj1, 2U);

  decref(iobj1.asPtr());
}

TEST(ObstackTest, testClearAndCollectIObjCycle) {
  Obstack stack;
  allocFill(stack);

  using IObjPtrObj = PtrObj<RObj, RObj>;

  auto pos0 = stack.note();
  auto pos1 = stack.note();

  auto obj1 = stack.allocObject<IObjPtrObj>(1);
  auto obj2 = stack.allocObject<IObjPtrObj>(2);
  obj1->m_p0 = obj2;
  obj2->m_p0 = obj1;
  auto iobj1 = stack.intern(obj1);
  auto iobj2 = stack.intern(obj2);

  auto& iobjD = iobj1->refcountDelegate();

  incref(iobj1.asPtr());
  incref(iobj2.asPtr());
  incref(&iobjD);

  auto pos2 = stack.note();

  auto obj3 = stack.allocObject<IObjPtrObj>(3);
  obj3->m_p0 = iobj1.asPtr();
  obj3->m_p1 = iobj2.asPtr();

  DEBUG_EXPECT_EQ(
      FILL_SIZE
          // pos0
          // pos1
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj1
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj2
          + PLACEHOLDER_SIZE // iobjD
          // pos2
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj3
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_TRUE(iobj1->isCycleMember());
  EXPECT_TRUE(iobj2->isCycleMember());
  EXPECT_EQ(4U, iobjD.currentRefcount());

  stack.collect(pos2, (RObjOrFakePtr*)&obj3, 1);

  DEBUG_EXPECT_EQ(
      FILL_SIZE
          // pos0
          // pos1
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj1
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj2
          + PLACEHOLDER_SIZE // iobjD
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj3
      // no placeholder because obj3 survived
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_TRUE(iobj1->isCycleMember());
  EXPECT_TRUE(iobj2->isCycleMember());
  EXPECT_EQ(4U, iobjD.currentRefcount());

  stack.collect(pos1, (RObjOrFakePtr*)&obj3, 1);

  DEBUG_EXPECT_EQ(
      FILL_SIZE
          // pos0
          + (sizeof(VTable*) + sizeof(IObjPtrObj)) // obj3
      // no placeholder because obj3 surived
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(1U, stack.DEBUG_getIobjCount());
  EXPECT_TRUE(iobj1->isCycleMember());
  EXPECT_TRUE(iobj2->isCycleMember());
  EXPECT_EQ(4U, iobjD.currentRefcount());

  stack.collect(pos0, nullptr, 0);

  DEBUG_EXPECT_EQ(
      FILL_SIZE
      // no survivors, but cycle remains
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(0U, stack.DEBUG_getIobjCount());
  EXPECT_TRUE(iobj1->isCycleMember());
  EXPECT_TRUE(iobj2->isCycleMember());
  EXPECT_EQ(3U, iobjD.currentRefcount());

  decref(&iobjD);
  decref(iobj2.asPtr());
  decref(iobj1.asPtr());
}

TEST(ObstackTest, testCollectSimple) {
  // Simplest GC test - no pointers, no actual movement
  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  // double note will not force an allocation in the obstack
  auto pos0 = stack.note();
  auto pos = stack.note();
  EXPECT_EQ(pos0.ptr, pos.ptr);
  auto pOrig = stack.allocObject<SimpleObj>(1);
  auto pCopy = pOrig;
  allocFill(stack);
  DEBUG_EXPECT_EQ(
      512U
          // pos0, pos
          + 1 * SIMPLEOBJ_SIZE // pOrig
          + 1 * FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());

  stack.collect(pos, (RObjOrFakePtr*)&pCopy, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  // Since the allocated objects fit in a single chunk and we're only collecting
  // one then we should end up with the same layout in the same position.
  EXPECT_EQ(pOrig, pCopy);
  pCopy->verify(stack, 1);
  DEBUG_EXPECT_EQ(
      512U
          // pos0, pos
          + 1 * SIMPLEOBJ_SIZE // pCopy
      ,
      stack.DEBUG_getSmallAllocTotal());

  // --- collect again

  stack.collect(pos, (RObjOrFakePtr*)&pCopy, 1);

  random = LameRandom(314);
  EXPECT_TRUE(random.compareFill(p, 512));

  // Since the allocated objects fit in a single chunk and we're only collecting
  // one then we should end up with the same layout in the same position.
  EXPECT_EQ(pOrig, pCopy);
  pCopy->verify(stack, 1);
  DEBUG_EXPECT_EQ(
      512U
          // pos0, pos
          + 1 * SIMPLEOBJ_SIZE // pCopy
      ,
      stack.DEBUG_getSmallAllocTotal());
}

TEST(ObstackTest, testResetNote) {
  // This tickles a bug where in VERIFY_POS mode we weren't preserving the note
  // back-pointer.
  Obstack stack;
  auto note1 = stack.note();
  auto note2 = stack.note();
  (void)stack.alloc(32);
  void* roots[2] = {(void*)-1, (void*)-1};
  // -1 will be treated as a fake pointer - but by having two we defeat the
  // -ability to use a "quick collect".
  stack.collect(note2, (RObjOrFakePtr*)roots, 2);
  (void)stack.alloc(32);
  stack.collect(note2);
  stack.collect(note1);
}

TEST(ObstackTest, testCollectLarge) {
  // GC test - test that large objects are kept properly and also are
  // updated with new pointers.

  Obstack stack;
  LameRandom random(314);

  void* p = stack.alloc(512);
  random.fill(p, 512);

  using SmallPtrObj = PtrObj<SimpleObj, SimpleObj, 8>;
  using LargePtrObj =
      PtrObj<SmallPtrObj, SimpleObj, Obstack::kChunkSize * 2 / 3>;
  using SmallPtrObj2 = PtrObj<LargePtrObj, SimpleObj, 8>;

  allocFill(stack);
  auto obj1 ATTR_UNUSED = stack.allocObject<SimpleObj>(1);
  auto obj2 = stack.allocObject<SmallPtrObj>(2);
  auto obj3 = stack.allocObject<LargePtrObj>(3);
  obj3->m_p0 = obj2;
  allocFill(stack);

  auto pos = stack.note();

  auto obj4 ATTR_UNUSED = stack.allocObject<SimpleObj>(4);
  auto obj5 = stack.allocObject<SmallPtrObj2>(5);
  obj5->m_p0 = obj3;
  allocFill(stack);
  auto obj6 = stack.allocObject<LargePtrObj>(6);
  obj6->m_p0 = obj2;
  obj6->m_p2 = obj3;
  allocFill(stack);
  auto obj7 ATTR_UNUSED = stack.allocObject<LargePtrObj>(7);
  allocFill(stack);

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + SIMPLEOBJ_SIZE // obj1 (dead)
          + (sizeof(VTable*) + sizeof(SmallPtrObj)) // obj2
          + PLACEHOLDER_SIZE // obj3
          + FILL_SIZE
          // pos
          + SIMPLEOBJ_SIZE // obj4 (dead)
          + (sizeof(VTable*) + sizeof(SmallPtrObj2)) // obj5
          + FILL_SIZE + PLACEHOLDER_SIZE // obj6
          + FILL_SIZE + PLACEHOLDER_SIZE // obj7 (dead)
          + FILL_SIZE,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(
      3 * (sizeof(VTable*) + sizeof(LargePtrObj)),
      stack.DEBUG_getLargeAllocTotal());

  std::array<RObj*, 2> objs{{obj5, obj6}};
  stack.collect(pos, (RObjOrFakePtr*)objs.data(), objs.size());
  EXPECT_NE(obj5, objs[0]);
  EXPECT_EQ(obj6, objs[1]); // Large object should NOT have moved!
  obj4 = nullptr;
  obj5 = (SmallPtrObj2*)objs[0];
  obj6 = (LargePtrObj*)objs[1];
  obj7 = nullptr;

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + SIMPLEOBJ_SIZE // obj1
          + (sizeof(VTable*) + sizeof(SmallPtrObj)) // obj2
          + PLACEHOLDER_SIZE // obj3
          + FILL_SIZE
          // pos
          //                  + SIMPLEOBJ_SIZE
          + (sizeof(VTable*) + sizeof(SmallPtrObj2)) // obj5
      //                  + FILL_SIZE
      //                  + FILL_SIZE
      //                  + FILL_SIZE
      //                // placeholder not needed because obj5 survived
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(
      2 * (sizeof(VTable*) + sizeof(LargePtrObj)),
      stack.DEBUG_getLargeAllocTotal());

  obj1->verify(stack, 1);
  obj2->verify(stack, 2);
  obj3->verify(stack, 3);
  obj5->verify(stack, 5);
  obj6->verify(stack, 6);

  // ---- collect again

  stack.collect(pos, (RObjOrFakePtr*)objs.data(), objs.size());
  EXPECT_EQ(obj5, objs[0]);
  EXPECT_EQ(obj6, objs[1]); // Large object should NOT have moved!
  obj4 = nullptr;
  obj5 = (SmallPtrObj2*)objs[0];
  obj6 = (LargePtrObj*)objs[1];
  obj7 = nullptr;

  DEBUG_EXPECT_EQ(
      512U + FILL_SIZE + SIMPLEOBJ_SIZE // obj1
          + (sizeof(VTable*) + sizeof(SmallPtrObj)) // obj2
          + PLACEHOLDER_SIZE // obj3
          + FILL_SIZE
          // pos
          //                  + SIMPLEOBJ_SIZE
          + (sizeof(VTable*) + sizeof(SmallPtrObj2))
      //                  + FILL_SIZE
      //                  + FILL_SIZE
      //                  + FILL_SIZE
      //                // placeholder still not needed
      ,
      stack.DEBUG_getSmallAllocTotal());
  DEBUG_EXPECT_EQ(
      2 * (sizeof(VTable*) + sizeof(LargePtrObj)),
      stack.DEBUG_getLargeAllocTotal());

  obj1->verify(stack, 1);
  obj2->verify(stack, 2);
  obj3->verify(stack, 3);
  obj5->verify(stack, 5);
  obj6->verify(stack, 6);
}

TEST(ObstackTest, testCollectLarge2) {
  Obstack stack;
  LameRandom random(314);

  using SmallObj = SimpleObject<128>;
  using LargeObj = PtrObj<SmallObj, SmallObj, 16384>;

  void* p = stack.alloc(512);
  random.fill(p, 512);

  auto pos = stack.note();

  auto p2 = stack.allocObject<LargeObj>(2);
  auto p3 = stack.allocObject<SmallObj>(3);
  p2->m_p0 = p3;
  p2->metadata().setFrozen();

  // Check that if we collect a large object twice we don't accidentally move it
  // behind the note and stop scanning it.
  stack.collect(pos, (RObjOrFakePtr*)&p2, 1);
  stack.collect(pos, (RObjOrFakePtr*)&p2, 1);

  auto p5 = stack.allocObject<SmallObj>(5);
  EXPECT_NE(p3, p5);
}

TEST(ObstackTest, testFreeze1) {
  Obstack stack;

  auto p1 = stack.allocObject<SimpleObject<128>>(1);
  auto p2 = reinterpret_cast<SimpleObject<128>*>(stack.freeze(p1).asPtr());
  auto p3 = stack.allocObject<SimpleObject<128>>(2);
  EXPECT_FALSE(p1->vtable().isFrozen());
  EXPECT_TRUE(p2->vtable().isFrozen());

  EXPECT_NOT_FROZEN(p1->m_data);
  EXPECT_FROZEN(p2->m_data);
  EXPECT_NOT_FROZEN(p3->m_data);

  // Freezing a frozen pointer should just yield the same pointer.
  auto p4 = reinterpret_cast<SimpleObject<128>*>(stack.freeze(p2).asPtr());
  EXPECT_EQ(p2, p4);
}

TEST(ObstackTest, testFreeze2) {
  Obstack stack;
  allocFill(stack);

  auto pos0 = stack.note();

  auto p1 = stack.allocObject<CyclicObj>(1);
  auto p2 = stack.allocObject<CyclicObj>(2);
  auto p3 = stack.allocObject<SimpleLargeObj>(3);
  auto p4 = stack.allocObject<PtrObj<CyclicObj, SimpleLargeObj>>(4);

  p2->m_p0 = p1;
  p4->m_p0 = p2;
  p4->m_p1 = p3;

  p1->verify(1);
  p2->verify(2);
  p3->verify(3);
  p4->verify(stack, 4);

  auto fz_p4 = reinterpret_cast<PtrObj<CyclicObj, SimpleLargeObj>*>(
      stack.freeze(p4).asPtr());

  EXPECT_FALSE(p1->vtable().isFrozen());
  EXPECT_FALSE(p2->vtable().isFrozen());
  EXPECT_FALSE(p3->vtable().isFrozen());
  EXPECT_FALSE(p4->vtable().isFrozen());
  p1->verify(1);
  p2->verify(2);
  p3->verify(3);
  p4->verify(stack, 4);

  auto fz_p3 = fz_p4->m_p1;
  auto fz_p2 = fz_p4->m_p0;
  auto fz_p1 = fz_p2->m_p0;

  EXPECT_TRUE(fz_p4->vtable().isFrozen());
  EXPECT_TRUE(fz_p3->vtable().isFrozen());
  EXPECT_TRUE(fz_p2->vtable().isFrozen());
  EXPECT_TRUE(fz_p1->vtable().isFrozen());
  fz_p1->verify(1);
  fz_p2->verify(2);
  fz_p3->verify(3);
  fz_p4->verify(stack, 4);

  EXPECT_NOT_FROZEN(&p1->m_a);
  EXPECT_NOT_FROZEN(&p2->m_a);
  EXPECT_NOT_FROZEN(&p3->m_a);
  EXPECT_NOT_FROZEN(&p4->m_a);

  EXPECT_FROZEN(&fz_p1->m_a);
  EXPECT_FROZEN(&fz_p2->m_a);
  EXPECT_FROZEN(&fz_p3->m_a);
  EXPECT_FROZEN(&fz_p4->m_a);

  stack.collect(pos0, (RObjOrFakePtr*)&fz_p4, 1);
  fz_p3 = fz_p4->m_p1;
  fz_p2 = fz_p4->m_p0;
  fz_p1 = fz_p2->m_p0;

  EXPECT_FROZEN(&fz_p1->m_a);
  EXPECT_FROZEN(&fz_p2->m_a);
  EXPECT_FROZEN(&fz_p3->m_a);
  EXPECT_FROZEN(&fz_p4->m_a);
}

TEST(ObstackTest, testFreeze3) {
  Obstack stack;
  for (size_t i = 0; i < 8; ++i)
    stack.alloc(500);

  auto pos0 = stack.note();
  auto pos1 = stack.note();
  auto p1 = stack.allocObject<CyclicObj>(1);
  auto p2 = stack.allocObject<CyclicObj>(2);
  p2->m_p0 = p1;
  auto p3 = stack.allocObject<CyclicObj>(3);
  p3->m_p0 = p2;

  auto fz_p3 = static_cast<const CyclicObj*>(stack.freeze(p3).asPtr());

  stack.collect(pos1, (RObjOrFakePtr*)&fz_p3, 1);
  auto fz_p2 = static_cast<const CyclicObj*>(fz_p3->m_p0);
  auto fz_p1 = static_cast<const CyclicObj*>(fz_p2->m_p0);

  EXPECT_FROZEN(fz_p1);
  EXPECT_FROZEN(fz_p2);
  EXPECT_FROZEN(fz_p3);
  fz_p1->verify(1);
  fz_p2->verify(2);
  fz_p3->verify(3);

  stack.collect(pos0, (RObjOrFakePtr*)&fz_p3, 1);
  fz_p2 = static_cast<const CyclicObj*>(fz_p3->m_p0);
  fz_p1 = static_cast<const CyclicObj*>(fz_p2->m_p0);

  EXPECT_FROZEN(fz_p1);
  EXPECT_FROZEN(fz_p2);
  EXPECT_FROZEN(fz_p3);
  fz_p1->verify(1);
  fz_p2->verify(2);
  fz_p3->verify(3);
}
} // anonymous namespace

namespace {

struct MyClass {
  static int g_cons;
  static int g_destroy;

  MyClass(int value) {
    EXPECT_EQ(value, 42);
    ++g_cons;
  }
  ~MyClass() {
    ++g_destroy;
  }
};

int MyClass::g_cons = 0;
int MyClass::g_destroy = 0;

TEST(ObstackTest, testFinalized) {
  Obstack& stack{Obstack::cur()};
  auto pos0 = stack.note();
  allocFill(stack);

  auto pos1 = stack.note();

  EXPECT_EQ(MyClass::g_cons, 0);
  EXPECT_EQ(MyClass::g_destroy, 0);

  IObj* p ATTR_UNUSED = Finalized<MyClass>::createNew(stack, 42);

  EXPECT_EQ(MyClass::g_cons, 1);
  EXPECT_EQ(MyClass::g_destroy, 0);

  stack.collect(pos1);

  EXPECT_EQ(MyClass::g_cons, 1);
  EXPECT_EQ(MyClass::g_destroy, 1);

  stack.collect(pos0);
}
} // anonymous namespace

namespace {

struct TestObj1 : RObj {
  int64_t m_verify = 1;

  void verify() {
    EXPECT_EQ(m_verify, 1);
  }

  DEFINE_VTABLE()
  static Type& static_type() {
    static auto singleton =
        Type::classFactory(typeid(TestObj1).name(), sizeof(TestObj1), {});
    return *singleton;
  }

  static size_t allocSize() {
    return sizeof(TestObj1) + static_type().uninternedMetadataByteSize();
  }
};

struct TestObj2 : RObj {
  int64_t m_verify = 2;
  TestObj1* m_p1;

  TestObj2(TestObj1* p1) : m_p1(p1) {}

  void verify() {
    EXPECT_EQ(m_verify, 2);
    m_p1->verify();
  }

  DEFINE_VTABLE()
  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(TestObj2).name(),
        sizeof(TestObj2),
        {
            offsetof(TestObj2, m_p1),
        });
    return *singleton;
  }

  static size_t allocSize() {
    return sizeof(TestObj2) + static_type().uninternedMetadataByteSize();
  }
};

struct TestObj4 : RObj {
  int64_t m_verify = 4;

  TestObj4() {}

  void verify() {
    EXPECT_EQ(m_verify, 4);
  }

  DEFINE_VTABLE()
  static Type& static_type() {
    static auto singleton =
        Type::classFactory(typeid(TestObj4).name(), sizeof(TestObj4), {});
    return *singleton;
  }

  static size_t allocSize() {
    return sizeof(TestObj4) + static_type().uninternedMetadataByteSize();
  }
};

struct TestLargeObj : RObj {
  int64_t m_verify = 4;
  std::array<char, Obstack::kChunkSize * 3 / 2> m_data;
  TestObj4* m_p4;

  TestLargeObj(TestObj4* p4) : m_p4(p4) {}

  void verify() {
    EXPECT_EQ(m_verify, 4);
    m_p4->verify();
  }

  DEFINE_VTABLE()
  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(TestLargeObj).name(),
        sizeof(TestLargeObj),
        {offsetof(TestLargeObj, m_p4)});
    return *singleton;
  }

  static size_t allocSize() {
    return sizeof(TestLargeObj) + static_type().uninternedMetadataByteSize();
  }
};

struct LifetimeTracker {
  ~LifetimeTracker() {
    --s_alive;
  }
  LifetimeTracker() {
    ++s_alive;
  }
  static size_t s_alive;
};
size_t LifetimeTracker::s_alive = 0;

struct TestObj3 : RObj {
  int64_t m_verify = 3;
  TestLargeObj* m_pLarge;
  LifetimeTracker* m_iobj;

  TestObj3(TestLargeObj* pLarge, LifetimeTracker* iobj)
      : m_pLarge(pLarge), m_iobj(iobj) {}

  void verify() {
    EXPECT_EQ(m_verify, 3);
    m_pLarge->verify();
  }

  DEFINE_VTABLE()
  static Type& static_type() {
    static auto singleton = Type::classFactory(
        typeid(TestObj3).name(),
        sizeof(TestObj3),
        {
            offsetof(TestObj3, m_pLarge),
            offsetof(TestObj3, m_iobj),
        });
    return *singleton;
  }

  static size_t allocSize() {
    return sizeof(TestObj3) + static_type().uninternedMetadataByteSize();
  }
};

#define CHECK_OBSTACK_SIZES(OBSTACK, EXPECTED_SMALL, EXPECTED_LARGE)       \
  DEBUG_EXPECT_EQ((EXPECTED_SMALL), (OBSTACK).DEBUG_getSmallAllocTotal()); \
  DEBUG_EXPECT_EQ((EXPECTED_LARGE), (OBSTACK).DEBUG_getLargeAllocTotal())

TEST(ObstackTest, testWorkerObstacks) {
  Obstack& stack1{Obstack::cur()};
  Obstack::PosScope scope(stack1); // stack1.note()

  // notes never allocate
  DEBUG_EXPECT_EQ(0, stack1.DEBUG_getSmallAllocTotal());

  TestObj1* p1_1 = stack1.allocObject<TestObj1>();
  TestLargeObj* pLarge1;

  DEBUG_EXPECT_EQ(
      TestObj1::allocSize() // p1_1
      ,
      stack1.DEBUG_getSmallAllocTotal());

  auto note1 = stack1.note();

  {
    Obstack stack2(note1);

    TestObj2* p2_2 = stack2.allocObject<TestObj2>(p1_1);
    TestObj3* p3_2;

    auto note2 = stack2.note();

    {
      Obstack stack3(note2);

      auto p4_3 = stack3.allocObject<TestObj4>();
      pLarge1 = stack3.allocObject<TestLargeObj>(p4_3);

      // This large object isn't copied so we need to make sure it's properly
      // cleaned up.
      auto pLarge2 = stack3.allocObject<TestLargeObj>(p4_3);

      // TODO: Add a small pointer to the large object and check it

      auto iobj1 = &Finalized<LifetimeTracker>::createNew(stack3)->m_cppClass;
      auto iobj2 = &Finalized<LifetimeTracker>::createNew(stack3)->m_cppClass;
      (void)iobj2;

      auto p3_3 = stack3.allocObject<TestObj3>(pLarge1, iobj1);
      EXPECT_TRUE(stack3.DEBUG_isAlive(p3_3));
      EXPECT_TRUE(stack3.DEBUG_isAlive(pLarge1));
      EXPECT_TRUE(stack3.DEBUG_isAlive(pLarge2));

      CHECK_OBSTACK_SIZES(stack1, TestObj1::allocSize(), 0);
      CHECK_OBSTACK_SIZES(stack2, TestObj2::allocSize(), 0);

      CHECK_OBSTACK_SIZES(
          stack3,
          0 + TestObj4::allocSize() // p4_3
              + PLACEHOLDER_SIZE // pLarge1
              + PLACEHOLDER_SIZE // pLarge2
              + PLACEHOLDER_SIZE // iobj1
              + PLACEHOLDER_SIZE // iobj2
              + TestObj3::allocSize() // p3_3
          ,
          TestLargeObj::allocSize() * 2);

      p3_2 = p3_3;
      stack2.TEST_stealObjects(note2, stack3);
      EXPECT_TRUE(stack2.DEBUG_isAlive(p3_2));
      EXPECT_TRUE(stack2.DEBUG_isAlive(p3_2->m_pLarge));

      p3_2->verify();
      EXPECT_EQ(p3_2->m_pLarge, pLarge1);
      EXPECT_FALSE(p3_2->isFrozen());

      CHECK_OBSTACK_SIZES(stack1, TestObj1::allocSize(), 0);
    }

    stack1.TEST_stealObjects(note1, stack2);
    auto p2_1 = p2_2;
    auto p3_1 = p3_2;
    EXPECT_TRUE(stack1.DEBUG_isAlive(p2_1));
    EXPECT_TRUE(stack1.DEBUG_isAlive(p3_1));

    EXPECT_EQ(p2_1->m_p1, p1_1);
    EXPECT_FALSE(p2_1->isFrozen());

    EXPECT_EQ(p3_1->m_pLarge, pLarge1);
    EXPECT_FALSE(p3_1->isFrozen());

    p2_1->verify();
    p3_1->verify();
    EXPECT_EQ(p2_1->m_p1, p1_1);
  }
}

// A pinned object is not movable, but also not inherently a root.
// Pinned objects are allocated from the large object space regardless
// of their size.
TEST(ObstackTest, testPinned) {
  Obstack stack;
  const auto note = stack.note();
  const auto sz = Obstack::kAllocAlign;
  (void)stack.alloc(sz); // unreachable, causing p1 to move
  const auto p1 = stack.allocObject<SimpleObject<sz>>(0x01); // movable
  const auto p2 = stack.allocPinnedObject<SimpleObject<sz>>(0x01); // pinned
  std::array<RObj*, 2> roots{{p1, p2}};
  stack.collect(note, (RObjOrFakePtr*)roots.data(), roots.size());
  EXPECT_NE(roots[0], p1);
  EXPECT_EQ(roots[1], p2);
}

// A handle acts as a root, and the target object can still be moved.
TEST(ObstackTest, testHandle_collect0) {
  Obstack stack;
  const auto note = stack.note();
  const auto sz = Obstack::kAllocAlign;
  (void)stack.alloc(sz); // unreachable, causing p1 to move
  const auto p1 = stack.allocObject<SimpleObject<sz>>(0x41); // movable
  auto handle = stack.makeHandle(p1);
  EXPECT_EQ(p1, handle->get().asPtr());
  EXPECT_EQ(p1->m_data[0], 0x41); // data intact
  stack.collect(note); // no explicit roots
  const auto p1a = static_cast<SimpleObject<sz>*>(handle->get().asPtr());
  EXPECT_NE(p1, p1a);
  EXPECT_EQ(p1a->m_data[0], 0x41); // data intact
}

// A handle acts as a root, and the target object can still be moved.
TEST(ObstackTest, testHandle_collect1) {
  Obstack stack;
  const auto note = stack.note();
  const auto sz = Obstack::kAllocAlign;
  (void)stack.alloc(sz); // unreachable, causing p1 to move
  const auto p1 = stack.allocObject<SimpleObject<sz>>(0x41); // movable
  const auto p2 = stack.allocObject<SimpleObject<sz>>(0x42); // movable
  // create a handle for p1, but collect with p2 as an explicit root
  auto handle = stack.makeHandle(p1);
  EXPECT_EQ(p1, handle->get().asPtr());
  EXPECT_EQ(p1->m_data[0], 0x41); // data intact
  std::array<RObj*, 1> roots{{p2}};
  stack.collect(note, (RObjOrFakePtr*)roots.data(), roots.size());
  const auto p1a = static_cast<SimpleObject<sz>*>(handle->get().asPtr());
  const auto p2a = static_cast<SimpleObject<sz>*>(roots[0]);
  EXPECT_NE(p1, p1a);
  EXPECT_NE(p2, p2a);
  EXPECT_EQ(p1a->m_data[0], 0x41); // data intact
  EXPECT_EQ(p2a->m_data[0], 0x42); // data intact
}
} // anonymous namespace

// clang-format off
ALLOW_PRIVATE_ACCESS{
static void testHandleSteal() {
  std::vector<std::unique_ptr<RObjHandle>> handles;
  auto note = Obstack::cur().note();

  constexpr int numTestResults = 10;
  std::vector<int> testResults;

  EXPECT_FALSE(Obstack::cur().m_detail->anyHandles());

  {
    // Create a Child process.
    auto child = Process::make(UnownedProcess{Process::cur()}, note, nullptr);

    {
      ProcessContextSwitcher switcher{child};

      Obstack& childObstack = Obstack::cur();

      // Allocate some objects (small and large).
      std::vector<RObj*> objs = {
        childObstack.allocObject<SimpleObject<128>>(0x01),
        childObstack.allocObject<SimpleObject<8 * 1024 * 1024>>(0x02)
      };

      // Create Handles for them.
      for (auto& obj : objs) {
        handles.push_back(childObstack.makeHandle(obj));
      }

      // Ensure each Handle is currently owned by the child process.
      for (auto& handle : handles) {
        EXPECT_EQ(handle->m_owner, child.get());
      }
    }

    // Post some tasks for the child, to make sure they get transferred.
    UnownedProcess remoteChild{child};
    for (size_t i = 0; i < numTestResults; ++i) {
      remoteChild.schedule([&testResults, i](){ testResults.push_back(i); });
    }
    EXPECT_EQ(testResults.size(), 0);

    Process::cur()->joinChild(*child, note);
  }

  // Ensure each Handle is now owned by the parent process.
  for (auto& handle : handles) {
    EXPECT_EQ(handle->m_owner, Process::cur());
  }

  // Ensure that dropping our handle references removes them from the Obstack.
  EXPECT_TRUE(Obstack::cur().m_detail->anyHandles());
  handles.clear();
  EXPECT_FALSE(Obstack::cur().m_detail->anyHandles());

  // Make sure no one has run the child's tasks.
  EXPECT_EQ(testResults.size(), 0);

  // Run tasks on the parent and make sure that ran the child tasks,
  // in the expected order (which is reversed from the posting order,
  // as it would have been if we ran them in the child).
  Process::cur()->runReadyTasks();
  EXPECT_EQ(testResults.size(), numTestResults);
  for (int i = 0; i < numTestResults; ++i) {
    EXPECT_EQ(testResults[i], numTestResults - 1 - i);
  }
}
};

namespace {

// A handle acts as a root, and the target object can still be moved.
TEST(ObstackTest, testHandle_steal) {
  // Run inside a subroutine with more access permissions.
  PRIVATE::testHandleSteal();
}

} // anonymous namespace
#endif
