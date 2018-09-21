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

#define TESTHELPER_FAKE_OBJECTS
#define TESTHELPER_VALUES
#include "testhelper_common.h"

using namespace skip;

namespace {

std::string arrayToId(skip::HhvmHandle* obj) {
  return FakeHhvmHeapObject::fromC(obj)->strValue();
}

void printSkipRetValue(SkipRetValue value) {
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
      printf("'%s'", skip::String{value.value.m_string}.toCppString().c_str());
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
    default:
      printf("unknown! (%d, %lld)", (int)value.type, value.value.m_int64);
      break;
  }
}
} // namespace

extern "C" {

void* SKIP_internalMakeObject(skip::String type);
void* SKIP_internalMakeObject(skip::String type) {
  auto od = FakeHhvmObjectData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

void* SKIP_internalMakeArray(skip::String type);
void* SKIP_internalMakeArray(skip::String type) {
  auto od = FakeHhvmArrayData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

SkipInt SKIP_internalGetHandle(void* p);
SkipInt SKIP_internalGetHandle(void* p) {
  return reinterpret_cast<SkipInt>(p);
}

SkipRetValue SKIP_HHVM_callFunction(
    skip::HhvmHandle* object,
    skip::String function,
    skip::String paramTypes_,
    ...) {
  printf("fun callFunction(");

  if (object) {
    printf("%s", FakeHhvmObjectData::fromC(object)->strValue().c_str());
  } else {
    printf("null");
  }

  std::array<char, String::CSTR_BUFFER_SIZE> buf;
  printf(", \"%s\"", String{function}.c_str(buf));

  static_assert(
      sizeof(svmi::ParamType) == sizeof(char), "invalid ParamType size");
  std::array<char, String::CSTR_BUFFER_SIZE> paramBuf;
  const svmi::ParamType* const paramTypes =
      reinterpret_cast<const svmi::ParamType*>(
          String{paramTypes_}.c_str(paramBuf));

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
        auto isNonNull = (SkipBool)va_arg(ap, int);
        auto v = (SkipBool)va_arg(ap, int);
        if (isNonNull) {
          printf(", <?bool> %s", v ? "true" : "false");
        } else {
          printf(", <?bool> null");
        }
        break;
      }
      case svmi::ParamType::int64: {
        auto i64 = va_arg(ap, SkipInt);
        printf(", <int64> %lld", (long long)i64);
        break;
      }
      case svmi::ParamType::nullable_int64: {
        auto isNonNull = (SkipBool)va_arg(ap, int);
        auto i64 = va_arg(ap, SkipInt);
        if (isNonNull) {
          printf(", <?int64> %lld", (long long)i64);
        } else {
          printf(", <?int64> null");
        }
        break;
      }
      case svmi::ParamType::float64: {
        auto d = va_arg(ap, SkipFloat);
        printf(", <float64> %f", d);
        break;
      }
      case svmi::ParamType::nullable_float64: {
        auto isNonNull = (SkipBool)va_arg(ap, int);
        auto d = va_arg(ap, SkipFloat);
        if (isNonNull) {
          printf(", <?float64> %f", d);
        } else {
          printf(", <?float64> null");
        }
        break;
      }
      case svmi::ParamType::string: {
        auto s = String(va_arg(ap, StringRep));
        printf(", <string> \"%s\"", s.c_str(buf));
        break;
      }
      case svmi::ParamType::nullable_string: {
        auto s = va_arg(ap, StringRep);
        if (s.m_sbits == 0) {
          printf(", <?string> null");
        } else {
          printf(", <?string> \"%s\"", String(s).c_str(buf));
        }
        break;
      }
      case svmi::ParamType::object: {
        auto o = va_arg(ap, skip::HhvmHandle*);
        printf(", %s", FakeHhvmObjectData::fromC(o)->strValue().c_str());
        break;
      }
      case svmi::ParamType::nullable_object: {
        auto o = va_arg(ap, skip::HhvmHandle*);
        if (o == nullptr) {
          printf(", <?object> null");
        } else {
          printf(
              ", <?object> %s",
              FakeHhvmObjectData::fromC(o)->strValue().c_str());
        }
        break;
      }
      case svmi::ParamType::array: {
        auto o = va_arg(ap, skip::HhvmHandle*);
        printf(", %s", FakeHhvmArrayData::fromC(o)->strValue().c_str());
        break;
      }
      case svmi::ParamType::nullable_array: {
        auto a = va_arg(ap, skip::HhvmHandle*);
        if (a == nullptr) {
          printf(", <?array> null");
        } else {
          printf(
              ", <?array> %s", FakeHhvmArrayData::fromC(a)->strValue().c_str());
        }
        break;
      }
      case svmi::ParamType::mixed: {
        SkipRetValue value;
        value.value.m_int64 = va_arg(ap, SkipInt);
        value.type = va_arg(ap, RetType);
        printf(", <mixed> ");
        printSkipRetValue(value);
        break;
      }
      default:
        fprintf(stderr, "Invalid ParamType: %d\n", (int)paramTypes[i]);
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
      printf("array");
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
      printf("?array");
      break;
    case svmi::ParamType::mixed:
      printf("HH.Mixed");
      break;
    default:
      abort();
  }

  printf("\n");

  if (retType == svmi::ParamType::voidType) {
    return svmi_void();
  } else {
    return nextValue();
  }
}

SkipRetValue SKIP_HHVM_Object_getField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name) {
  String::CStrBuffer buf;
  printf(
      "getField(wrapper: %s, field: '%s')\n",
      FakeHhvmObjectData::fromC(wrapper)->strValue().c_str(),
      String{name}.c_str(buf));
  return nextValue();
}

SkipRetValue SKIP_HHVM_Shape_getField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name) {
  String::CStrBuffer buf;
  printf(
      "getField<shape>(wrapper: %s, field: '%s')\n",
      FakeHhvmArrayData::fromC(wrapper)->strValue().c_str(),
      String{name}.c_str(buf));
  return nextValue();
}

void SKIP_HHVM_Object_setField_Mixed(
    skip::HhvmHandle* wrapper,
    skip::String name,
    SkipRetValue value) {
  String::CStrBuffer buf;
  printf(
      "setField(wrapper: %s, field: '%s', value: ",
      FakeHhvmObjectData::fromC(wrapper)->strValue().c_str(),
      String{name}.c_str(buf));
  printSkipRetValue(value);
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
  printSkipRetValue(value);
  printf(")\n");
}
} // extern "C"

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
