/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "skip/fwd.h"
#include "skip/intern.h"
#include "skip/memoize.h"
#include "skip/objects.h"
#include "skip/Refcount.h"
#include "skip/set.h"
#include "skip/String.h"
#include "skip/util.h"
#include "skip/VTable.h"

#include <memory>
#include <vector>
#include <set>
#include <signal.h>
#include <gtest/gtest.h>

namespace skip {

// Needed to print skip::String within EXPECT_EQ.
::std::ostream& operator<<(::std::ostream& os, const String& s);

namespace test {

using RObjPairSet =
    skip::fast_set<std::pair<const RObj*, const RObj*>, pair_hash>;

// Each test type we create here registers an explicit function to
// recursively compare two instances of the right VTable.
using DeepComparator =
    bool (*)(const RObj& i1, const RObj& i2, RObjPairSet& seen);

bool objectsEqualHelper(const RObj* t1, const RObj* t2, RObjPairSet& seen);

/**
 * Return true iff t1 and t2 are completely isomorphic.
 *
 * NOTE: This implementation is intentionally separate from deepCompare()
 * to provide an independent version for testing.
 */
bool objectsEqual(const RObj* t1, const RObj* t2);

// Return a test bit pattern that lets us ensure no one clobbers what would
// be the next() field of a test RObj, if it existed.
IObj* testPointerPattern(IObj& p);

/// Counts how many Revisions are in the list.
size_t numRevisions(Invocation& inv);

std::unique_ptr<Type> createArrayType(
    const char* name,
    size_t slotByteSize,
    const std::vector<size_t>& slotRefOffsets,
    DeepComparator comparator);

void* initializeMetadata(void* rawStorage, const VTableRef vtable);

std::unique_ptr<Type> createClassType(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    DeepComparator comparator);

std::unique_ptr<Type> createInvocationType(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    DeepComparator comparator);

template <typename Derived, typename DerivedIObj>
struct TestRObjHelper {
  using InternType = DerivedIObj;

  skip::intrusive_ptr<DerivedIObj> intern() {
    auto derived_this = reinterpret_cast<Derived*>(this);
    auto p = reinterpret_cast<DerivedIObj*>(skip::intern(derived_this));
    return skip::intrusive_ptr<DerivedIObj>(p, false);
  }

  bool equal(const RObj& other) const {
    auto derived_this = reinterpret_cast<const Derived*>(this);
    return objectsEqual(derived_this, &other);
  }

  static void operator delete(void* p) {
    ::free(
        reinterpret_cast<char*>(p) -
        Derived::static_type().uninternedMetadataByteSize());
  }
};

template <typename Derived>
struct TestIObjHelper {
  bool equal(const RObj& other) const {
    auto derived_this = reinterpret_cast<const Derived*>(this);
    return objectsEqual(derived_this, &other);
  }

  using Ptr = skip::intrusive_ptr<const Derived>;
};

template <typename Derived>
void intrusive_ptr_add_ref(TestIObjHelper<Derived>* iobj) {
  Derived* derived = reinterpret_cast<Derived*>(iobj);
  incref(derived);
}

template <typename Derived>
void intrusive_ptr_release(TestIObjHelper<Derived>* iobj) {
  Derived* derived = reinterpret_cast<Derived*>(iobj);
  decref(derived);
}

/// Return a single test object.
template <typename T, typename... U>
std::unique_ptr<T> newTestInstance(U&&... u) {
  auto vtable = T::static_vtable();
  auto& type = T::static_type();
  const size_t metadataSize = type.uninternedMetadataByteSize();
  const size_t size = metadataSize + type.userByteSize();

  static_assert(
      !(std::is_base_of<IObj, T>::value), "T must not be a descendant of IObj");

  char* const mem = static_cast<char*>(::malloc(size));
  assert(mem != nullptr);

  memset(mem, 0, size);

  auto ptr = initializeMetadata(mem, vtable);
  auto ret = new (ptr) T(std::forward<U>(u)...);

  EXPECT_FALSE(ret->isInterned());

  return std::unique_ptr<T>(ret);
}

/// Return a vector of test objects.
template <typename T>
std::vector<std::unique_ptr<T>> newTestInstances(
    size_t count,
    std::unique_ptr<T> (*factory)() = newTestInstance<T>) {
  std::vector<std::unique_ptr<T>> ret(count);
  for (size_t i = 0; i < count; ++i)
    ret[i] = factory();
  return ret;
}

/// A horrible but deterministic random number generator for testing.
/// I have no idea what its period is.
///
/// TODO: Now that we are willing to use boost, we should just switch to
/// one of boost's PRNGs.
struct LameRandom {
  explicit LameRandom(size_t seed) : m_state(seed) {
    next();
    next();
  }

  uint32_t next() {
    m_state = hashCombine(hashCombine(m_state, 12345), 0xFEEDFACE);
    return (uint32_t)m_state;
  }

  uint32_t next(uint32_t max) {
    return next() % max;
  }

  uint64_t next64() {
    const uint64_t n = next();
    next();
    return n | ((uint64_t)next() << 32);
  }

  uint64_t next64(uint64_t max) {
    return next64() % max;
  }

  void fill(void* p, size_t sz) {
    for (; sz >= sizeof(uint64_t);
         sz -= sizeof(uint64_t), p = (char*)p + sizeof(uint64_t)) {
      *((uint64_t*)p) = next64();
    }
    for (; sz; --sz, p = (char*)p + 1) {
      *((uint8_t*)p) = (uint8_t)next();
    }
  }

  // Return true if p matches a fill() of the same size
  bool compareFill(const void* p, size_t sz) {
    while (sz > 0) {
      std::array<char, 64> buf;
      auto batch = std::min(sz, buf.size());
      fill(buf.data(), batch);
      if (memcmp(buf.data(), p, batch) != 0)
        return false;
      p = mem::add(p, batch);
      sz -= batch;
    }
    return true;
  }

 private:
  size_t m_state;
};

// Generate a set of "interesting" patterns for testing.
std::set<std::string> generateInterestingStrings();
std::set<int64_t> generateInterestingInt64s();

bool probeIsWritable(void* addr);
bool probeIsReadable(void* addr);

#define EXPECT_WRITABLE(addr) EXPECT_TRUE(::skip::test::probeIsWritable(addr))
#define EXPECT_NOT_WRITABLE(addr) \
  EXPECT_FALSE(::skip::test::probeIsWritable((void*)(addr)))

#define EXPECT_READABLE(addr) EXPECT_TRUE(::skip::test::probeIsReadable(addr))
#define EXPECT_NOT_READABLE(addr) \
  EXPECT_FALSE(::skip::test::probeIsReadable((void*)(addr)))

/**
 * Purge the invocation LRU list and return the number of purged invocations.
 */
size_t purgeLruList();
} // namespace test
} // namespace skip

// Put all the test code inside a bogus friend class so it can test
// private fields.
//
// This is a hack to not reindent the whole file.
#define PRIVATE skip::TestPrivateAccess
#define ALLOW_PRIVATE_ACCESS struct PRIVATE

#if FORCE_IOBJ_DELEGATE
// When in FORCE_IOBJ_DELEGATE mode non-delegate refcounts will return
// kCycleMemberRefcountSentinel.
#define EXPECT_REFCOUNT(obj, n)                                  \
  do {                                                           \
    EXPECT_TRUE((obj)->isCycleMember());                         \
    EXPECT_EQ((n), (obj)->refcountDelegate().currentRefcount()); \
  } while (false)
#else
#define EXPECT_REFCOUNT(obj, n) EXPECT_EQ((n), (obj)->currentRefcount())
#endif

#define DEFINE_VTABLE()                                            \
  static const VTableRef static_vtable() {                         \
    static auto singleton = RuntimeVTable::factory(static_type()); \
    static auto vtable = VTableRef(singleton->vtable());           \
    return vtable;                                                 \
  }
