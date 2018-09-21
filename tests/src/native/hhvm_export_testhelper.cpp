/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/String.h"
#include "skip/external.h"
#include "skip/plugin-extc.h"
#include "skip/plugin/hhvm.h"
#include "skip/util.h"

#include <cstdarg>
#include <limits>
#include <boost/format.hpp>
#include <boost/intrusive_ptr.hpp>

#define TESTHELPER_FAKE_OBJECTS
#define TESTHELPER_VALUES
#include "testhelper_common.h"

using namespace skip;

namespace skiptest {
extern svmi::FunctionSignature* globals;
}

namespace {

std::string arrayToId(skip::HhvmHandle* obj) {
  return FakeHhvmHeapObject::fromC(obj)->strValue();
}

enum FromNull {};
enum FromBool {};
enum FromInt {};
enum FromFloat {};
enum FromString {};
enum FromObject {};
enum FromArray {};

struct FakeHhvmVariant {
  enum class Kind {
    undef,
    object,
    null,
    boolean,
    int64,
    float64,
    string,
    array,
  };

  Kind m_kind;
  SkipBool m_value_b;
  SkipInt m_value_i;
  SkipFloat m_value_f;
  std::string m_value_s;
  FakeHhvmObjectData::Ptr m_value_o;
  FakeHhvmArrayData::Ptr m_value_a;

  ~FakeHhvmVariant() = default;

  explicit FakeHhvmVariant() : m_kind(Kind::undef) {}
  explicit FakeHhvmVariant(FromNull) : m_kind(Kind::null) {}
  FakeHhvmVariant(FromBool, SkipBool b) : FakeHhvmVariant() {
    setBool(b);
  }
  FakeHhvmVariant(FromInt, SkipInt i) : FakeHhvmVariant() {
    setInt64(i);
  }
  FakeHhvmVariant(FromFloat, SkipFloat f) : FakeHhvmVariant() {
    setFloat64(f);
  }
  FakeHhvmVariant(FromString, const std::string& s) : FakeHhvmVariant() {
    setString(s);
  }
  FakeHhvmVariant(FromObject, FakeHhvmObjectData::Ptr o) : FakeHhvmVariant() {
    setObject(o);
  }
  FakeHhvmVariant(FromArray, FakeHhvmArrayData::Ptr a) : FakeHhvmVariant() {
    setArray(a);
  }
  FakeHhvmVariant(const FakeHhvmVariant&) = delete;

  //  static FakeHhvmVariant::Ptr fromC(skip::HhvmHandle* handle) {
  //    return reinterpret_cast<FakeHhvmVariant*>(
  //      HhvmHandle::fromC(handle)->m_heapObject);
  //  }

  static FakeHhvmVariant* fromC(HhvmVariant* handle) {
    return reinterpret_cast<FakeHhvmVariant*>(handle);
  }

  HhvmVariant* toC() {
    return reinterpret_cast<HhvmVariant*>(this);
  }

  void setUndef() {
    m_kind = Kind::undef;
  }
  void setNull() {
    m_kind = Kind::null;
  }
  void setBool(SkipBool b) {
    m_kind = Kind::boolean;
    m_value_b = b;
  }
  void setInt64(SkipInt i) {
    m_kind = Kind::int64;
    m_value_i = i;
  }
  void setFloat64(SkipFloat f) {
    m_kind = Kind::float64;
    m_value_f = f;
  }
  void setString(const std::string& s) {
    m_kind = Kind::string;
    m_value_s = s;
  }
  void setObject(FakeHhvmObjectData::Ptr o) {
    m_kind = Kind::object;
    m_value_o = o;
  }
  void setArray(FakeHhvmArrayData::Ptr a) {
    m_kind = Kind::array;
    m_value_a = a;
  }

  HhvmVariant* toVariantRet() {
    return reinterpret_cast<HhvmVariant*>(this);
  }

  std::string strValue() const {
    switch (m_kind) {
      case Kind::undef:
        return "variant#undef";
      case Kind::object:
        return "variant#object(" +
            (m_value_o ? m_value_o->strValue() : "<null>") + ")";
      case Kind::array:
        return "variant#array(" +
            (m_value_a ? m_value_a->strValue() : "<null>") + ")";
      case Kind::null:
        return "variant#null";
      case Kind::boolean:
        return (boost::format("variant#bool(%s)") %
                (m_value_b ? "true" : "false"))
            .str();
      case Kind::int64:
        return (boost::format("variant#int(%d)") % m_value_i).str();
      case Kind::float64:
        return (boost::format("variant#float(%f)") % m_value_f).str();
      case Kind::string:
        return "variant#string(" + m_value_s + ")";
      default:
        abort();
    }
  }

  SkipRetValue report() const {
    return svmi_string(strValue());
  }
};

template <typename T>
T* getExportedFunction(
    const std::string name,
    const svmi::ParamType retType,
    const std::vector<svmi::ParamType>& paramTypes) {
  for (const svmi::FunctionSignature* p = skiptest::globals; p->m_name; ++p) {
    if (name == p->m_name) {
      if (retType != p->m_retType) {
        fail(
            "%s: incorrect return type.  Expected %d but got %d\n",
            name.c_str(),
            (int)retType,
            (int)p->m_retType);
      }
      if (paramTypes.size() != p->m_argCount) {
        fail(
            "%s: incorrect param count.  Expected %zd but got %d\n",
            name.c_str(),
            paramTypes.size(),
            (int)p->m_argCount);
      }
      for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (paramTypes[i] != p->m_argTypes[i]) {
          fail(
              "%s: arg %zd incorrect type.  Expected %d but got %d\n",
              name.c_str(),
              i,
              (int)paramTypes[i],
              (int)p->m_argTypes[i]);
        }
      }
      return reinterpret_cast<T*>(p->m_fnptr);
    }
  }
  fail("exported function '%s' not found", name.c_str());
}

SkipRetValue test_params() {
  printf("--- test_params\n");

  {
    auto fnptr =
        getExportedFunction<void()>("sk_fn1", svmi::ParamType::voidType, {});
    (*fnptr)();
  }

  {
    auto fnptr = getExportedFunction<void(SkipBool)>(
        "sk_fn1_b", svmi::ParamType::voidType, {svmi::ParamType::boolean});
    (*fnptr)(false);
    (*fnptr)(true);
  }

  {
    auto fnptr = getExportedFunction<void(SkipInt)>(
        "sk_fn1_i", svmi::ParamType::voidType, {svmi::ParamType::int64});
    (*fnptr)(0);
    (*fnptr)(std::numeric_limits<SkipInt>::max());
    (*fnptr)(std::numeric_limits<SkipInt>::min());
  }

  {
    auto fnptr = getExportedFunction<void(SkipFloat)>(
        "sk_fn1_f", svmi::ParamType::voidType, {svmi::ParamType::float64});
    (*fnptr)(0);
    (*fnptr)(std::numeric_limits<SkipFloat>::max());
    (*fnptr)(std::numeric_limits<SkipFloat>::min());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmString)>(
        "sk_fn1_s", svmi::ParamType::voidType, {svmi::ParamType::string});
    (*fnptr)(FakeHhvmString("short").toC());
    (*fnptr)(FakeHhvmString("this is a long string").toC());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmVariant*)>(
        "sk_fn1_m", svmi::ParamType::voidType, {svmi::ParamType::mixed});
    (*fnptr)(FakeHhvmVariant{FromNull{}}.toC());
    (*fnptr)(FakeHhvmVariant{FromBool{}, true}.toC());
    (*fnptr)(FakeHhvmVariant{FromInt{}, 123}.toC());
    (*fnptr)(FakeHhvmVariant{FromFloat{}, 4.5}.toC());
    (*fnptr)(FakeHhvmVariant{FromString{}, "abcde"}.toC());
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmObject&)>(
        "sk_fn1_o0", svmi::ParamType::voidType, {svmi::ParamType::object});
    auto od = FakeHhvmObjectData::make("HhvmObject0");
    FakeHhvmObject o{od};
    (*fnptr)(o);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmObject&)>(
        "sk_fn1_o1", svmi::ParamType::voidType, {svmi::ParamType::object});
    auto od = FakeHhvmObjectData::make("HhvmObject1");
    FakeHhvmObject o{od};
    (*fnptr)(o);
  }

  {
    s_gatherBuffer.push_back(32);
    s_gatherBuffer.push_back(33);
    auto fnptr = getExportedFunction<void(FakeHhvmObject&)>(
        "sk_fn1_o2", svmi::ParamType::voidType, {svmi::ParamType::object});
    auto od = FakeHhvmObjectData::make("HhvmObject2");
    FakeHhvmObject o{od};
    (*fnptr)(o);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmArray&)>(
        "sk_fn1_a", svmi::ParamType::voidType, {svmi::ParamType::array});
    auto ad = FakeHhvmArrayData::make("HhvmArray");
    FakeHhvmArray a{ad};
    (*fnptr)(a);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmArray&)>(
        "sk_fn1_shape1", svmi::ParamType::voidType, {svmi::ParamType::array});
    auto ad = FakeHhvmArrayData::make("Shape1");
    FakeHhvmArray a{ad};
    (*fnptr)(a);
  }

  {
    s_pendingResults.push_back(svmi_int64(40));
    s_pendingResults.push_back(svmi_int64(41));
    auto fnptr = getExportedFunction<void(FakeHhvmArray&)>(
        "sk_fn1_shape2", svmi::ParamType::voidType, {svmi::ParamType::array});
    auto ad = FakeHhvmArrayData::make("Shape2");
    FakeHhvmArray a{ad};
    (*fnptr)(a);
  }

  {
    s_pendingResults.push_back(svmi_int64(100));
    s_pendingResults.push_back(svmi_int64(101));
    auto fnptr = getExportedFunction<void(FakeHhvmArray&)>(
        "sk_fn1_tuple", svmi::ParamType::voidType, {svmi::ParamType::array});
    auto ad = FakeHhvmArrayData::make("Tuple2");
    FakeHhvmArray a{ad};
    (*fnptr)(a);
  }

  return svmi_void();
}

template <typename FN>
void callNull(FN fnptr) {
  FakeHhvmVariant v{FromNull{}};
  (*fnptr)(v);
}

template <typename T, typename FN, typename K>
void callHelper(FN fnptr, K kind, T value) {
  FakeHhvmVariant v{kind, value};
  (*fnptr)(v);
}

SkipRetValue test_nullable_params() {
  printf("--- test_nullable_params\n");

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_b",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_boolean});
    callNull(fnptr);
    callHelper(fnptr, FromBool{}, false);
    callHelper(fnptr, FromBool{}, true);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_i",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_int64});
    callNull(fnptr);
    callHelper<SkipInt>(fnptr, FromInt{}, 0);
    callHelper(fnptr, FromInt{}, std::numeric_limits<SkipInt>::max());
    callHelper(fnptr, FromInt{}, std::numeric_limits<SkipInt>::min());
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_f",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_float64});
    callNull(fnptr);
    callHelper<SkipFloat>(fnptr, FromFloat{}, 0);
    callHelper(fnptr, FromFloat{}, std::numeric_limits<SkipFloat>::max());
    callHelper(fnptr, FromFloat{}, std::numeric_limits<SkipFloat>::min());
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_s",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_string});
    callNull(fnptr);
    callHelper<std::string>(fnptr, FromString{}, "short");
    callHelper<std::string>(fnptr, FromString{}, "this is a long string");
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_o0",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_object});
    callNull(fnptr);
    auto subObjectData = FakeHhvmObjectData::make("HhvmObject0");
    callHelper(fnptr, FromObject{}, subObjectData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_o1",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_object});
    callNull(fnptr);
    auto subObjectData = FakeHhvmObjectData::make("HhvmObject1");
    callHelper(fnptr, FromObject{}, subObjectData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn3_a",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_array});
    callNull(fnptr);
    auto subArrayData = FakeHhvmArrayData::make("HhvmArray");
    callHelper(fnptr, FromArray{}, subArrayData);
  }

  return svmi_void();
}

SkipRetValue test_option_params() {
  printf("--- test_option_params\n");

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_b",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_boolean});
    callNull(fnptr);
    callHelper(fnptr, FromBool{}, false);
    callHelper(fnptr, FromBool{}, true);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_i",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_int64});
    callNull(fnptr);
    callHelper<SkipInt>(fnptr, FromInt{}, 0);
    callHelper(fnptr, FromInt{}, std::numeric_limits<SkipInt>::max());
    callHelper(fnptr, FromInt{}, std::numeric_limits<SkipInt>::min());
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_f",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_float64});
    callNull(fnptr);
    callHelper<SkipFloat>(fnptr, FromFloat{}, 0);
    callHelper(fnptr, FromFloat{}, std::numeric_limits<SkipFloat>::max());
    callHelper(fnptr, FromFloat{}, std::numeric_limits<SkipFloat>::min());
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_s",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_string});
    callNull(fnptr);
    callHelper<std::string>(fnptr, FromString{}, "short");
    callHelper<std::string>(fnptr, FromString{}, "this is a long string");
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_o0",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_object});
    callNull(fnptr);
    auto subObjectData = FakeHhvmObjectData::make("HhvmObject0");
    callHelper(fnptr, FromObject{}, subObjectData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_o1",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_object});
    callNull(fnptr);
    auto subObjectData = FakeHhvmObjectData::make("HhvmObject1");
    callHelper(fnptr, FromObject{}, subObjectData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_o2",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_object});
    callNull(fnptr);
    auto subObjectData = FakeHhvmObjectData::make("HhvmObject2");
    s_pendingResults.push_back(svmi_int64(50));
    s_pendingResults.push_back(svmi_int64(60));
    callHelper(fnptr, FromObject{}, subObjectData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_a",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_array});
    callNull(fnptr);
    auto subArrayData = FakeHhvmArrayData::make("HhvmArray");
    callHelper(fnptr, FromArray{}, subArrayData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_shape1",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_array});
    callNull(fnptr);
    auto subArrayData = FakeHhvmArrayData::make("Shape1");
    callHelper(fnptr, FromArray{}, subArrayData);
  }

  {
    auto fnptr = getExportedFunction<void(FakeHhvmVariant&)>(
        "sk_fn5_shape2",
        svmi::ParamType::voidType,
        {svmi::ParamType::nullable_array});
    callNull(fnptr);
    auto subArrayData = FakeHhvmArrayData::make("Shape2");
    s_pendingResults.push_back(svmi_int64(51));
    s_pendingResults.push_back(svmi_int64(61));
    callHelper(fnptr, FromArray{}, subArrayData);
  }

  return svmi_void();
}

SkipRetValue test_returns() {
  printf("--- test_returns\n");

  {
    auto fnptr = getExportedFunction<SkipBool()>(
        "sk_fn2_b0", svmi::ParamType::boolean, {});
    printf("%s\n", (*fnptr)() ? "true" : "false");
  }

  {
    auto fnptr = getExportedFunction<SkipBool()>(
        "sk_fn2_b1", svmi::ParamType::boolean, {});
    printf("%s\n", (*fnptr)() ? "true" : "false");
  }

  printf(
      "%lld\n",
      (long long)(*getExportedFunction<SkipInt()>(
          "sk_fn2_i0", svmi::ParamType::int64, {}))());
  printf(
      "%lld\n",
      (long long)(*getExportedFunction<SkipInt()>(
          "sk_fn2_i1", svmi::ParamType::int64, {}))());
  printf(
      "%lld\n",
      (long long)(*getExportedFunction<SkipInt()>(
          "sk_fn2_i2", svmi::ParamType::int64, {}))());

  printf(
      "%f\n",
      (*getExportedFunction<SkipFloat()>(
          "sk_fn2_f0", svmi::ParamType::float64, {}))());
  printf(
      "%f\n",
      (*getExportedFunction<SkipFloat()>(
          "sk_fn2_f1", svmi::ParamType::float64, {}))());
  printf(
      "%f\n",
      (*getExportedFunction<SkipFloat()>(
          "sk_fn2_f2", svmi::ParamType::float64, {}))());

  {
    auto fnptr = getExportedFunction<void(HhvmStringRet)>(
        "sk_fn2_s0", svmi::ParamType::string, {});
    FakeHhvmStringRet str;
    (*fnptr)(str.toC());
    printf("%s\n", str.m_str.c_str());
  }
  {
    auto fnptr = getExportedFunction<void(HhvmStringRet)>(
        "sk_fn2_s1", svmi::ParamType::string, {});
    FakeHhvmStringRet str;
    (*fnptr)(str.toC());
    printf("%s\n", str.m_str.c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmObjectRet)>(
        "sk_fn2_o0", svmi::ParamType::object, {});
    FakeHhvmObjectRet o;
    (*fnptr)(o.toC());
    printf("%s\n", o.m_object->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmObjectRet)>(
        "sk_fn2_o1", svmi::ParamType::object, {});
    FakeHhvmObjectRet o;
    (*fnptr)(o.toC());
    printf("%s\n", o.m_object->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmObjectRet)>(
        "sk_fn2_o2", svmi::ParamType::object, {});
    FakeHhvmObjectRet o;
    (*fnptr)(o.toC());
    printf("%s\n", o.m_object->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmArrayRet)>(
        "sk_fn2_a", svmi::ParamType::array, {});
    FakeHhvmArrayRet a;
    (*fnptr)(a.toC());
    printf("%s\n", a.m_array->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmArrayRet)>(
        "sk_fn2_shape1", svmi::ParamType::array, {});
    FakeHhvmArrayRet a;
    (*fnptr)(a.toC());
    printf("%s\n", a.m_array->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmArrayRet)>(
        "sk_fn2_shape2", svmi::ParamType::array, {});
    FakeHhvmArrayRet a;
    (*fnptr)(a.toC());
    printf("%s\n", a.m_array->strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmVariant*)>(
        "sk_fn2_m0", svmi::ParamType::mixed, {});
    FakeHhvmVariant ret;
    (*fnptr)(ret.toC());
    printf("%s\n", ret.strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmVariant*)>(
        "sk_fn2_m1", svmi::ParamType::mixed, {});
    FakeHhvmVariant ret;
    (*fnptr)(ret.toC());
    printf("%s\n", ret.strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmVariant*)>(
        "sk_fn2_m2", svmi::ParamType::mixed, {});
    FakeHhvmVariant ret;
    (*fnptr)(ret.toC());
    printf("%s\n", ret.strValue().c_str());
  }

  {
    auto fnptr = getExportedFunction<void(HhvmArrayRet)>(
        "sk_fn2_tup", svmi::ParamType::array, {});
    FakeHhvmArrayRet ret;
    (*fnptr)(ret.toC());
    printf("%s\n", ret.m_array->strValue().c_str());
  }

  return svmi_void();
}

void test_nullable_return(const std::string& name, svmi::ParamType type) {
  FakeHhvmVariant v;
  auto fnptr = getExportedFunction<void(HhvmVariant*)>(name, type, {});
  (*fnptr)(v.toVariantRet());
  printf("%s: %s\n", name.c_str(), v.strValue().c_str());
}

SkipRetValue test_nullable_returns() {
  printf("--- test_nullable_returns\n");

  test_nullable_return("sk_fn4_b0", svmi::ParamType::nullable_boolean);
  test_nullable_return("sk_fn4_b1", svmi::ParamType::nullable_boolean);
  test_nullable_return("sk_fn4_b2", svmi::ParamType::nullable_boolean);

  test_nullable_return("sk_fn4_i0", svmi::ParamType::nullable_int64);
  test_nullable_return("sk_fn4_i1", svmi::ParamType::nullable_int64);
  test_nullable_return("sk_fn4_i2", svmi::ParamType::nullable_int64);

  test_nullable_return("sk_fn4_f0", svmi::ParamType::nullable_float64);
  test_nullable_return("sk_fn4_f1", svmi::ParamType::nullable_float64);

  test_nullable_return("sk_fn4_s0", svmi::ParamType::nullable_string);
  test_nullable_return("sk_fn4_s1", svmi::ParamType::nullable_string);
  test_nullable_return("sk_fn4_s2", svmi::ParamType::nullable_string);

  test_nullable_return("sk_fn4_o0_0", svmi::ParamType::nullable_object);
  test_nullable_return("sk_fn4_o0_1", svmi::ParamType::nullable_object);

  test_nullable_return("sk_fn4_o1_0", svmi::ParamType::nullable_object);
  test_nullable_return("sk_fn4_o1_1", svmi::ParamType::nullable_object);

  test_nullable_return("sk_fn4_a_0", svmi::ParamType::nullable_array);
  test_nullable_return("sk_fn4_a_1", svmi::ParamType::nullable_array);

  return svmi_void();
}

void test_option_return(const std::string& name, svmi::ParamType type) {
  FakeHhvmVariant v;
  auto fnptr = getExportedFunction<void(HhvmVariant*, SkipInt)>(
      name, type, {svmi::ParamType::int64});
  (*fnptr)(v.toVariantRet(), 123);
  printf("%s: %s\n", name.c_str(), v.strValue().c_str());
}

SkipRetValue test_option_returns() {
  printf("--- test_option_returns\n");

  test_option_return("sk_fn6_b0", svmi::ParamType::nullable_boolean);
  test_option_return("sk_fn6_b1", svmi::ParamType::nullable_boolean);
  test_option_return("sk_fn6_b2", svmi::ParamType::nullable_boolean);

  test_option_return("sk_fn6_i0", svmi::ParamType::nullable_int64);
  test_option_return("sk_fn6_i1", svmi::ParamType::nullable_int64);
  test_option_return("sk_fn6_i2", svmi::ParamType::nullable_int64);

  test_option_return("sk_fn6_f0", svmi::ParamType::nullable_float64);
  test_option_return("sk_fn6_f1", svmi::ParamType::nullable_float64);

  test_option_return("sk_fn6_s0", svmi::ParamType::nullable_string);
  test_option_return("sk_fn6_s1", svmi::ParamType::nullable_string);
  test_option_return("sk_fn6_s2", svmi::ParamType::nullable_string);

  test_option_return("sk_fn6_o0_0", svmi::ParamType::nullable_object);
  test_option_return("sk_fn6_o0_1", svmi::ParamType::nullable_object);

  test_option_return("sk_fn6_o1_0", svmi::ParamType::nullable_object);
  test_option_return("sk_fn6_o1_1", svmi::ParamType::nullable_object);

  test_option_return("sk_fn6_o2_0", svmi::ParamType::nullable_object);
  test_option_return("sk_fn6_o2_1", svmi::ParamType::nullable_object);

  test_option_return("sk_fn6_a_0", svmi::ParamType::nullable_array);
  test_option_return("sk_fn6_a_1", svmi::ParamType::nullable_array);

  test_option_return("sk_fn6_shape1_0", svmi::ParamType::nullable_array);
  test_option_return("sk_fn6_shape1_1", svmi::ParamType::nullable_array);

  test_option_return("sk_fn6_shape2_0", svmi::ParamType::nullable_array);
  test_option_return("sk_fn6_shape2_1", svmi::ParamType::nullable_array);

  return svmi_void();
}

SkipRetValue test_gather_param() {
  printf("--- test_gather_param\n");

  // a: Bool
  pushGatherBool(false);
  // b: Int
  pushGatherInt(23);
  // c: Float
  pushGatherFloat(3.125);
  // d: String
  pushGatherString("hello, world!");
  // e: Copy
  pushGatherInt(123);
  pushGatherInt(124);
  // f: ?Copy
  pushGatherInt(0);
  // g: ?Copy
  pushGatherInt(1);
  pushGatherInt(100);
  pushGatherInt(101);
  // h: Proxy
  pushGatherHandle(skip::Obstack::cur().wrapHhvmHeapObject(
      FakeHhvmObjectData::make("Proxy")->toC()));
  // i: Base
  pushGatherInt(lookupClassIndexByName("GatherTest.Copy"));
  pushGatherInt(200);
  pushGatherInt(201);
  // j: Array<Int>
  pushGatherInt(3);
  pushGatherInt(10);
  pushGatherInt(9);
  pushGatherInt(8);
  // k: Map<Int, Int>
  pushGatherInt(2);
  pushGatherInt(1);
  pushGatherInt(2);
  pushGatherInt(3);
  pushGatherInt(4);
  // l: Set<Int>
  pushGatherInt(3);
  pushGatherInt(10);
  pushGatherInt(11);
  pushGatherInt(12);
  // m: ??Copy
  pushGatherInt(-1);
  // n: ??Copy
  pushGatherInt(0);
  // o: ??Copy
  pushGatherInt(1);
  pushGatherInt(2);
  pushGatherInt(3);
  // p: CopyShape
  pushGatherInt(10);
  pushGatherInt(11);
  // q: (Int, String, Float)
  pushGatherInt(5);
  pushGatherString("ten");
  pushGatherFloat(10.25);
  // r: HH.Int
  pushGatherInt(23);
  pushGatherInt(2); // int
  // s: HH.Number
  pushGatherFloat(10.5);
  pushGatherInt(3); // float
  // t: HHArray
  pushGatherHandle(skip::Obstack::cur().wrapHhvmHeapObject(
      FakeHhvmArrayData::make("HHArray", 2)->toC()));

  auto fnptr = getExportedFunction<void(FakeHhvmObject&)>(
      "sk_fn7", svmi::ParamType::voidType, {svmi::ParamType::object});
  auto od = FakeHhvmObjectData::make("Gather");
  FakeHhvmObject o{od};
  (*fnptr)(o);

  return svmi_void();
}

SkipRetValue test_bugs() {
  {
    printf("--- bug1\n");
    auto fnptr = getExportedFunction<void(HhvmVariant*, FakeHhvmVariant&)>(
        "Bugs_bug1",
        svmi::ParamType::nullable_int64,
        {svmi::ParamType::nullable_int64});
    FakeHhvmVariant ret;
    FakeHhvmVariant v{FromNull{}};
    (*fnptr)(ret.toVariantRet(), v);
    printf("%s\n", ret.strValue().c_str());
    v.setInt64(27);
    (*fnptr)(ret.toVariantRet(), v);
    printf("%s\n", ret.strValue().c_str());
  }

  {
    printf("--- bug2\n");
    auto fnptr = getExportedFunction<void(HhvmVariant*, int64_t)>(
        "Bugs_bug2", svmi::ParamType::mixed, {svmi::ParamType::int64});
    FakeHhvmVariant ret;
    (*fnptr)(ret.toVariantRet(), 0);
    printf("%s\n", ret.strValue().c_str());
    (*fnptr)(ret.toVariantRet(), 1);
    printf("%s\n", ret.strValue().c_str());
  }

  return svmi_void();
}
} // anonymous namespace

skip::String SKIP_string_extractData(HhvmString handle) {
  return String{FakeHhvmString::fromC(handle)->m_str};
}

void SKIP_HhvmStringRet_create(HhvmStringRet ret, skip::String s) {
  FakeHhvmStringRet::fromC(ret)->m_str = String{s}.toCppString();
}

void SKIP_HhvmObjectRet_create(HhvmObjectRet ret, skip::HhvmHandle* wrapper) {
  FakeHhvmObjectRet::fromC(ret)->m_object = FakeHhvmObjectData::fromC(wrapper);
}

void SKIP_HhvmArrayRet_create(HhvmArrayRet ret, skip::HhvmHandle* wrapper) {
  FakeHhvmArrayRet::fromC(ret)->m_array = FakeHhvmArrayData::fromC(wrapper);
}

void SKIP_HHVM_throwException(SkipRObj* exc, SkipObstackPos note) {
  auto s = skip::String{SKIP_getExceptionMessage(exc)};
  throw std::runtime_error(s.toCppString());
}

SkipRetValue SKIP_HHVM_callFunction(
    skip::HhvmHandle* object,
    skip::String function_,
    skip::String paramTypes_,
    ...) {
  auto function = String{function_}.toCppString();
  if (function == "test_params") {
    return test_params();
  } else if (function == "test_option_params") {
    return test_option_params();
  } else if (function == "test_nullable_params") {
    return test_nullable_params();
  } else if (function == "test_returns") {
    return test_returns();
  } else if (function == "test_nullable_returns") {
    return test_nullable_returns();
  } else if (function == "test_option_returns") {
    return test_option_returns();
  } else if (function == "test_gather_param") {
    return test_gather_param();
  } else if (function == "Bugs.test") {
    return test_bugs();
  } else if (function == "report") {
    auto obj = FakeHhvmObjectData::fromC(object);
    return obj->report();
  } else if (function == "skip_hhvm_handle_inspect") {
    va_list ap;
    va_start(ap, paramTypes_);
    auto obj = FakeHhvmHeapObject::fromC(va_arg(ap, skip::HhvmHandle*));
    va_end(ap);
    return svmi_string(obj->strValue());
  } else if (
      (function == "HhvmObject0::make") || (function == "HhvmObject1::make")) {
    va_list ap;
    va_start(ap, paramTypes_);
    auto s = String{va_arg(ap, StringRep)}.toCppString();
    auto o = FakeHhvmObjectData::make(s);
    auto v = ret_garbage();
    v.value.m_object = SKIP_Obstack_wrapHhvmHeapObject(o->toC());
    v.type = RetType::object;
    va_end(ap);
    return v;
  } else if ((function == "Shape1::make") || (function == "arrayMake")) {
    va_list ap;
    va_start(ap, paramTypes_);
    auto s = String{va_arg(ap, StringRep)}.toCppString();
    auto o = FakeHhvmArrayData::make(s);
    auto v = ret_garbage();
    v.value.m_array = SKIP_Obstack_wrapHhvmHeapObject(o->toC());
    v.type = RetType::array;
    va_end(ap);
    return v;
  } else if (function == "arrayReport") {
    va_list ap;
    va_start(ap, paramTypes_);
    auto o = FakeHhvmArrayData::fromC(va_arg(ap, skip::HhvmHandle*));
    va_end(ap);
    return o->report();
  } else {
    fail("Unknown function: %s", function.c_str());
  }
}

SkipRetValue SKIP_HHVM_Nullable_getBool(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::boolean);
    return svmi_bool(p.m_value_b);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getInt64(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::int64);
    return svmi_int64(p.m_value_i);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getFloat64(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::float64);
    return svmi_float64(p.m_value_f);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getString(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::string);
    return svmi_string(p.m_value_s);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getObject(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::object);
    auto handle = SKIP_Obstack_wrapHhvmHeapObject(p.m_value_o->toC());
    return svmi_object(handle);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getArray(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  if (p.m_kind == FakeHhvmVariant::Kind::null) {
    return svmi_null();
  } else {
    assert(p.m_kind == FakeHhvmVariant::Kind::array);
    auto handle = SKIP_Obstack_wrapHhvmHeapObject(p.m_value_a->toC());
    return svmi_array(handle);
  }
}

SkipRetValue SKIP_HHVM_Nullable_getMixed(HhvmVariant* p_) {
  FakeHhvmVariant& p = *FakeHhvmVariant::fromC(p_);
  switch (p.m_kind) {
    case FakeHhvmVariant::Kind::undef:
      return svmi_null();
    case FakeHhvmVariant::Kind::null:
      return svmi_null();
    case FakeHhvmVariant::Kind::boolean:
      return svmi_bool(p.m_value_b);
    case FakeHhvmVariant::Kind::int64:
      return svmi_int64(p.m_value_i);
    case FakeHhvmVariant::Kind::float64:
      return svmi_float64(p.m_value_f);
    case FakeHhvmVariant::Kind::string:
      return svmi_string(p.m_value_s);
    case FakeHhvmVariant::Kind::object: {
      auto handle = SKIP_Obstack_wrapHhvmHeapObject(p.m_value_o->toC());
      return svmi_object(handle);
    }
    case FakeHhvmVariant::Kind::array: {
      auto handle = SKIP_Obstack_wrapHhvmHeapObject(p.m_value_a->toC());
      return svmi_array(handle);
    }
  }
  fprintf(stderr, "unimplemented");
  abort();
}

SkipRetValue SKIP_HHVM_Object_getField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name) {
  printf(
      "SKIP_HHVM_Object_getField_Mixed(%s, %s)\n",
      FakeHhvmObjectData::fromC(wrapper)->strValue().c_str(),
      name.toCppString().c_str());
  return nextValue();
}

SkipRetValue SKIP_HHVM_Shape_getField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name) {
  printf(
      "SKIP_HHVM_Shape_getField_Mixed(%s, %s)\n",
      FakeHhvmArrayData::fromC(wrapper)->strValue().c_str(),
      name.toCppString().c_str());
  return nextValue();
}

void SKIP_HhvmVariant_fromNull(HhvmVariant* ret_) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setNull();
}

void SKIP_HhvmVariant_fromBool(HhvmVariant* ret_, SkipBool b) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setBool(b);
}

void SKIP_HhvmVariant_fromInt64(HhvmVariant* ret_, SkipInt i) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setInt64(i);
}

void SKIP_HhvmVariant_fromFloat64(HhvmVariant* ret_, SkipFloat f) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setFloat64(f);
}

void SKIP_HhvmVariant_fromString(HhvmVariant* ret_, skip::String s) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setString(String{s}.toCppString());
}

void SKIP_HhvmVariant_fromObject(HhvmVariant* ret_, skip::HhvmHandle* wrapper) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setObject(FakeHhvmObjectData::fromC(wrapper));
}

void SKIP_HhvmVariant_fromArray(HhvmVariant* ret_, skip::HhvmHandle* wrapper) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  ret->setArray(FakeHhvmArrayData::fromC(wrapper));
}

void SKIP_HhvmVariant_fromMixed(HhvmVariant* ret_, SkipRetValue value) {
  auto ret = FakeHhvmVariant::fromC(ret_);
  switch (value.type) {
    case RetType::null:
      ret->setNull();
      break;
    case RetType::boolean:
      ret->setBool(value.value.m_boolean);
      break;
    case RetType::int64:
      ret->setInt64(value.value.m_int64);
      break;
    case RetType::float64:
      ret->setFloat64(value.value.m_float64);
      break;
    case RetType::string:
      ret->setString(String{value.value.m_string}.toCppString());
      break;
    case RetType::object:
      ret->setObject(FakeHhvmObjectData::fromC(value.value.m_object));
      break;
    case RetType::array:
      ret->setArray(FakeHhvmArrayData::fromC(value.value.m_array));
      break;
  }
}

void SKIP_HHVM_Object_setField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name,
    SkipRetValue value) {
  String::CStrBuffer buf;
  printf(
      "SKIP_HHVM_Object_setField_Mixed(%s, \"%s\", ",
      FakeHhvmObjectData::fromC(wrapper)->strValue().c_str(),
      String{name}.c_str(buf));
  switch (value.type) {
    case RetType::null:
      printf("null");
      break;
    case RetType::boolean:
      printf(value.value.m_boolean ? "true" : "false");
      break;
    case RetType::int64:
      printf("%lld", value.value.m_int64);
      break;
    case RetType::float64:
      printf("%f", value.value.m_float64);
      break;
    case RetType::string:
      printf(
          "\"%s\"", skip::String{value.value.m_string}.toCppString().c_str());
      break;
    case RetType::object:
      printf(
          "%s",
          FakeHhvmObjectData::fromC(value.value.m_object)->strValue().c_str());
      break;
    case RetType::array:
      printf(
          "%s",
          FakeHhvmArrayData::fromC(value.value.m_array)->strValue().c_str());
      break;
  }
  printf(")\n");
}

void SKIP_HHVM_Shape_setField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name,
    SkipRetValue value) {
  String::CStrBuffer buf;
  printf(
      "SKIP_HHVM_Shape_setField_Mixed(%s, \"%s\", ",
      FakeHhvmArrayData::fromC(wrapper)->strValue().c_str(),
      String{name}.c_str(buf));
  switch (value.type) {
    case RetType::null:
      printf("null");
      break;
    case RetType::boolean:
      printf(value.value.m_boolean ? "true" : "false");
      break;
    case RetType::int64:
      printf("%lld", value.value.m_int64);
      break;
    case RetType::float64:
      printf("%f", value.value.m_float64);
      break;
    case RetType::string:
      printf(
          "\"%s\"", skip::String{value.value.m_string}.toCppString().c_str());
      break;
    case RetType::object:
      printf(
          "%s",
          FakeHhvmObjectData::fromC(value.value.m_object)->strValue().c_str());
      break;
    case RetType::array:
      printf(
          "%s",
          FakeHhvmArrayData::fromC(value.value.m_array)->strValue().c_str());
      break;
  }
  printf(")\n");
}

namespace SKIP {
namespace Vector {
namespace HH_varray {

template <>
SkipRetValue get<>(skip::HhvmHandle* obj, SkipInt idx) {
  printf(
      "SKIP::Vector::HH_varray::get(%s, %lld)\n", arrayToId(obj).c_str(), idx);
  return nextValue();
}

template <>
skip::HhvmHandle* internalCreate(skip::RObj*) {
  printf("HH_varray::internalCreate()\n");
  auto od = FakeHhvmArrayData::make("varray", 0);
  auto handle = skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
  return handle;
}

template <>
void append(skip::HhvmHandle* obj, SkipRetValue value) {
  printf(
      "HH_varray::append(%s, %s)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str(),
      repr(value).c_str());
}
} // namespace HH_varray
} // namespace Vector
} // namespace SKIP
