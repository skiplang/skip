/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <sys/time.h>

#include <cstddef>
#include <iostream>

#include <boost/functional/hash.hpp>

#include <folly/Hash.h>

#include <gtest/gtest.h>

#include "skip/intern-extc.h"

#include "skip/Type.h"
#include "skip/leak.h"
#include "skip/objects.h"
#include "skip/Obstack.h"
#include "skip/String.h"
#include "testutil.h"

using namespace skip;
using namespace skip::test;

auto& internTable = getInternTable();

// clang-format off
ALLOW_PRIVATE_ACCESS{

  static void InternTable_hardReset() {
    internTable.TEST_hardReset();
  }

};
// clang-format on

namespace {

struct InternTest : ::testing::Test {
 protected:
  ~InternTest() {
    EXPECT_EQ(internTable.size(), 0U);
  }
};

struct AcyclicTest : InternTest {};

// Run cyclic tests in two modes, with and without local hash collisions.
struct CyclicTest : InternTest, ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    forceFakeLocalHashCollisionsForTests(GetParam());
  }
};

/// A verbosity level, higher is more verbose. Each "-v" increments it.
static int verbose = 0;

struct RTest1;

template <typename Base>
struct Test1 : Base {
  int64_t n1;
  const RTest1* p1;
  int64_t n2;
  const RTest1* p2;
  int64_t n3;
  String s1;

  DEFINE_VTABLE()

  static Type& static_type() {
    // Compute lazily to avoid the "static initialization order fiasco".
    static auto t = createClassType(
        typeid(Test1).name(),
        sizeof(Test1),
        {offsetof(Test1, p1), offsetof(Test1, p2), offsetof(Test1, s1)},
        comparator);
    return *t;
  }

  static bool comparator(const RObj& r1, const RObj& r2, RObjPairSet& seen);

 protected:
  ~Test1() = default;
  Test1() = default;
};

// An ITest1 is a Test1 which inherits from IObj
struct MutableITest1 final : Test1<IObj>, TestIObjHelper<MutableITest1> {
  const RTest1* asRObj() const;
};

using ITest1 = const MutableITest1;
using ITest1Ptr = boost::intrusive_ptr<ITest1>;

// An RTest1 is a Test1 which inherits from RObj
struct RTest1 final : Test1<RObj>, TestRObjHelper<RTest1, ITest1> {};

template <typename Base>
bool Test1<Base>::comparator(
    const RObj& r1,
    const RObj& r2,
    RObjPairSet& seen) {
  // The caller ensured this is legal.
  const Test1& t1 = static_cast<const Test1&>(r1);
  const Test1& t2 = static_cast<const Test1&>(r2);

  // Compare the fields.
  if (t1.n1 != t2.n1 || t1.n2 != t2.n2 || t1.n3 != t2.n3 || t1.s1 != t2.s1) {
    return false;
  }

  return (
      objectsEqualHelper(t1.p1, t2.p1, seen) &&
      objectsEqualHelper(t1.p2, t2.p2, seen));
}

static_assert(sizeof(ITest1) == sizeof(RTest1), "mismatched sizes");

const RTest1* ITest1::asRObj() const {
  return reinterpret_cast<const RTest1*>(this);
}

/// Test deepCompare() for internal consistency on the given graph.
template <typename T>
static void checkCompare(
    const skip::fast_map<T*, typename T::InternType::Ptr>& graph) {
  // Convert to a more convenient array form.
  std::vector<typename T::InternType*> objs;
  objs.reserve(graph.size());
  for (auto vp : graph) {
    objs.push_back(vp.second.get());
  }

  // Compare all pairs of values.
  const size_t size = objs.size();
  std::vector<std::vector<int8_t>> cmp(size);
  size_t i1 = 0;
  for (auto o1 : objs) {
    for (auto o2 : objs) {
      auto cc = deepCompare(o1, o2);

      // Convert the number -1, 0, 1 form and record the value.
      const int8_t c = (cc > 0) - (cc < 0);
      cmp[i1].push_back(c);

      // Make sure that ordered and unordered comparisons agree on equality.
      EXPECT_EQ((c == 0), deepEqual(o1, o2));
    }

    ++i1;
  }

  // Check some obvious properties of the comparison matrix.
  for (size_t i = 0; i < size; ++i) {
    // Everything must equal itself.
    EXPECT_EQ(cmp[i][i], 0);

    // Flipping the order of the compares should flip the result sign.
    for (size_t j = 0; j < size; ++j) {
      EXPECT_EQ(cmp[i][j], -cmp[j][i]);
    }
  }

  for (size_t i = 0; i < size; ++i) {
    for (size_t j = 0; j < size; ++j) {
      switch (cmp[i][j]) {
        case 0:
          // i and j are equal. That means everything i is equal to, j must
          // be also, and vice versa.
          for (size_t k = 0; k < size; ++k) {
            EXPECT_EQ((cmp[i][k] == 0), (cmp[j][k] == 0));
          }
          break;
        case 1:
          // i > j. Everything i is <=, j must be <.
          for (size_t k = 0; k < size; ++k) {
            if (cmp[i][k] <= 0) {
              EXPECT_LT(cmp[j][k], 0);
            }
          }
          break;
        case -1:
          // i < j. Everything i is >=, j must be >.
          for (size_t k = 0; k < size; ++k) {
            if (cmp[i][k] >= 0) {
              EXPECT_GT(cmp[j][k], 0);
            }
          }
          break;
        default:
          abort();
      }
    }
  }
}

template <typename T>
static void mungeGraph(
    const skip::fast_map<T*, typename T::InternType::Ptr>& graph) {
  // Use this graph to test out deepCompare().
  checkCompare(graph);

  bool verified = false;

  // Initial sanity check the input graph.
  for (auto vp : graph) {
    auto u = vp.first;
    auto& i = vp.second;

    EXPECT_FALSE(u->isInterned());
    EXPECT_TRUE(i->isInterned());
    EXPECT_TRUE(i->vtable().isFrozen());
    EXPECT_TRUE(u->equal(*i));

    // Make sure it interns exactly as the caller claimed.
    auto iobj = vp.first->intern();
    EXPECT_EQ(iobj, vp.second);

    if (!verified) {
      internTable.verifyInvariants();
      verified = true;
    }
  }

  const size_t internTableSize = internTable.size();

  // Make sure no two interned objects are recursively equal.
  for (auto it1 = graph.begin(); it1 != graph.end(); ++it1) {
    auto& obj1 = it1->second;

    for (auto it2 = it1; ++it2 != graph.end();) {
      auto& obj2 = it2->second;

      if (obj1 != obj2 && obj1->equal(*obj2)) {
        std::cerr << "Interned objects unexpectedly equal: "
                  << (void*)obj1.get() << " == " << (void*)obj2.get()
                  << std::endl;
        abort();
      }
    }
  }

  // Try replacing every reference in the original graph to a reference
  // in the already-interned graph. This should have no effect on interning,
  // since they are by definition isomorphic.
  for (auto vp : graph) {
    auto u = vp.first;

    u->eachValidRef([&](RObj*& ref) {
      auto old = ref;

      if (!old->isInterned()) {
        auto it = graph.find(static_cast<T*>(old));
        if (it != graph.end()) {
          ref = const_cast<
              typename std::remove_const<typename T::InternType>::type*>(
              it->second.get());

          // Reintern every member of the graph and check the answer.
          for (auto vvp : graph) {
            auto iobj = vvp.first->intern();

            // Interning again should not change the table size.
            EXPECT_EQ(internTableSize, internTable.size());
            EXPECT_EQ(iobj, vvp.second);
          }

          // Make sure that comparison invariants still hold.
          checkCompare(graph);

          ref = old;
        }
      }
    });
  }

  internTable.verifyInvariants();
}

// For debugging only.
static void dumpGraph(const std::vector<std::unique_ptr<RTest1>>& objs) {
  skip::fast_map<const RTest1*, size_t> objToIndex;
  for (auto& obj : objs) {
    bool isNew = objToIndex.emplace(obj.get(), objToIndex.size()).second;
    EXPECT_TRUE(isNew);
  }

  std::cout << "Graph of size " << objs.size() << '\n';

  size_t index = 0;
  for (auto& obj : objs) {
    std::cout << "  #" << index++ << ": ";
    if (obj->n1 != 0) {
      std::cout << " n1=" << obj->n1;
    }
    if (obj->n2 != 0) {
      std::cout << " n3=" << obj->n2;
    }
    if (obj->n3 != 0) {
      std::cout << " n3=" << obj->n3;
    }
    if (const RTest1* p1 = obj->p1) {
      std::cout << " p1->";

      auto it = objToIndex.find(p1);
      if (it != objToIndex.end()) {
        std::cout << '#' << it->second;
      } else {
        std::cout << '[' << (void*)p1 << " [n1=" << obj->n1 << ']';
      }
    }
    if (const RTest1* p2 = obj->p2) {
      std::cout << " p2->";

      auto it = objToIndex.find(p2);
      if (it != objToIndex.end()) {
        std::cout << '#' << it->second;
      } else {
        std::cout << '[' << (void*)p2 << " [n1=" << obj->n1 << ']';
      }
    }
    std::cout << '\n';
  }
}

struct TestValueClass1 final {
  const RTest1* p1;
  size_t n1;
  const RTest1* p2;
};

template <typename Base>
struct ArrayOfTestValueClass1 : Base {
  DEFINE_VTABLE()

  static Type& static_type() {
    static auto t = createArrayType(
        typeid(TestValueClass1).name(),
        sizeof(TestValueClass1),
        {offsetof(TestValueClass1, p1), offsetof(TestValueClass1, p2)},
        comparator);
    return *t;
  }

  static bool comparator(const RObj& i1, const RObj& i2, RObjPairSet& seen) {
    // The caller ensured this is legal.
    auto& t1 = static_cast<const ArrayOfTestValueClass1&>(i1);
    auto& t2 = static_cast<const ArrayOfTestValueClass1&>(i2);

    const size_t size = t1.arraySize();
    if (size != t2.arraySize()) {
      return false;
    }

    // Check local state before recursing.
    for (size_t i = 0; i < size; ++i) {
      if (t1[i].n1 != t2[i].n1) {
        return false;
      }
    }

    // Recurse on all references.
    for (size_t i = 0; i < size; ++i) {
      if (!objectsEqualHelper(t1[i].p1, t2[i].p1, seen) ||
          !objectsEqualHelper(t1[i].p2, t2[i].p2, seen)) {
        return false;
      }
    }

    return true;
  }

  TestValueClass1& operator[](size_t index) {
    return *static_cast<TestValueClass1*>(mem::add(
        this, roundUp(sizeof(TestValueClass1), sizeof(void*)) * index));
  }

  const TestValueClass1& operator[](size_t index) const {
    return *static_cast<const TestValueClass1*>(mem::add(
        this, roundUp(sizeof(TestValueClass1), sizeof(void*)) * index));
  }

 protected:
  ~ArrayOfTestValueClass1() = default;
  ArrayOfTestValueClass1() = default;
};

struct RArrayOfTestValueClass1;

struct MutableIArrayOfTestValueClass1 final
    : ArrayOfTestValueClass1<IObj>,
      TestIObjHelper<MutableIArrayOfTestValueClass1> {
  const RArrayOfTestValueClass1* asRObj() const;
};

using IArrayOfTestValueClass1 = const MutableIArrayOfTestValueClass1;

struct RArrayOfTestValueClass1 final
    : ArrayOfTestValueClass1<AObjBase>,
      TestRObjHelper<RArrayOfTestValueClass1, IArrayOfTestValueClass1> {};

static_assert(
    sizeof(IArrayOfTestValueClass1) == sizeof(RArrayOfTestValueClass1),
    "mismatched sizes");

const RArrayOfTestValueClass1* IArrayOfTestValueClass1::asRObj() const {
  return reinterpret_cast<const RArrayOfTestValueClass1*>(this);
}

static void dumpGraph(
    const std::vector<std::unique_ptr<RArrayOfTestValueClass1>>&) {
  // Dumping not implemented yet.
}

template <typename T>
static void mungeGraph(const std::vector<std::unique_ptr<T>>& objs) {
  if (verbose > 1) {
    dumpGraph(objs);
  }

  // Convert the vector to a graph by interning everything.
  skip::fast_map<T*, typename T::InternType::Ptr> graph;
  graph.reserve(objs.size());
  for (auto& t : objs) {
    graph.emplace(t.get(), t->intern());
  }

  // Test the graph.
  mungeGraph(graph);
}

TEST_F(AcyclicTest, testSimple) {
  // Make two identical test instances.
  auto obj1 = newTestInstance<RTest1>();
  auto obj2 = newTestInstance<RTest1>();

  obj1->n3 = 37;
  obj2->n3 = 37;

  // Make sure that interning yields the same values.
  auto iobj1a = obj1->intern();
  auto iobj1b = obj1->intern();
  auto iobj2 = obj2->intern();
  EXPECT_EQ(iobj1a, iobj1b);
  EXPECT_EQ(iobj1a, iobj2);

  EXPECT_TRUE(iobj1a->equal(*obj1));

  EXPECT_TRUE(iobj1a->vtable().isFrozen());
  EXPECT_TRUE(iobj1b->vtable().isFrozen());
  EXPECT_TRUE(iobj2->vtable().isFrozen());

  EXPECT_REFCOUNT(iobj1a, 3U);
}

TEST_F(AcyclicTest, testString) {
  Obstack::PosScope pos(Obstack::cur());

  std::vector<std::unique_ptr<RTest1>> objs;
  for (auto i : generateInterestingStrings()) {
    auto obj = newTestInstance<RTest1>();
    obj->s1 = String(i);
    objs.push_back(std::move(obj));
  }

  std::vector<ITest1Ptr> iobjs(objs.size());
  for (size_t i = 0; i < objs.size(); ++i) {
    iobjs[i] = objs[i]->intern();
    // Interning a second time should give the same interned object.
    EXPECT_EQ(objs[i]->intern(), iobjs[i]);
    EXPECT_TRUE(iobjs[i]->equal(*objs[i]));
  }

  for (size_t i = 0; i < objs.size(); ++i) {
    for (size_t j = 0; j < objs.size(); ++j) {
      if (i == j) {
        EXPECT_EQ(iobjs[i]->s1, objs[j]->s1);
      } else {
        EXPECT_NE(iobjs[i]->s1, iobjs[j]->s1);
        EXPECT_NE(iobjs[i]->s1, objs[j]->s1);
      }
    }
  }

  // TODO: EXPECT_NE test too
}

/// Insert enough objects to force some collisions or rehashes.
TEST_F(AcyclicTest, testRehashing) {
  const size_t count = 1 + ((size_t)1 << InternTable::kLog2MinBuckets) * 4;

  std::vector<ITest1::Ptr> mappings(count);

  // Create a bunch of dummy test objects.
  uint64_t val = 0;
  for (size_t i = 0; i < count; ++i) {
    // Create a unique test object.
    auto robj = newTestInstance<RTest1>();

    val += i;
    robj->n1 = val;

    // Intern it.
    auto iobj = robj->intern();
    EXPECT_REFCOUNT(iobj, 1U);

    EXPECT_EQ(internTable.size(), i + 1);

    // Remember what it mapped to.
    mappings[i] = std::move(iobj);

    // Check invariants only occasionally, as it is expensive.
    if (i % (count / 3) == 0) {
      internTable.verifyInvariants();
    }
  }

  internTable.verifyInvariants();

  // Look up the dummy test objects.
  val = 0;
  for (size_t i = 0; i < count; ++i) {
    // Create a unique test object.
    auto robj = newTestInstance<RTest1>();
    val += i;
    robj->n1 = val;

    // Intern it. We should find one that already existed.
    {
      auto iobj = robj->intern();
      EXPECT_EQ(mappings[i], iobj);
      EXPECT_REFCOUNT(iobj, 2U);
    }

    EXPECT_EQ(internTable.size(), count);

    // Check invariants only occasionally, as it is expensive.
    if (i % (count / 3) == 0) {
      internTable.verifyInvariants();
    }
  }

  // Free interned objects.
  EXPECT_EQ(internTable.size(), mappings.size());
  while (!mappings.empty()) {
    mappings.pop_back();
    EXPECT_EQ(internTable.size(), mappings.size());
  }

  internTable.verifyInvariants();
  PRIVATE::InternTable_hardReset();
}

/// Test a simple list of two objects.
TEST_F(AcyclicTest, testList) {
  auto leaf = newTestInstance<RTest1>();
  auto root = newTestInstance<RTest1>();
  root->p1 = leaf.get();

  // Create an interned root.
  auto iRoot = root->intern();

  EXPECT_EQ(internTable.size(), 2U);

  EXPECT_TRUE(iRoot->equal(*root));
  EXPECT_REFCOUNT(iRoot, 1U);
  EXPECT_REFCOUNT(iRoot->p1->asInterned(), 1U);

  // Interning the leaf directly should do nothing but bump its refcount.
  auto iLeaf = leaf->intern();
  EXPECT_EQ(iLeaf->asRObj(), iRoot->p1);
  EXPECT_REFCOUNT(iLeaf, 2U);
  EXPECT_EQ(internTable.size(), 2U);
}

template <typename T>
std::unique_ptr<T> newArrayInstance(skip::arraysize_t size) {
  Type& type = T::static_type();
  const size_t metadataSize = type.uninternedMetadataByteSize();
  const size_t byteSize = metadataSize + size * type.userByteSize();

  char* const mem = static_cast<char*>(::malloc(byteSize));
  EXPECT_NE(mem, nullptr);

  memset(mem, 0, byteSize);

  auto ptr = initializeMetadata(mem, T::static_vtable());
  auto ret = static_cast<T*>(ptr);

  ret->setArraySize(size);

  EXPECT_FALSE(ret->isInterned());

  return std::unique_ptr<T>(ret);
};

// Simple test of an array.
TEST_F(AcyclicTest, testArray1) {
  const size_t size = 9;
  auto a = newArrayInstance<RArrayOfTestValueClass1>(size);
  EXPECT_EQ(a->arraySize(), size);

  for (size_t i = 0; i < size; ++i) {
    (*a)[i].n1 = (i + 1) * (i + 1);
  }

  size_t index = 0;
  a->eachValidRef([&](RObj*& ref) {
    if (index & 1) {
      EXPECT_EQ((void*)&ref, (void*)&(*a)[index >> 1].p2);
    } else {
      EXPECT_EQ((void*)&ref, (void*)&(*a)[index >> 1].p1);
    }
    ++index;
  });

  {
    auto x = a->intern();

    EXPECT_NE(x->asRObj(), a.get());

    EXPECT_EQ(1U, internTable.size());

    EXPECT_TRUE(x->isFullyInterned());
    EXPECT_TRUE(x->vtable().isFrozen());
    ASSERT_EQ(size, x->arraySize());

    for (size_t i = 0; i < size; ++i) {
      EXPECT_EQ((*x)[i].n1, (i + 1) * (i + 1));
    }
  }

  std::vector<std::unique_ptr<RArrayOfTestValueClass1>> objs;
  objs.push_back(std::move(a));

  mungeGraph(objs);
}

TEST_F(AcyclicTest, testArray2) {
  const size_t size = 16;

  // Create some objects where each block of 4 will get interned.
  auto objs = newTestInstances<RTest1>(size * 2);
  for (size_t i = 0; i < objs.size(); ++i) {
    objs[i]->n1 = (i >> 2) + 99;
  }

  auto a = newArrayInstance<RArrayOfTestValueClass1>(size);
  EXPECT_EQ(a->arraySize(), size);

  for (size_t i = 0; i < size; ++i) {
    (*a)[i].n1 = (i + 1) * (i + 1);
    (*a)[i].p1 = objs[i].get();
    (*a)[i].p2 = objs[i + size].get();
  }

  {
    auto x = a->intern();
    EXPECT_NE(x->asRObj(), a.get());

    EXPECT_EQ(internTable.size(), 1 + (objs.size() >> 2));

    EXPECT_TRUE(x->isFullyInterned());
    EXPECT_TRUE(x->vtable().isFrozen());
    EXPECT_EQ(x->arraySize(), size);

    // Make sure the fields got processed properly.

    for (size_t i = 0; i < size; ++i) {
      const auto d = (*x)[i];

      EXPECT_EQ(d.n1, (i + 1) * (i + 1));

      EXPECT_EQ(d.p1->n1, (int)(i >> 2) + 99);
      EXPECT_TRUE(d.p1->isFullyInterned());
      EXPECT_TRUE(d.p1->vtable().isFrozen());
      EXPECT_REFCOUNT(d.p1->asInterned(), 4U);

      EXPECT_EQ(d.p2->n1, (int)((i + size) >> 2) + 99);
      EXPECT_TRUE(d.p2->isFullyInterned());
      EXPECT_TRUE(d.p2->vtable().isFrozen());
      EXPECT_REFCOUNT(d.p2->asInterned(), 4U);

      // Subobjects should have been interned in this pattern.
      EXPECT_EQ(d.p1, (*x)[i & -4].p1);
      EXPECT_EQ(d.p2, (*x)[i & -4].p2);
    }
  }
}

TEST_F(AcyclicTest, testDag) {
  // Make a simple DAG:
  //
  //     root
  //     |  |
  //  mid1  mid2
  //     |  |
  //     leaf

  auto leaf = newTestInstance<RTest1>();
  auto mid1 = newTestInstance<RTest1>();
  auto mid2 = newTestInstance<RTest1>();
  auto root = newTestInstance<RTest1>();

  root->p1 = mid1.get();
  root->p2 = mid2.get();
  root->n1 = 37;

  mid1->p1 = leaf.get();
  mid1->n1 = 99;

  mid2->p2 = leaf.get();
  mid2->n1 = 65;

  leaf->n1 = 55;
  leaf->n2 = 66;
  leaf->n3 = 77;

  // Create an interned root.
  auto iRoot = root->intern();

  EXPECT_EQ(internTable.size(), 4U);

  EXPECT_TRUE(iRoot->equal(*root));

  EXPECT_REFCOUNT(iRoot, 1U);
  EXPECT_REFCOUNT(iRoot->p1->asInterned(), 1U);
  EXPECT_REFCOUNT(iRoot->p2->asInterned(), 1U);
  EXPECT_REFCOUNT(iRoot->p1->p1->asInterned(), 2U);
  EXPECT_EQ(iRoot->p1->p1, iRoot->p2->p2);
}

TEST_P(CyclicTest, testComplexSCC) {
  auto C = newTestInstance<RTest1>();
  auto A1 = newTestInstance<RTest1>();
  auto A2 = newTestInstance<RTest1>();
  auto B1 = newTestInstance<RTest1>();
  auto B2 = newTestInstance<RTest1>();

  A1->p1 = B1.get();
  B1->p1 = A2.get();
  A2->p1 = B2.get();
  B2->p1 = A1.get();
  B1->p2 = C.get();
  B2->p2 = C.get();
  auto A = A1->intern();
  auto Atest = A2->intern();
  EXPECT_EQ(A, Atest);
}

static size_t testOneRandomDag(LameRandom& r) {
  constexpr uint32_t kNull = (1u << 14) - 1;

  // This encodes the parameters to create a Test1 object.  We use it
  // to produce a canonical key per object to predict what the
  // interner is going to do, so we can verify it interned as expected.
  union TestCase {
    struct {
      uint32_t n1 : 1;
      uint32_t n2 : 1;
      uint32_t n3 : 1;
      uint32_t p1 : 14;
      uint32_t p2 : 15;
    };
    uint32_t bits;
  };

  const size_t size = r.next(10) + 1;
  auto objs = newTestInstances<RTest1>(size);

  // These data structures track what the interner should end up doing.
  std::vector<size_t> canonicalIndex(size);
  skip::fast_map<uint32_t, size_t> keyToIndex(size);

  for (size_t i = 0; i < size; ++i) {
    TestCase tc = {{0, 0, 0, kNull, kNull}};

    const uint32_t bits = r.next();

    // Only use a small number of bit patterns to make sharing more likely.
    tc.n1 = (bits & 0x00F) != 0;
    tc.n2 = (bits & 0x0F0) != 0;
    tc.n3 = (bits & 0xF00) != 0;

    if (i != 0) {
      if (bits & 0x3000) {
        // Tend to point to later objects in the array, for longer chains.
        const uint32_t n1 = r.next(i);
        const uint32_t n2 = r.next(i);
        tc.p1 = std::max(n1, n2);
      }
      if (bits & 0xC000) {
        // Tend to point to later objects in the array, for longer chains.
        const uint32_t n1 = r.next(i);
        const uint32_t n2 = r.next(i);
        tc.p2 = std::max(n1, n2);
      }
    }

    // Create a test object with the given parameters.
    auto& obj = objs[i];
    obj->n1 = tc.n1;
    obj->n2 = tc.n2;
    obj->n3 = tc.n3;
    if (tc.p1 != kNull) {
      obj->p1 = objs[tc.p1].get();

      // Canonicalize the pointed-to child in the same way the interner will.
      tc.p1 = canonicalIndex[tc.p1];
    }
    if (tc.p2 != kNull) {
      obj->p2 = objs[tc.p2].get();

      // Canonicalize the pointed-to child in the same way the interner will.
      tc.p2 = canonicalIndex[tc.p2];
    }

    // Record this object.
    canonicalIndex[i] = keyToIndex.emplace(tc.bits, i).first->second;
  }

  // Intern from high to low to force longer chains to get done at once.
  std::vector<ITest1::Ptr> interned(size);
  for (size_t i = size; i-- > 0;) {
    auto iobj = objs[i]->intern();
    EXPECT_TRUE(objs[i]->equal(*iobj));
    interned[i] = std::move(iobj);
  }

  internTable.verifyInvariants();

  EXPECT_EQ(internTable.size(), keyToIndex.size());

  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(interned[i], interned[canonicalIndex[i]]);
  }

  mungeGraph(objs);

  // Free the interned objects.
  interned.clear();
  EXPECT_EQ(internTable.size(), 0U);

  return size - keyToIndex.size();
}

TEST_F(AcyclicTest, testManyRandomDags) {
  LameRandom r(37);

  size_t totalShared = 0;
  for (int i = 0; i < 1000; ++i) {
    totalShared += testOneRandomDag(r);
  }

  // Make sure the test is actually doing some useful sharing work.
  EXPECT_NE(totalShared, 0U);
}

static std::unique_ptr<RTest1> newTrivialCycleInstance() {
  auto p = newTestInstance<RTest1>();

  p->n1 = 10;
  p->n2 = 20;
  p->n3 = 30;
  p->p1 = p.get();
  p->p2 = nullptr;

  return p;
}

static void checkTrivialCycleInstance(
    ITest1& p,
    ITest1& expectedP1,
    Refcount cycleRefcount) {
  EXPECT_TRUE(p.isInterned());
  EXPECT_TRUE(p.vtable().isFrozen());
  EXPECT_TRUE(p.isCycleMember());
  EXPECT_TRUE(
      p.refcountDelegate().vtable().type() == CycleHandle::static_type());
  EXPECT_EQ(p.refcountDelegate().currentRefcount(), cycleRefcount);

  EXPECT_EQ(p.n1, 10);
  EXPECT_EQ(p.n2, 20);
  EXPECT_EQ(p.n3, 30);
  EXPECT_EQ(p.p1, expectedP1.asRObj());
  EXPECT_EQ(p.p2, nullptr);
}

/// Test interning a single object that points to itself, and various
/// graphs that reduce to that.
TEST_P(CyclicTest, testCycle1) {
  //
  // Intern a single object that points to itself.
  //

  auto t1 = newTrivialCycleInstance();
  auto i1 = t1->intern();
  EXPECT_EQ(internTable.size(), 1U);

  EXPECT_NE(i1->asRObj(), t1.get());
  checkTrivialCycleInstance(*i1, *i1, 1);

  //
  // Make another object that points to itself.
  //
  {
    auto t2 = newTrivialCycleInstance();
    auto i2 = t2->intern();
    EXPECT_EQ(internTable.size(), 1U);

    EXPECT_EQ(i2, i1);
    checkTrivialCycleInstance(*i2, *i1, 2);
  }

  //
  // Make an object that does nothing but point to the interned object above
  // (Example 1 in intern.cpp). It should end up getting collapsed into it.
  //
  {
    auto t3 = newTrivialCycleInstance();
    t3->p1 = const_cast<RTest1*>(i1->asRObj());

    auto i3 = t3->intern();
    EXPECT_EQ(internTable.size(), 1U);
    EXPECT_EQ(i3, i1);
    checkTrivialCycleInstance(*i3, *i1, 2);

    EXPECT_EQ(internTable.size(), 1U);
  }

  //
  // Make a chain of isomorphic objects leading up to a loop of various sizes.
  //
  {
    const size_t loopSize = 10;
    auto loop = newTestInstances<RTest1>(loopSize, newTrivialCycleInstance);
    for (size_t i = 0; i < loopSize; ++i) {
      loop[i]->p1 = loop[(i + 1) % loopSize].get();
    }

    // Try pointing the last entry to various kinds of loops. No matter
    // what we should end up interned with the same one-object loop.
    std::vector<const RTest1*> lastPointers = {t1.get(), i1->asRObj()};
    for (auto& p : loop) {
      lastPointers.push_back(p.get());
    }

    for (auto lastPointer : lastPointers) {
      loop[loopSize - 1]->p1 = lastPointer;

      auto i4 = loop[0]->intern();
      EXPECT_EQ(internTable.size(), 1U);

      EXPECT_EQ(i4, i1);
      checkTrivialCycleInstance(*i4, *i1, 2);

      EXPECT_EQ(internTable.size(), 1U);
    }
  }
}

/// Test a 2-object cycle.
TEST_P(CyclicTest, testCycle2a) {
  auto objs = newTestInstances<RTest1>(2);

  // Create a two object cycle.
  objs[0]->p1 = objs[1].get();
  objs[0]->n1 = 10;
  objs[1]->p2 = objs[0].get();
  objs[1]->n1 = 20;

  mungeGraph(objs);
}

/// Test a one-object cycle that points to another node.
TEST_P(CyclicTest, testCycle2b) {
  auto objs = newTestInstances<RTest1>(2);

  // Create a two object cycle.
  objs[0]->p1 = objs[0].get();
  objs[0]->p2 = objs[1].get();

  auto i0 = objs[0]->intern();
  auto i1 = i0->p2->asInterned();
  EXPECT_TRUE(i0->isFullyInterned());
  EXPECT_TRUE(i0->vtable().isFrozen());
  EXPECT_TRUE(i0->isCycleMember());
  EXPECT_TRUE(i1->isFullyInterned());
  EXPECT_TRUE(i1->vtable().isFrozen());
  EXPECT_FALSE(!FORCE_IOBJ_DELEGATE && i1->isCycleMember());

  EXPECT_EQ(i0->refcountDelegate().currentRefcount(), 1U);
  EXPECT_REFCOUNT(i1, 1U);

  mungeGraph(objs);
}

/// Test an object pointing to both nodes of a 2-object cycle,
/// where the two objects in the cycle are isomorphic to each other.
TEST_P(CyclicTest, testCycle3) {
  auto objs = newTestInstances<RTest1>(3);

  objs[0]->p1 = objs[1].get();
  objs[0]->p2 = objs[2].get();
  objs[0]->n1 = 10;

  objs[1]->p1 = objs[2].get();
  objs[1]->n1 = 20;

  objs[2]->p1 = objs[1].get();
  objs[2]->n1 = 20;

  mungeGraph(objs);
}

/// Test a 4-object cycle (after interning from a 5-object cycle).
TEST_P(CyclicTest, testCycle4) {
  auto objs = newTestInstances<RTest1>(5);

  // Create a cycle where objs[1] and objs[2] are isomorphic.
  objs[0]->p1 = objs[1].get();
  objs[0]->p2 = objs[2].get();
  objs[1]->p1 = objs[3].get();
  objs[2]->p1 = objs[3].get();
  objs[3]->p1 = objs[4].get();
  objs[4]->p1 = objs[0].get();

  for (int init = 0; init < 2; ++init) {
    if (init) {
      // Try setting some non-equal fields and see if that matters.
      objs[0]->n1 = 9;
      objs[1]->n2 = 7;
      objs[2]->n2 = 7;
      objs[3]->n1 = 4;
      objs[4]->n1 = 5;
    }

    // Intern the cycle once.
    auto i0 = objs[0]->intern();

    // Only the CycleHandle should be inserted.
    EXPECT_EQ(internTable.size(), 1U);

    // Try interning starting at every object. We should always end up with
    // pointers into the interned cycle above.
    for (size_t i = 0; i < objs.size(); ++i) {
      auto& obj = objs[i];

      auto iobj = obj->intern();
      EXPECT_EQ(internTable.size(), 1U);
      EXPECT_TRUE(obj->equal(*iobj));

      EXPECT_TRUE(obj->equal(*iobj));

      // Ensure that this interned object is part of the original cycle.
      switch (i) {
        case 0:
          EXPECT_EQ(iobj, i0);
          break;
        case 1:
          EXPECT_EQ(iobj->asRObj(), i0->p1);
          EXPECT_EQ(iobj->asRObj(), i0->p2);
          break;
        case 2:
          EXPECT_EQ(iobj->asRObj(), i0->p1);
          EXPECT_EQ(iobj->asRObj(), i0->p2);
          break;
        case 3:
          EXPECT_EQ(iobj->asRObj(), i0->p1->p1);
          break;
        case 4:
          EXPECT_EQ(iobj->asRObj(), i0->p1->p1->p1);
          break;
        default:
          abort();
      }
    }

    mungeGraph(objs);
  }
}

/** Grab a number in the range from [0, n) from 'source' and remove that
 * information from 'source' for future calls.
 *
 * For example, if you called this with 2 it would grab a single bit and
 * right-shift source by 1. If you called it with 4 it would grab two bits.
 * The idea generalizes to non-powers-of-two as well.
 */
static size_t extract(size_t n, uint64_t& source) {
  const size_t ret = source % n;
  source /= n;
  return ret;
}

static double countCycleCombinations(size_t size) {
  // We need one bit per object plus two pointers per object each
  // with size + 1 things they could point to (including nullptr).
  return pow(2, size) * pow(size + 1, 2 * size);
}

static void checkCycleCombination(size_t size, uint64_t shape) {
  // Make sure there are enough bits in 'shape' to actually encode everything.
  EXPECT_LT(countCycleCombinations(size), pow(2, 64));

  auto objs = newTestInstances<RTest1>(size);

  for (auto& obj : objs) {
    obj->n1 = extract(2, shape);

    const size_t c1 = extract(size + 1, shape);
    obj->p1 = (c1 == 0) ? nullptr : objs[c1 - 1].get();

    const size_t c2 = extract(size + 1, shape);
    obj->p2 = (c2 == 0) ? nullptr : objs[c2 - 1].get();
  }

  mungeGraph(objs);

  EXPECT_EQ(internTable.size(), 0U);
}

TEST_P(CyclicTest, testBruteForce) {
  // This can run with much larger sizes, but becomes exponentially slow, so
  // we can't make that the normal behavior.
  for (size_t size = 1; size < 3; ++size) {
    // Don't try anything too crazy.
    auto fpNumCombinations = countCycleCombinations(size);
    EXPECT_LT(fpNumCombinations, 1e8);
    const uint64_t numCombinations = (uint64_t)fpNumCombinations;

    if (verbose) {
      std::cout << "Trying size " << size << " with " << numCombinations
                << " combinations..." << std::flush;
    }

    uint64_t lastPercent = -1ull;

    for (uint64_t i = 0; i < numCombinations; ++i) {
      checkCycleCombination(size, i);

      if (verbose && numCombinations > 10000) {
        uint64_t percent = (i * 100 / numCombinations);
        if (percent != lastPercent) {
          lastPercent = percent;
          std::cout << percent << "%..." << std::flush;
        }
      }
    }

    if (verbose) {
      std::cout << "done." << std::endl;
    }
  }
}

static void testRandom(size_t seed, size_t count) {
  if (verbose) {
    std::cout << "Trying " << count << " random graphs with seed " << seed
              << ": " << std::flush;
  }

  LameRandom r(seed);

  for (size_t i = 0; i < count; ++i) {
    // Choose a graph size. testBruteForce() already does all 1- and 2-node
    // graphs, so no point in testing those here.
    const uint32_t size = r.next(6) + 3;
    const uint64_t shape = r.next64((uint64_t)countCycleCombinations(size));

    checkCycleCombination(size, shape);

    if (verbose && i % 1000 == 0) {
      std::cout << i << "..." << std::flush;
    }
  }

  if (verbose) {
    std::cout << "done." << std::endl;
  }
}

TEST_P(CyclicTest, testRandom) {
  testRandom(1337, 1000);
}

TEST(TortureTest, DISABLED_testTorture) {
  // Seed the random tests based on the current time. Making this
  // nondeterministic gives us better coverage. We log the seed
  // in case we need to repro the failure.
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  LameRandom seed((uint64_t)tv.tv_sec * 1000000 + tv.tv_usec);

  while (true) {
    for (int forceCollisions = 0; forceCollisions < 2; ++forceCollisions) {
      forceFakeLocalHashCollisionsForTests(forceCollisions != 0);
      testRandom((size_t)seed.next64(), 10000);

      assertLeakCountersZero();
    }
  }
}

INSTANTIATE_TEST_CASE_P(CyclicTestInstance, CyclicTest, ::testing::Bool());

struct InternTestEnv : ::testing::Environment {
  void TearDown() override {
    assertLeakCountersZero();
  }
};
auto s_env ATTR_UNUSED = ::testing::AddGlobalTestEnvironment(new InternTestEnv);

TEST(InternUtilTest, testTOrFakePtr) {
  auto obj1 = newTestInstance<RTest1>();
  RObjOrFakePtr f1(obj1.get());
  EXPECT_FALSE(f1.isFakePtr());
  EXPECT_TRUE(f1.isPtr());
  EXPECT_EQ(f1.asPtr(), obj1.get());

  RObjOrFakePtr f2(0xDEADBEEFFEDFACEFULL);
  EXPECT_TRUE(f2.isFakePtr());
  EXPECT_FALSE(f2.isPtr());
  EXPECT_EQ(f2.asPtr(), nullptr);

  RObjOrFakePtr f1a(obj1.get());
  RObjOrFakePtr f2a(0xDEADBEEFFEDFACEFULL);

  EXPECT_EQ(f1, f1a);
  EXPECT_EQ(f2, f2a);
  EXPECT_NE(f1, f2);
}

TEST(InternUtilTest, testExtIntern) {
  // Intern a short string
  {
    String s("111");
    EXPECT_EQ(s.asLongString(), nullptr);

    auto res = SKIP_intern(RObjOrFakePtr(s.sbits()));
    SCOPE_EXIT {
      if (auto p = res.asPtr())
        decref(p);
    };

    auto s2 = String(res.bits());
    EXPECT_EQ(s, s2);
    EXPECT_EQ(s2.asLongString(), nullptr);
  }

  // Intern a long string
  {
    String s("This is a longer string that won't be short");
    EXPECT_NE(s.asLongString(), nullptr);
    EXPECT_FALSE(s.asLongString()->isInterned());

    auto res = SKIP_intern(RObjOrFakePtr(s.sbits()));
    SCOPE_EXIT {
      if (auto p = res.asPtr())
        decref(p);
    };

    auto s2 = String(res.bits());
    EXPECT_EQ(s, s2);
    EXPECT_NE(s2.asLongString(), nullptr);
    EXPECT_TRUE(s2.asLongString()->isInterned());
  }

  // Intern a generic object
  {
    auto obj = newTestInstance<RTest1>();
    obj->n3 = 123;
    EXPECT_FALSE(obj->isInterned());

    auto res = SKIP_intern(RObjOrFakePtr(obj.get()));
    SCOPE_EXIT {
      if (auto p = res.asPtr())
        decref(p);
    };

    EXPECT_TRUE(res->isInterned());
    ITest1* iobj = (ITest1*)res.asPtr();
    EXPECT_EQ(iobj->n3, 123);
  }

  EXPECT_EQ(internTable.size(), 0);
}
} // anonymous namespace

namespace {

struct MyObj : RObj {
  using MetadataType = AObjMetadata;
  WithVTable(MetadataType);
  int64_t internData;

  static Type& static_type() {
    // Compute lazily to avoid the "static initialization order fiasco".
    static auto cls = Type::factory(
        typeid(MyObj).name(),
        Type::Kind::refClass,
        sizeof(MyObj),
        {},
        nullptr,
        sizeof(MetadataType),
        std::max(sizeof(IObjMetadata), sizeof(MetadataType)));
    return *cls;
  }

  DEFINE_VTABLE()
};

TEST(InternTest, testUnequalMetadata) {
  auto& obstack = Obstack::cur();
  Obstack::PosScope pos(obstack);

  // Make two identical objects
  auto o1 = obstack.allocObject<MyObj>();
  o1->metadata()._dummy = 1;
  o1->metadata().m_arraySize = 2;
  o1->internData = 3;

  auto o2 = obstack.allocObject<MyObj>();
  o2->metadata()._dummy = 1;
  o2->metadata().m_arraySize = 2;
  o2->internData = 3;

  auto i1 = obstack.intern(o1).asPtr();
  auto i2 = obstack.intern(o2).asPtr();

  EXPECT_EQ(i1, i2);

  // Now tweak the metadata and we should still get the same interned object
  o2->metadata()._dummy = 2;
  o2->metadata().m_arraySize = 3;
  o2->internData = 3;
  i2 = obstack.intern(o2).asPtr();
  EXPECT_EQ(i1, i2);

  // But tweaking an interned field should get a different object.
  o2->metadata()._dummy = 1;
  o2->metadata().m_arraySize = 2;
  o2->internData = 4;
  i2 = obstack.intern(o2).asPtr();
  EXPECT_NE(i1, i2);
}
} // anonymous namespace
