/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "skip/Obstack-extc.h"
#include "skip/String-extc.h"
#include "skip/objects-extc.h"
#include "skip/detail/FakePtr.h"

#include <cstddef>

namespace skip {
using OptHhvmHandle = detail::TOrFakePtr<HhvmHandle>;
struct OptString;
} // namespace skip

extern "C" {

// These entrypoints are exported by the plugin.

DEFINE_PTR_TYPE(HhvmString, skip::HhvmString);
DEFINE_TYPE(HhvmVariant, skip::HhvmVariant);

enum class RetType : SkipInt {
  null = 0,
  boolean,
  int64,
  float64,
  string,
  object,
  array,
};

struct SkipRetValue {
  union ParamValue {
    SkipBool m_boolean;
    SkipInt m_int64;
    SkipFloat m_float64;
    skip::StringRep m_string;
    SkipHhvmHandle* m_object;
    SkipHhvmHandle* m_array;
  } value;
  // This is actually a bool wrapped in an int64_t for calling convention
  // purposes
  RetType type;
};

extern SkipString SKIP_string_extractData(HhvmString handle);

extern void SKIP_HHVM_throwException(SkipRObj* exc, SkipObstackPos note);

extern void SKIP_HhvmStringRet_create(HhvmString* ret, SkipString s);
extern void SKIP_HhvmObjectRet_create(HhvmObject* ret, SkipHhvmHandle* wrapper);
extern void SKIP_HhvmArrayRet_create(HhvmArray* ret, SkipHhvmHandle* wrapper);

extern void SKIP_HhvmVariant_fromNull(HhvmVariant* ret);
extern void SKIP_HhvmVariant_fromBool(HhvmVariant* ret, SkipBool b);
extern void SKIP_HhvmVariant_fromInt64(HhvmVariant* ret, SkipInt i);
extern void SKIP_HhvmVariant_fromFloat64(HhvmVariant* ret, SkipFloat f);
extern void SKIP_HhvmVariant_fromString(HhvmVariant* ret, SkipString s);
extern void SKIP_HhvmVariant_fromObject(
    HhvmVariant* ret,
    SkipHhvmHandle* wrapper);
extern void SKIP_HhvmVariant_fromArray(
    HhvmVariant* ret,
    SkipHhvmHandle* wrapper);
extern void SKIP_HhvmVariant_fromMixed(HhvmVariant* ret, SkipRetValue value);

extern void SKIP_HHVM_incref(SkipHhvmHandle* wrapper);
extern void SKIP_HHVM_decref(SkipHhvmHandle* wrapper);
extern SkipString SKIP_HHVM_Object_getType(SkipHhvmHandle* wrapper);
extern SkipHhvmHandle* SKIP_HHVM_Object_create(SkipString name);
extern SkipHhvmHandle* SKIP_HHVM_Shape_create(SkipString name);
extern SkipRetValue SKIP_HHVM_MaybeConvertToArray(SkipRetValue obj);

extern void SKIP_HHVM_Object_setField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name,
    SkipRetValue value);

extern void SKIP_HHVM_Shape_setField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name,
    SkipRetValue value);

extern SkipRetValue SKIP_HHVM_Object_getField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name);

extern SkipRetValue SKIP_HHVM_Shape_getField_Mixed(
    SkipHhvmHandle* wrapper,
    SkipString name);

extern SkipHhvmHandle* SKIP_HhvmInterop_createArrayFromFixedVector(
    SkipString kind,
    SkipInt size,
    SkipRObj* iterator);

extern SkipRetValue SKIP_HHVM_callFunction(
    SkipHhvmHandle* object,
    SkipString function,
    // The first byte is the (ParamType) return type.  Subsequent
    // bytes represent the parameters.
    SkipString paramTypes,
    ...);

// Used by @hhvm_export to process Variant parameters
extern SkipRetValue SKIP_HHVM_Nullable_getBool(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getInt64(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getFloat64(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getString(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getObject(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getArray(HhvmVariant* obj);
extern SkipRetValue SKIP_HHVM_Nullable_getMixed(HhvmVariant* obj);

// HhvmInterop_ObjectCons
extern SkipHhvmHandle* SKIP_HhvmInterop_ObjectCons_create(SkipInt classId);
extern void SKIP_HhvmInterop_ObjectCons_finish(SkipHhvmHandle* handle);
extern void SKIP_HhvmInterop_ObjectCons_setFieldMixed(
    SkipHhvmHandle* handle,
    SkipInt index,
    SkipRetValue value);

// HhvmInterop_ShapeCons
extern SkipHhvmHandle* SKIP_HhvmInterop_ShapeCons_create(SkipInt shapeId);
extern void SKIP_HhvmInterop_ShapeCons_finish(SkipHhvmHandle* handle);
extern void SKIP_HhvmInterop_ShapeCons_setFieldMixed(
    SkipHhvmHandle* handle,
    SkipInt index,
    SkipRetValue value);

struct SkipGatherData {
  void* cleanupHandle;
  void* data;
};

// gatherCollect() is passed an HPHP::ObjectData* and serializes it.  After
// processing the serialized data call gatherCleanup() to free associated
// memory.
extern SkipGatherData SKIP_HhvmInterop_Gather_gatherCollect(
    void* object,
    SkipInt classId);

extern void SKIP_HhvmInterop_Gather_gatherCleanup(void* handle);
} // extern "C"

namespace skip {
using HhvmImportFactory =
    SkipRObj* (*)(SkipHhvmHandle* proxyPointer, SkipString hhvmType);

using HhvmArrayFactory = SkipRObj* (*)(SkipHhvmHandle* proxyPointer);
} // namespace skip

namespace SKIP {

namespace ArrayKey {
struct ArrayKeyRep {
  SkipString m_string;
  SkipInt m_int;
};
} // namespace ArrayKey

namespace Vector {
namespace HH_varray {

template <typename... T>
skip::HhvmHandle* internalCreate(skip::RObj*);

template <typename... T>
SkipRetValue get(skip::HhvmHandle*, SkipInt);

template <typename... T>
void append(skip::HhvmHandle*, SkipRetValue);

template <typename... T>
SkipInt size(skip::HhvmHandle*);
} // namespace HH_varray
} // namespace Vector

namespace Map {
namespace HH_darray {

template <typename... T>
SkipRetValue iterGetKey(skip::RObj*, skip::HhvmHandle*, SkipInt);

template <typename... T>
void internalSet(skip::RObj*, skip::HhvmHandle*, SkipRetValue, SkipRetValue);

template <typename... T>
SkipInt iterAdvance(skip::RObj*, skip::HhvmHandle*, SkipInt);

template <typename... T>
SkipRetValue iterGetValue(skip::RObj*, skip::HhvmHandle*, SkipInt);

template <typename... T>
skip::RObj* internalCreate(skip::HhvmArrayFactory, skip::RObj*);

template <typename... T>
SkipInt size(skip::HhvmHandle*);

template <typename... T>
SkipInt iterBegin(skip::RObj*, skip::HhvmHandle*);

template <typename... T>
SkipInt iterEnd(skip::RObj*, skip::HhvmHandle*);
} // namespace HH_darray
} // namespace Map

namespace Set {
namespace HH_keyset {

template <typename... T>
skip::RObj* internalCreate(skip::HhvmArrayFactory, skip::RObj*);

template <typename... T>
void internalSet(skip::RObj*, skip::HhvmHandle*, SkipRetValue);
} // namespace HH_keyset
} // namespace Set
} // namespace SKIP
