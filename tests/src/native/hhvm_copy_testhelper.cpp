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
#include <deque>
#include <boost/format.hpp>
#include <boost/intrusive_ptr.hpp>

#define TESTHELPER_FAKE_OBJECTS
#define TESTHELPER_VALUES
#include "testhelper_common.h"

extern "C" {

skip::HhvmHandle* SKIP_internalMakeObject(skip::String type);
skip::HhvmHandle* SKIP_internalMakeObject(skip::String type) {
  auto od = FakeHhvmObjectData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

skip::HhvmHandle* SKIP_internalMakeArray(skip::String type, SkipInt size);
skip::HhvmHandle* SKIP_internalMakeArray(skip::String type, SkipInt size) {
  auto od = FakeHhvmArrayData::make(type.toCppString().c_str(), size);
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
}

skip::HhvmHandle* SKIP_internalMakeShape(skip::String type);
skip::HhvmHandle* SKIP_internalMakeShape(skip::String type) {
  auto od = FakeHhvmArrayData::make(type.toCppString().c_str());
  return skip::Obstack::cur().wrapHhvmHeapObject(od->toC());
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

void SKIP_HHVM_Object_setField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name,
    SkipRetValue value) {
  printf(
      "SKIP_HHVM_Object_setField_Mixed(%s, %s, %s)\n",
      FakeHhvmObjectData::fromC(wrapper)->strValue().c_str(),
      name.toCppString().c_str(),
      repr(value).c_str());
}

void SKIP_HHVM_Shape_setField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name,
    SkipRetValue value) {
  printf(
      "SKIP_HHVM_Shape_setField_Mixed(%s, %s, %s)\n",
      FakeHhvmArrayData::fromC(wrapper)->strValue().c_str(),
      name.toCppString().c_str(),
      repr(value).c_str());
}
} // extern "C"

namespace SKIP {
namespace Vector {
namespace HH_varray {

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

template <>
SkipRetValue get<>(skip::HhvmHandle* obj, SkipInt idx) {
  printf(
      "SKIP::Vector::HH_varray::get(%s, %lld)\n",
      FakeHhvmArrayData::fromC(obj)->strValue().c_str(),
      idx);
  return nextValue();
}
} // namespace HH_varray
} // namespace Vector
} // namespace SKIP
