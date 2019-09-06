/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "testutil.h"

#include "skip/Exception.h"
#include "skip/external.h"
#include "skip/map.h"
#include "skip/plugin-extc.h"
#include "skip/System.h"

#include <gtest/gtest.h>

#include <folly/init/Init.h>

#include <limits>
#include <fcntl.h>

namespace skip {

namespace {
struct Exception : RObj {
  const String m_what;

  Exception(String what) : m_what(what) {}

  static const VTableRef static_vtable();
  static Type& static_type();
};

const VTableRef Exception::static_vtable() {
  static auto singleton = RuntimeVTable::factory(static_type());
  static VTableRef vtable{singleton->vtable()};
  return vtable;
}

Type& Exception::static_type() {
  static auto singleton = Type::classFactory(
      typeid(Exception).name(),
      sizeof(Exception),
      {offsetof(Exception, m_what)});
  return *singleton;
}
} // namespace

namespace test {

static skip::fast_map<Type*, DeepComparator> comparators;

IObj* testPointerPattern(IObj& p) {
  uintptr_t n = reinterpret_cast<uintptr_t>(&p);
  n ^= (uintptr_t)0xDEADBEEFFEDFACEFULL;
  return reinterpret_cast<IObj*>(n);
}

bool objectsEqualHelper(const RObj* t1, const RObj* t2, RObjPairSet& seen) {
  if (t1 == t2) {
    return true;
  }

  if (t1 == nullptr || t2 == nullptr) {
    return false;
  }

  if (t1->vtable() != t2->vtable()) {
    return false;
  }

  if (!seen.insert({t1, t2}).second) {
    // Already checking this pair, don't bother looking again.
    return true;
  }

  auto it = comparators.find(&t1->type());
  EXPECT_NE(it, comparators.end());

  // Delegate the details comparator for this specific VTable.
  // We do this rather than writing something generic looking at eachValidRef(),
  // purely for testing purposes.
  return it->second(*t1, *t2, seen);
}

bool objectsEqual(const RObj* t1, const RObj* t2) {
  RObjPairSet seen;
  return objectsEqualHelper(t1, t2, seen);
}

size_t numRevisions(Invocation& inv) {
  Revision* prev = nullptr;
  size_t len = 0;

  for (auto r = inv.m_headValue.ptr(); r != nullptr; r = r->m_next.ptr()) {
    EXPECT_EQ(r->m_prev.ptr(), prev);
    prev = r;
    ++len;
  }

  EXPECT_EQ(inv.m_tailValue.ptr(), prev);

  return len;
}

std::unique_ptr<Type> createClassType(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    DeepComparator comparator) {
  auto cls = Type::classFactory(name, userByteSize, refOffsets);
  comparators.insert({cls.get(), comparator});
  return cls;
}

std::unique_ptr<Type> createInvocationType(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    DeepComparator comparator) {
  auto cls = Type::invocationFactory(name, userByteSize, refOffsets);
  comparators.insert({cls.get(), comparator});
  return cls;
}

std::unique_ptr<Type> createArrayType(
    const char* name,
    size_t slotByteSize,
    const std::vector<size_t>& slotRefOffsets,
    DeepComparator comparator) {
  auto cls = Type::arrayFactory(name, slotByteSize, slotRefOffsets);
  comparators.insert({cls.get(), comparator});
  return cls;
}

void* initializeMetadata(void* rawStorage, const VTableRef vtable) {
  const size_t metadataSize = vtable.type().uninternedMetadataByteSize();
  void* const pThis = mem::add(rawStorage, metadataSize);

  // Initialize the metadata.
  RObj& robj = *static_cast<RObj*>(pThis);
  new (&robj.metadata()) RObjMetadata(vtable);
  return pThis;
}

namespace {
std::string randomString(LameRandom& rand, size_t len) {
  std::string s(len, 'x');
  for (size_t i = 0; i < len; ++i) {
    s[i] = (char)(rand.next(127 - 32) + 32);
  }
  return s;
}
} // namespace

std::set<std::string> generateInterestingStrings() {
  LameRandom rand(123);
  std::set<std::string> unique;
  unique.insert("");

  static const char* EURO = "\xE2\x82\xAC";

  for (size_t i = 7; i <= 9; ++i) {
    unique.insert(randomString(rand, i));
    for (size_t j = 0; j < i; ++j) {
      unique.insert(
          randomString(rand, j) + 'a' + randomString(rand, (i - 1) - j));
    }
  }

  for (size_t i = 7; i <= 9; ++i) {
    for (size_t j = 0; j < i - 2; ++j) {
      unique.insert(
          randomString(rand, j) + EURO + randomString(rand, (i - 3) - j));
    }
  }

  for (size_t i = 0; i < 9; ++i) {
    for (size_t j = 0; j < 9; ++j) {
      unique.insert(randomString(rand, i) + '\0' + randomString(rand, j));
    }
  }

  unique.insert("abcabc");
  unique.insert("abcabd");
  unique.insert("abcabcabc");
  unique.insert("abcabcabd");
  return unique;
}

std::set<int64_t> generateInterestingInt64s()
// TODO: T26263524 fix signed-integer-overflow undefined behavior
#if defined(__has_feature)
#if __has_feature(__address_sanitizer__)
    __attribute__((__no_sanitize__("signed-integer-overflow")))
#endif
#endif
{
  std::vector<int64_t> values{
      0, 9223372036854775807LL, -9223372036854775807LL - 1};

  for (size_t i = 0, val = 1; i < 63; ++i, val *= 2)
    values.push_back(val);
  for (size_t i = 0, val = 1; i < 20; ++i, val *= 10)
    values.push_back(val);

  std::set<int64_t> result;
  for (auto v : values) {
    result.insert(v - 1);
    result.insert(v);
    result.insert(v + 1);
    result.insert(-v - 1);
    result.insert(-v);
    result.insert(-v + 1);
  }
  return result;
}

struct FdPipe {
  FdPipe() {
    int res = pipe(m_pipe);
    if (res == -1)
      errnoFatal("pipe() failed");
  }

  ~FdPipe() {
    if (m_pipe[0] != -1) {
      close(m_pipe[0]);
      close(m_pipe[1]);
    }
    m_pipe[0] = -1;
    m_pipe[1] = -1;
  }

  FdPipe(const FdPipe&) = delete;
  FdPipe& operator=(const FdPipe&) = delete;

  int fd_read() {
    return m_pipe[0];
  }
  int fd_write() {
    return m_pipe[1];
  }

 private:
  int m_pipe[2];
};

static FdPipe& probePipe() {
  static FdPipe pipe;
  return pipe;
}

bool probeIsReadable(void* addr) {
  // Can't use /dev/null because the kernel "knows" it will always work.
  int res = write(probePipe().fd_write(), addr, 1);
  if (res == -1) {
    if (errno == EFAULT)
      return false;
    errnoFatal("write() failed");
  }
  char tmp;
  res = read(probePipe().fd_read(), &tmp, 1);
  if (res == -1) {
    errnoFatal("read() failed");
  }
  return true;
}

bool probeIsWritable(void* addr) {
  // Check that we can read from it (on x86 we can't have a write-only address
  // space).
  int res = write(probePipe().fd_write(), addr, 1);
  if (res == -1) {
    if (errno == EFAULT)
      return false;
    errnoFatal("write() failed");
  }

  // Now try to write to it.
  res = read(probePipe().fd_read(), addr, 1);
  if (res == -1) {
    if (errno == EFAULT)
      return true;
    errnoFatal("read() failed");
  }

  return true;
}

size_t purgeLruList() {
  size_t count;
  for (count = 0; discardLeastRecentlyUsedInvocation(); ++count) {
    // Do nothing.
    /* sleep override */
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  }
  return count;
}
} // namespace test

::std::ostream& operator<<(::std::ostream& os, const String& s) {
  String::DataBuffer buf;
  for (auto ch : s.slice(buf)) {
    if (isprint(ch)) {
      os << ch;
    } else {
      os << "\\x"
         << "0123456789ABCDEF"[(ch >> 4) & 15] << "0123456789ABCDEF"[ch & 15];
    }
  }
  return os;
}
} // namespace skip

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  folly::init(&argc, &argv, true);
  skip::initializeSkip(argc, argv, false);
  skip::initializeThreadWithPermanentProcess();
  return RUN_ALL_TESTS();
}

extern "C" {

skip::RObj* SKIP_makeRuntimeError(skip::String message) {
  return skip::Obstack::cur().allocObject<skip::Exception>(message);
}

skip::String SKIP_getExceptionMessage(skip::RObj* exc) {
  assert(exc->vtable() == skip::Exception::static_vtable());
  return static_cast<skip::Exception*>(exc)->m_what;
}

void SKIP_throwRuntimeError(skip::String message) {
  skip::String::CStrBuffer buf;
  skip::fatal("Called mocked SKIP_throwRuntimeError", message.c_str(buf));
}

skip::RObj* SKIP_createStringVector(int64_t size) {
  using namespace skip;
  // Can't use Obstack::allocObject() because that doesn't take into account
  // the element count.
  static auto type = test::createArrayType(
      "string array",
      sizeof(String),
      {0},
      [](const RObj& r1, const RObj& r2, test::RObjPairSet& seen) {
        return false;
      });
  static auto vtable = RuntimeVTable::factory(*type);

  const size_t metadataSize = type->uninternedMetadataByteSize();
  const size_t byteSize = metadataSize + type->userByteSize() * size;
  void* const mem = Obstack::cur().calloc(byteSize);
  AObj<String>* obj =
      reinterpret_cast<AObj<String>*>(mem::add(mem, metadataSize));
  obj->metadata().m_arraySize = size;
  obj->metadata().m_vtable = VTableRef(vtable->vtable());
  return obj;
}

void SKIP_throwInvariantViolation(skip::String /*msg*/) {
  abort();
}

skip::RObj* SKIP_createIntVector(int64_t /*size*/) {
  abort();
}

skip::RObj* SKIP_createMixedBool(bool /*value*/) {
  abort();
}
skip::RObj* SKIP_createMixedFloat(double /*value*/) {
  abort();
}
skip::RObj* SKIP_createMixedInt(int64_t /*value*/) {
  abort();
}
skip::RObj* SKIP_createMixedNull(void) {
  abort();
}
skip::RObj* SKIP_createMixedString(skip::String /*value*/) {
  abort();
}

skip::RObj* SKIP_createMixedDict(int64_t /*capacity*/) {
  abort();
}
void SKIP_MixedDict_set(
    skip::RObj* /*obj*/,
    skip::String /*key*/,
    skip::RObj* /*value*/) {
  abort();
}
skip::RObj* SKIP_MixedDict_freeze(skip::RObj* /*obj*/) {
  abort();
}

skip::RObj* SKIP_createMixedVec(int64_t /*capacity*/) {
  abort();
}
void SKIP_MixedVec_push(skip::RObj* /*obj*/, skip::RObj* /*value*/) {
  abort();
}
skip::RObj* SKIP_MixedVec_freeze(skip::RObj* /*obj*/) {
  abort();
}
SkipRetValue SKIP_createTupleFromMixed(skip::RObj*) {
  abort();
}

void SKIP_awaitableResume(SkipRObj* /*awaitable*/) {
  abort();
}

void SKIP_awaitableFromMemoValue(
    const skip::MemoValue* /*mv*/,
    skip::Awaitable* /*awaitable*/) {
  abort();
}

void SKIP_awaitableToMemoValue(
    skip::MemoValue* /*mv*/,
    skip::Awaitable* /*awaitable*/) {
  abort();
}

SkipRetValue SKIPC_iteratorNext(skip::RObj* /*iterator*/) {
  abort();
}

skip::RObj* SKIP_unsafeCreateSubprocessOutput(
    int64_t /*returnCode*/,
    skip::RObj* /*stdout*/,
    skip::RObj* /*stderr*/) {
  abort();
}

skip::RObj* SKIP_UInt8Array_create(int64_t /*capacity*/) {
  abort();
}

size_t SKIPC_buildHash() {
  return 0x123456789ABCDEFULL;
}
} // extern "C"
