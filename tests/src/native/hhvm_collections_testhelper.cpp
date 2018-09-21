/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/String.h"
#include "skip/external.h"
#include "skip/plugin-extc.h"

#include <cstdarg>
#include <folly/Format.h>

#define TESTHELPER_FAKE_OBJECTS
#define TESTHELPER_VALUES
#include "testhelper_common.h"

using namespace skip;

namespace {

std::string arrayToId(skip::HhvmHandle* obj) {
  return FakeHhvmHeapObject::fromC(obj)->strValue();
}
} // namespace

extern "C" {

skip::String SKIP_internalGetHandle(skip::HhvmHandle* handle);
skip::String SKIP_internalGetHandle(skip::HhvmHandle* handle) {
  auto res = FakeHhvmHeapObject::fromC(handle)->strValue();
  return skip::String{res.begin(), res.end()};
}
} // extern "C"

namespace SKIP {
namespace Vector {
namespace HH_varray {

template <>
SkipRetValue get<>(skip::HhvmHandle* obj, SkipInt idx) {
  printf(
      "SKIP::Vector::HH_varray::get<HH.Mixed>(%s, %lld)\n",
      arrayToId(obj).c_str(),
      idx);
  return nextValue();
}
} // namespace HH_varray
} // namespace Vector
} // namespace SKIP

namespace {

template <typename T, typename FN>
skip::HhvmHandle* createArrayHelper(
    std::string name,
    skip::String kind_,
    RObj* vec,
    SkipInt size,
    FN fmt) {
  std::string kind = kind_.toCppString();
  printf(
      "SKIP_HhvmInterop_createArrayFromFixedVector_%s<%s>[",
      name.c_str(),
      kind.c_str());
  auto vector = static_cast<skip::AObj<T>*>(vec);
  for (auto i = 0; i < size; ++i) {
    if (i != 0)
      printf(", ");
    printf("%s", fmt(vector->unsafe_at(i)).c_str());
  }
  printf("]\n");
  auto array = FakeHhvmArrayData::make(kind);
  return SKIP_Obstack_wrapHhvmHeapObject(array->toC());
}
} // namespace

static bool endswith(const std::string& s, const std::string& c) {
  return (s.size() >= c.size() && s.substr(s.size() - c.size(), s.size()) == c);
}

SkipRetValue SKIP_HHVM_callFunction(
    skip::HhvmHandle* object,
    skip::String function_,
    skip::String paramTypes_,
    ...) {
  std::string function = function_.toCppString();
  if (endswith(function, "make_object")) {
    va_list ap;
    va_start(ap, paramTypes_);
    auto s = skip::String{va_arg(ap, skip::StringRep)}.toCppString();
    auto o = FakeHhvmObjectData::make(s);
    auto v = ret_garbage();
    v.value.m_object = SKIP_Obstack_wrapHhvmHeapObject(o->toC());
    v.type = RetType::object;
    va_end(ap);
    return v;
  }

  printf("fun callFunction(");

  if (object) {
    printf("<object> %lld", (long long)object);
  } else {
    printf("null");
  }

  printf(", \"%s\"", function.c_str());

  static_assert(
      sizeof(svmi::ParamType) == sizeof(char), "invalid ParamType size");
  std::array<char, skip::String::CSTR_BUFFER_SIZE> paramBuf;
  const svmi::ParamType* const paramTypes =
      reinterpret_cast<const svmi::ParamType*>(paramTypes_.c_str(paramBuf));

  svmi::ParamType retType = paramTypes[0];

  va_list ap;
  va_start(ap, paramTypes_);

  for (size_t i = 1; paramTypes[i] != svmi::ParamType::voidType; ++i) {
    switch (paramTypes[i]) {
      case svmi::ParamType::boolean: {
        SkipBool v = (SkipBool)va_arg(ap, int);
        printf(", <bool> %s", v ? "true" : "false");
        break;
      }
      case svmi::ParamType::nullable_boolean: {
        auto isNull = (SkipBool)va_arg(ap, int);
        auto v = (SkipBool)va_arg(ap, int);
        if (isNull) {
          printf(", <?bool> null");
        } else {
          printf(", <?bool> %s", v ? "true" : "false");
        }
        break;
      }
      case svmi::ParamType::int64: {
        auto i64 = va_arg(ap, SkipInt);
        printf(", <int64> %lld", i64);
        break;
      }
      case svmi::ParamType::nullable_int64: {
        auto isNull = (SkipBool)va_arg(ap, int);
        auto i64 = va_arg(ap, SkipInt);
        if (isNull) {
          printf(", <?int64> null");
        } else {
          printf(", <?int64> %lld", i64);
        }
        break;
      }
      case svmi::ParamType::float64: {
        auto d = va_arg(ap, SkipFloat);
        printf(", <float64> %f", d);
        break;
      }
      case svmi::ParamType::nullable_float64: {
        auto isNull = (SkipBool)va_arg(ap, int);
        auto d = va_arg(ap, SkipFloat);
        if (isNull) {
          printf(", <?float64> null");
        } else {
          printf(", <?float64> %f", d);
        }
        break;
      }
      case svmi::ParamType::string: {
        auto s = va_arg(ap, skip::StringRep);
        skip::String::CStrBuffer buf;
        printf(", <string> \"%s\"", skip::String{s}.c_str(buf));
        break;
      }
      case svmi::ParamType::nullable_string: {
        auto s = va_arg(ap, skip::StringRep);
        skip::String::CStrBuffer buf;
        if (s.m_sbits == 0) {
          printf(", <?string> null");
        } else {
          printf(", <?string> \"%s\"", skip::String{s}.c_str(buf));
        }
        break;
      }
      case svmi::ParamType::object: {
        auto o = va_arg(ap, skip::HhvmHandle*);
        printf(", <object> %lld", (long long)o);
        break;
      }
      case svmi::ParamType::nullable_object: {
        auto o = va_arg(ap, HhvmObjectDataPtr*);
        if (o == nullptr) {
          printf(", <?object> null");
        } else {
          printf(", <?object> %lld", (long long)o);
        }
        break;
      }
      default:
        abort();
    }
  }

  va_end(ap);
  printf("): ");

  switch (retType) {
    case svmi::ParamType::voidType:
      printf("void");
      break;
    case svmi::ParamType::boolean:
      printf("Bool");
      break;
    case svmi::ParamType::int64:
      printf("Int");
      break;
    case svmi::ParamType::float64:
      printf("Float");
      break;
    case svmi::ParamType::string:
      printf("String");
      break;
    case svmi::ParamType::object:
      printf("HhvmObject");
      break;
    case svmi::ParamType::array:
      printf("HhvmArray");
      break;
    case svmi::ParamType::nullable_boolean:
      printf("?Bool");
      break;
    case svmi::ParamType::nullable_int64:
      printf("?Int");
      break;
    case svmi::ParamType::nullable_float64:
      printf("?Float");
      break;
    case svmi::ParamType::nullable_string:
      printf("?String");
      break;
    case svmi::ParamType::nullable_object:
      printf("?HhvmObject");
      break;
    case svmi::ParamType::nullable_array:
      printf("?HhvmArray");
      break;
    default:
      abort();
  }

  printf("\n");

  return nextValue();
}
