/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>

#include "skip/plugin/hhvm.h"

#include <boost/format.hpp>
#include <boost/intrusive_ptr.hpp>

#include <folly/Format.h>

// Since we're running a test here force assert to always check
#undef assert
#define assert(chk) assert_(__FILE__, __LINE__, (chk), #chk)

namespace {

void assert_(const char* file, int line, bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "%s@%d: Assertion failed! %s\n", file, line, what);
  }
}

FOLLY_MAYBE_UNUSED void fail(const char* msg, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));
FOLLY_MAYBE_UNUSED void fail(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  abort();
}

FOLLY_MAYBE_UNUSED SkipRetValue ret_garbage() {
  SkipRetValue v;
  memset(&v, 0xcd, sizeof(v));
  return v;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_void() {
  return ret_garbage();
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_null() {
  auto ret = ret_garbage();
  ret.type = RetType::null;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_bool(SkipBool b) {
  auto ret = ret_garbage();
  ret.value.m_boolean = b;
  ret.type = RetType::boolean;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_int64(SkipInt i) {
  auto ret = ret_garbage();
  ret.value.m_int64 = i;
  ret.type = RetType::int64;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_float64(SkipFloat f) {
  auto ret = ret_garbage();
  ret.value.m_float64 = f;
  ret.type = RetType::float64;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_string(const std::string& s) {
  auto ret = ret_garbage();
  ret.value.m_string = skip::String{s};
  ret.type = RetType::string;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_object(skip::HhvmHandle* o) {
  auto ret = ret_garbage();
  ret.value.m_object = o;
  ret.type = RetType::object;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_array(skip::HhvmHandle* a) {
  auto ret = ret_garbage();
  ret.value.m_array = a;
  ret.type = RetType::array;
  return ret;
}

#ifdef TESTHELPER_FAKE_OBJECTS

struct FakeHhvmHeapObject {
  using Ptr = boost::intrusive_ptr<FakeHhvmHeapObject>;

  const bool m_isObj;
  size_t m_refcount = 0;
  size_t m_hhvmRefCount = 0;

  FakeHhvmHeapObject(bool isObj) : m_isObj(isObj) {}

  virtual ~FakeHhvmHeapObject() {
    assert(m_refcount == 0);
    assert(m_hhvmRefCount == 0);
  }

  virtual std::string strValue() const = 0;

  static Ptr fromC(skip::HhvmHandle* handle) {
    return reinterpret_cast<FakeHhvmHeapObject*>(handle->heapObject());
  }
};

void intrusive_ptr_release(FakeHhvmHeapObject* p) {
  --p->m_refcount;
  if (p->m_refcount + p->m_hhvmRefCount == 0) {
    delete p;
  }
}
void intrusive_ptr_add_ref(FakeHhvmHeapObject* p) {
  ++p->m_refcount;
}

struct FakeHhvmObjectData : FakeHhvmHeapObject {
  using Ptr = boost::intrusive_ptr<FakeHhvmObjectData>;

  std::string m_name;

  explicit FakeHhvmObjectData(const std::string& name)
      : FakeHhvmHeapObject(true), m_name(name) {}

  static Ptr fromC(skip::HhvmHandle* handle) {
    return boost::static_pointer_cast<FakeHhvmObjectData>(
        FakeHhvmHeapObject::fromC(handle));
  }

  static Ptr make(std::string name) {
    return FakeHhvmObjectData::Ptr(new FakeHhvmObjectData{name});
  }

  std::string strValue() const override {
    return "object<" + m_name + ">";
  }

  HhvmObjectDataPtr toC() {
    return reinterpret_cast<HhvmObjectDataPtr>(this);
  }

  SkipRetValue report() const {
    return svmi_string(
        (boost::format("<object:%s(#%d)>") % m_name % m_hhvmRefCount).str());
  }
};

struct FakeHhvmArrayData : FakeHhvmHeapObject {
  using Ptr = boost::intrusive_ptr<FakeHhvmArrayData>;

  std::string m_name;
  int64_t m_size;

  explicit FakeHhvmArrayData(const std::string& name, int64_t size = 0)
      : FakeHhvmHeapObject(false), m_name(name), m_size(size) {}

  static Ptr fromC(skip::HhvmHandle* handle) {
    return boost::static_pointer_cast<FakeHhvmArrayData>(
        FakeHhvmHeapObject::fromC(handle));
  }

  static Ptr make(std::string name, int64_t size = 0) {
    return FakeHhvmArrayData::Ptr(new FakeHhvmArrayData{name, size});
  }

  std::string strValue() const override {
    return "array<" + m_name + ">";
  }

  HhvmArrayDataPtr toC() {
    return reinterpret_cast<HhvmArrayDataPtr>(this);
  }

  SkipRetValue report() const {
    return svmi_string(
        (boost::format("<array:%s(#%d)>") % m_name % m_hhvmRefCount).str());
  }
};

struct FakeHhvmObject {
  FakeHhvmObjectData::Ptr object;

  HhvmObjectPtr toC() {
    return reinterpret_cast<HhvmObjectPtr>(this);
  }
  static FakeHhvmObject* fromC(HhvmVariant* p) {
    return reinterpret_cast<FakeHhvmObject*>(p);
  }
};

using HhvmObjectRet = HhvmObject*;

struct FakeHhvmObjectRet {
  FakeHhvmObjectData::Ptr m_object;

  HhvmObjectRet toC() {
    return reinterpret_cast<HhvmObjectRet>(this);
  }
  static FakeHhvmObjectRet* fromC(HhvmObjectRet p) {
    return reinterpret_cast<FakeHhvmObjectRet*>(p);
  }
};

struct FakeHhvmArray {
  FakeHhvmArrayData::Ptr object;

  HhvmArrayPtr toC() {
    return reinterpret_cast<HhvmArrayPtr>(this);
  }
  static FakeHhvmArray* fromC(HhvmVariant* p) {
    return reinterpret_cast<FakeHhvmArray*>(p);
  }
};

using HhvmArrayRet = HhvmArray*;

struct FakeHhvmArrayRet {
  FakeHhvmArrayData::Ptr m_array;

  HhvmArrayRet toC() {
    return reinterpret_cast<HhvmArrayRet>(this);
  }
  static FakeHhvmArrayRet* fromC(HhvmArrayRet p) {
    return reinterpret_cast<FakeHhvmArrayRet*>(p);
  }
};

struct FakeHhvmString {
  std::string m_str;

  explicit FakeHhvmString(const std::string& s) : m_str(s) {}

  HhvmString toC() {
    return reinterpret_cast<HhvmString>(this);
  }

  static FakeHhvmString* fromC(HhvmString p) {
    return reinterpret_cast<FakeHhvmString*>(p);
  }
};

using HhvmStringRet = HhvmString*;

struct FakeHhvmStringRet {
  std::string m_str;

  HhvmStringRet toC() {
    return reinterpret_cast<HhvmStringRet>(this);
  }

  static FakeHhvmStringRet* fromC(HhvmStringRet p) {
    return reinterpret_cast<FakeHhvmStringRet*>(p);
  }
};

#endif // TESTHELPER_FAKE_OBJECTS

__attribute__((__used__)) std::string repr(SkipRetValue v) {
  switch (v.type) {
    case RetType::null:
      return "null";
    case RetType::boolean:
      return v.value.m_boolean ? "true" : "false";
    case RetType::int64:
      return folly::to<std::string>(v.value.m_int64);
    case RetType::float64:
      return folly::to<std::string>(v.value.m_float64);
    case RetType::string:
      return "\"" + skip::String{v.value.m_string}.toCppString() + "\"";
    case RetType::object:
#ifdef TESTHELPER_FAKE_OBJECTS
      return FakeHhvmObjectData::fromC(v.value.m_object)->strValue();
#else
      return "object";
#endif
    case RetType::array:
#ifdef TESTHELPER_FAKE_OBJECTS
      return FakeHhvmArrayData::fromC(v.value.m_array)->strValue();
#else
      return "array";
#endif
    default:
      return folly::format("unknown type: {}", (int)v.type).str();
  }
}

const svmi::Class& lookupClassByIndex(SkipInt classId) {
  static auto table = SKIPC_hhvmTypeTable();
  return *table->classes->at(classId);
}

skip::fast_map<std::string, size_t> computeClassMapping(
    const skip::AObj<svmi::Class*>* classes) {
  skip::fast_map<std::string, size_t> mapping;
  for (size_t i = 0; i < classes->arraySize(); ++i) {
    auto* cls = classes->at(i);
    mapping.insert(std::make_pair(cls->name.toCppString(), i));
  }
  return mapping;
}

ATTR_UNUSED const size_t lookupClassIndexByName(std::string className) {
  static auto mapping = computeClassMapping(SKIPC_hhvmTypeTable()->classes);
  auto i = mapping.find(className);
  assert(i != mapping.end());
  return i->second;
}

std::vector<const svmi::Field*> computeFieldMapping(
    const skip::AObj<svmi::Class*>* classes,
    bool wantShape) {
  std::vector<const svmi::Field*> mapping;
  for (int i = 0; i < classes->arraySize(); ++i) {
    auto* cls = classes->at(i);
    if (cls->kind == svmi::ClassKind::base)
      continue;
    bool isShape =
        ((cls->kind == svmi::ClassKind::copyShape) ||
         (cls->kind == svmi::ClassKind::proxyShape));
    if (isShape != wantShape)
      continue;
    for (int j = 0; j < cls->fields->arraySize(); ++j) {
      auto* fld = cls->fields->at(j);
      mapping.push_back(fld);
    }
  }
  return mapping;
}

ATTR_UNUSED const svmi::Field& lookupObjectFieldByIndex(SkipInt slot) {
  static auto mapping =
      computeFieldMapping(SKIPC_hhvmTypeTable()->classes, false);
  assert(slot < mapping.size());
  return *mapping[slot];
}

ATTR_UNUSED const svmi::Field& lookupShapeFieldByIndex(SkipInt slot) {
  static auto mapping =
      computeFieldMapping(SKIPC_hhvmTypeTable()->classes, true);
  assert(slot < mapping.size());
  return *mapping[slot];
}
} // anonymous namespace

#ifdef TESTHELPER_FAKE_OBJECTS

extern "C" {

skip::String SKIP_HHVM_Object_getType(skip::HhvmHandle* wrapper) {
  return skip::String{FakeHhvmObjectData::fromC(wrapper)->m_name};
}

void SKIP_HHVM_incref(skip::HhvmHandle* wrapper) {
  auto obj = FakeHhvmHeapObject::fromC(wrapper);
  ++obj->m_hhvmRefCount;
}

void SKIP_HHVM_decref(skip::HhvmHandle* wrapper) {
  auto obj = FakeHhvmHeapObject::fromC(wrapper);
  --obj->m_hhvmRefCount;
}

skip::HhvmHandle* SKIP_HHVM_Object_create(skip::String type) {
  printf("SKIP_HHVM_Object_create(%s)\n", type.toCppString().c_str());
  auto od = FakeHhvmObjectData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

skip::HhvmHandle* SKIP_HHVM_Shape_create(skip::String type) {
  printf("SKIP_HHVM_Shape_create(%s)\n", type.toCppString().c_str());
  auto od = FakeHhvmArrayData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

skip::HhvmHandle* SKIP_HhvmInterop_createArrayFromFixedVector(
    skip::String kind,
    SkipInt size,
    skip::RObj* iterator) {
  skip::String::CStrBuffer buf;
  printf("SKIP_HhvmInterop_createArrayFromFixedVector<%s>[", kind.c_str(buf));
  for (auto i = 0; i < size; ++i) {
    if (i != 0)
      printf(", ");
    SkipRetValue next = SKIPC_iteratorNext(iterator);
    printf("%s", repr(next).c_str());
  }
  printf("]\n");
  auto array = FakeHhvmArrayData::make(kind.toCppString(), size);
  return SKIP_Obstack_wrapHhvmHeapObject(array->toC());
}

skip::HhvmHandle* SKIP_HhvmInterop_ObjectCons_create(SkipInt classId) {
  auto& e = lookupClassByIndex(classId);
  printf(
      "SKIP_HhvmInterop_ObjectCons_create(%s)\n", e.name.toCppString().c_str());
  auto od = FakeHhvmObjectData::make(e.name.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

void SKIP_HhvmInterop_ObjectCons_finish(skip::HhvmHandle* obj) {
  printf(
      "SKIP_HhvmInterop_ObjectCons_finish(%s)\n",
      FakeHhvmObjectData::fromC(obj)->strValue().c_str());
}

void SKIP_HhvmInterop_ObjectCons_setFieldMixed(
    skip::HhvmHandle* obj,
    SkipInt slot,
    SkipRetValue value) {
  printf(
      "SKIP_HhvmInterop_ObjectCons_setFieldMixed(%s, %s, %s)\n",
      FakeHhvmObjectData::fromC(obj)->strValue().c_str(),
      lookupObjectFieldByIndex(slot).name.toCppString().c_str(),
      repr(value).c_str());
}

skip::HhvmHandle* SKIP_HhvmInterop_ShapeCons_create(SkipInt classId) {
  auto& e = lookupClassByIndex(classId);
  printf(
      "SKIP_HhvmInterop_ShapeCons_create(%s)\n", e.name.toCppString().c_str());
  auto od = FakeHhvmArrayData::make(e.name.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

void SKIP_HhvmInterop_ShapeCons_finish(skip::HhvmHandle* obj) {
  printf(
      "SKIP_HhvmInterop_ShapeCons_finish(%s)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str());
}

void SKIP_HhvmInterop_ShapeCons_setFieldMixed(
    skip::HhvmHandle* obj,
    SkipInt slot,
    SkipRetValue value) {
  printf(
      "SKIP_HhvmInterop_ShapeCons_setFieldMixed(%s, %s, %s)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str(),
      lookupShapeFieldByIndex(slot).name.toCppString().c_str(),
      repr(value).c_str());
}
} // extern "C"

#endif // TESTHELPER_FAKE_OBJECTS

#ifdef TESTHELPER_VALUES

namespace {

std::deque<SkipRetValue> s_pendingResults;
int64_t s_resultLock = 0;

SkipRetValue nextValue() {
  if (s_pendingResults.empty()) {
    fprintf(stderr, "expected s_pendingResults to not be empty\n");
    abort();
  }
  auto next = s_pendingResults.front();
  if (!s_resultLock) {
    s_pendingResults.pop_front();
  }
  return next;
}

__attribute__((__used__)) SkipRetValue nextValue(RetType expectedType) {
  auto next = nextValue();
  if (next.type != expectedType) {
    fprintf(
        stderr,
        "Invalid type - expected %d but got %d\n",
        (int)expectedType,
        (int)next.type);
    abort();
  }
  return next;
}

std::vector<int64_t> s_gatherBuffer;

ATTR_UNUSED void pushGatherBool(bool b) {
  s_gatherBuffer.push_back(b ? 1 : 0);
}
ATTR_UNUSED void pushGatherInt(int64_t i) {
  s_gatherBuffer.push_back(i);
}
ATTR_UNUSED void pushGatherFloat(double d) {
  union {
    double d;
    int64_t i;
  } u;
  u.d = d;
  s_gatherBuffer.push_back(u.i);
}
ATTR_UNUSED void pushGatherString(std::string s) {
  s_gatherBuffer.push_back(skip::String{s}.m_sbits);
}
ATTR_UNUSED void pushGatherHandle(skip::HhvmHandle* h) {
  union {
    skip::HhvmHandle* h;
    intptr_t i;
  } u;
  u.h = h;
  s_gatherBuffer.push_back(u.i);
}
} // anonymous namespace

extern "C" {

void SKIP_internalPushRet(skip::RObj* value);
void SKIP_internalPushRet(skip::RObj* value) {
  s_pendingResults.push_back(SKIP_createTupleFromMixed(value));
}

void SKIP_internalPushRetUndefined();
void SKIP_internalPushRetUndefined() {
  SkipRetValue value;
  value.type = RetType::null;
  value.value.m_int64 = -1;
  s_pendingResults.push_back(value);
}

void SKIP_checkRetUsed();
void SKIP_checkRetUsed() {
  if (!s_pendingResults.empty()) {
    fprintf(stderr, "expected s_pendingResults to be empty\n");
    abort();
  }
}

void SKIP_internalPushRet_obj(skip::HhvmHandle* value);
void SKIP_internalPushRet_obj(skip::HhvmHandle* value) {
  s_pendingResults.push_back(svmi_object(value));
}

void SKIP_internalPushRet_arr(skip::HhvmHandle* value);
void SKIP_internalPushRet_arr(skip::HhvmHandle* value) {
  s_pendingResults.push_back(svmi_array(value));
}

void SKIP_internalSetRet(skip::RObj* value);
void SKIP_internalSetRet(skip::RObj* value) {
  s_pendingResults.push_front(SKIP_createTupleFromMixed(value));
  s_resultLock++;
}

void SKIP_internalUnsetRet();
void SKIP_internalUnsetRet() {
  s_pendingResults.pop_front();
  s_resultLock--;
}

SkipRetValue SKIP_HHVM_MaybeConvertToArray(SkipRetValue obj) {
  return obj;
}

SkipGatherData SKIP_HhvmInterop_Gather_gatherCollect(
    void* objectData,
    SkipInt classId) {
  printf(
      "SKIP_HhvmInterop_Gather_gatherCollect(%s, %s)\n",
      (*reinterpret_cast<FakeHhvmHeapObject**>(objectData))->strValue().c_str(),
      lookupClassByIndex(classId).name.toCppString().c_str());

  // pad with crazyness just in case we go over
  s_gatherBuffer.resize(s_gatherBuffer.size() + 1024, -1);
  return SkipGatherData{&s_gatherBuffer, s_gatherBuffer.data()};
}

void SKIP_HhvmInterop_Gather_gatherCleanup(void* handle) {
  printf("SKIP_HhvmInterop_Gather_gatherCleanup()\n");
  assert(handle == &s_gatherBuffer);
  s_gatherBuffer.clear();
}
} // extern "C"

#endif // TESTHELPER_VALUES

#if defined(TESTHELPER_FAKE_OBJECTS) && defined(TESTHELPER_VALUES)

namespace SKIP {
namespace Vector {
namespace HH_varray {

template <>
SkipInt size(skip::HhvmHandle* obj) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("SKIP::Vector::HH_varray::size(%s)\n", data->strValue().c_str());
  return data->m_size;
}
} // namespace HH_varray
} // namespace Vector

namespace Map {
namespace HH_darray {

template <>
SkipInt iterBegin<>(skip::RObj*, skip::HhvmHandle* obj) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("HH_darray::iterBegin(%s)\n", data->strValue().c_str());
  return 0;
}

template <>
SkipInt iterEnd<>(skip::RObj*, skip::HhvmHandle* obj) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("HH_darray::iterEnd(%s)\n", data->strValue().c_str());
  return data->m_size;
}

template <>
SkipInt iterAdvance<>(skip::RObj*, skip::HhvmHandle* obj, SkipInt it) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("HH_darray::iterAdvance(%s, %lld)\n", data->strValue().c_str(), it);
  return it + 1;
}

template <>
SkipRetValue iterGetKey<>(skip::RObj*, skip::HhvmHandle* obj, SkipInt it) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("HH_darray::iterGetKey(%s, %lld)\n", data->strValue().c_str(), it);
  return nextValue();
}

template <>
SkipRetValue iterGetValue<>(skip::RObj*, skip::HhvmHandle* obj, SkipInt it) {
  auto data = FakeHhvmArrayData::fromC(obj);
  printf("HH_darray::iterGetValue(%s, %lld)\n", data->strValue().c_str(), it);
  return nextValue();
}

template <>
skip::RObj* internalCreate(skip::HhvmArrayFactory factory, skip::RObj*) {
  printf("HH_darray::internalCreate()\n");
  auto od = FakeHhvmArrayData::make("darray", 0);
  auto handle = skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
  return factory(handle);
}

template <>
void internalSet(
    skip::RObj*,
    skip::HhvmHandle* obj,
    SkipRetValue key,
    SkipRetValue value) {
  printf(
      "HH_darray::internalSet(%s, %s, %s)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str(),
      repr(key).c_str(),
      repr(value).c_str());
}
} // namespace HH_darray
} // namespace Map

namespace Set {
namespace HH_keyset {

template <>
skip::RObj* internalCreate(skip::HhvmArrayFactory factory, skip::RObj*) {
  printf("HH_keyset::internalCreate()\n");
  auto od = FakeHhvmArrayData::make("keyset", 0);
  auto handle = skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
  return factory(handle);
}

template <>
void internalSet(skip::RObj*, skip::HhvmHandle* obj, SkipRetValue key) {
  printf(
      "HH_keyset::internalSet(%s, %s)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str(),
      repr(key).c_str());
}
} // namespace HH_keyset
} // namespace Set
} // namespace SKIP

#endif // defined(TESTHELPER_FAKE_OBJECTS) && defined(TESTHELPER_VALUES)
