/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Obstack-extc.h"
#include "String-extc.h"
#include "plugin-extc.h"
#include "VTable.h"

// These are callback functions defined by the Skip compiler.  If you add a
// function here remember to add a stub to
// tests/src/runtime/native/testutil.cpp.

namespace svmi {

// The parameter type of the arg.
//
// In the table below if the return is marked with (1) then the return is
// actually a reference passed in the first parameter.
//
// For example - A function which takes and returns a ParamType::int64 would be
// written as:
//
//   int64_t myfn(int64_t param0) { return ...; }
//
// But a function which takes and returns a ParamType::string would be written
// as:
//
//   void myfn(HhvmStringRet ret, HhvmString param0) { *ret = ...; }
//
enum class ParamType : uint8_t {
  nullableMask = 64,
  //                        parameter         return
  voidType = 0, // n/a               void
  boolean = 1, // bool              bool
  int64 = 2, // int64_t           int64_t
  float64 = 3, // double            double
  string = 4, // HhvmString        HhvmStringRet(1)
  object = 5, // SkipHhvmHandle*   SkipHhvmHandle*
  array = 6, // SkipHhvmHandle*   SkipHhvmHandle*
  mixed = 7, // SkipRetType   HhvmVariant(1)
  nullable_boolean = 65, // HhvmVariant       HhvmVariant(1)
  nullable_int64 = 66, // HhvmVariant       HhvmVariant(1)
  nullable_float64 = 67, // HhvmVariant       HhvmVariant(1)
  nullable_string = 68, // HhvmVariant       HhvmVariant(1)
  nullable_object = 69, // HhvmVariant       HhvmVariant(1)
  nullable_array = 70, // HhvmVariant       HhvmVariant(1)
};

using FunctionPtr = void (*)();

struct FunctionSignature {
  const char* m_name; // name of the function

  // Pointer to the function.
  FunctionPtr m_fnptr;

  ParamType m_retType;
  uint8_t m_argCount;
  const ParamType* m_argTypes;
};

struct Desc {
  union {
    svmi::ParamType paramType;
    SkipInt _padding;
  };
  SkipInt classId; // when typ == Array or Object
  skip::AObj<Desc*>* targs;
};

struct Field {
  skip::String name;
  Desc* typ;
};

enum class ClassKind : SkipInt {
  base = 0,
  proxyClass = 1,
  copyClass = 2,
  proxyShape = 3,
  copyShape = 4,
};

// Magic class IDs for field types of 'array'
enum class ClassIdMagic {
  unknown = -1,
  vec = -16,
  dict = -17,
  keyset = -18,
  tuple = -19,
};

struct Class {
  skip::String name;
  ClassKind kind;
  skip::AObj<Field*>* fields;
};

struct TypeTable {
  skip::AObj<Class*>* classes;
};
} // namespace svmi

extern "C" {

#define SKIP_NORETURN __attribute__((__noreturn__))

// T30334345: New symbols added to this file should be prefixed with 'SKIPC_'
// NOT 'SKIP_'!

extern const svmi::FunctionSignature* SKIP_initializeSkip(void);

// Defined in prelude/native/Runtime.sk
extern SkipString SKIP_getExceptionMessage(SkipRObj* skipException);
extern SkipRObj* SKIP_makeRuntimeError(SkipString message);
extern SKIP_NORETURN void SKIP_throwRuntimeError(SkipString message);
extern SkipRObj* SKIP_createStringVector(int64_t size);
extern SkipRObj* SKIP_createIntVector(int64_t size);
extern SKIP_NORETURN void SKIP_throwInvariantViolation(SkipString msg);
extern SKIP_NORETURN void SKIP_throwOutOfBounds(void);
extern SkipRObj* SKIP_createMixedBool(bool value);
extern SkipRObj* SKIP_createMixedFloat(double value);
extern SkipRObj* SKIP_createMixedInt(int64_t value);
extern SkipRObj* SKIP_createMixedNull(void);
extern SkipRObj* SKIP_createMixedString(SkipString value);

extern SkipRObj* SKIP_createMixedDict(int64_t capacity);
extern void SKIP_MixedDict_set(SkipRObj* obj, SkipString key, SkipRObj* value);
extern SkipRObj* SKIP_MixedDict_freeze(SkipRObj* obj);

extern SkipRObj* SKIP_createMixedVec(int64_t capacity);
extern void SKIP_MixedVec_push(SkipRObj* obj, SkipRObj* value);
extern SkipRObj* SKIP_MixedVec_freeze(SkipRObj* obj);

extern SKIP_NORETURN void SKIP_throwHhvmException(SkipHhvmHandle* object);

extern SkipHhvmHandle* SKIP_getHhvmException(
    SkipRObj* exception,
    SkipHhvmHandle* none);

// Defined in prelude/native/Awaitable.sk
extern void SKIP_awaitableResume(SkipRObj* awaitable);
extern void SKIP_awaitableFromMemoValue(
    const skip::MemoValue* mv,
    skip::Awaitable* awaitable);
extern void SKIP_awaitableToMemoValue(
    skip::MemoValue* mv,
    skip::Awaitable* awaitable);

// Defined in prelude/HhvmInterop.sk
extern SkipRetValue SKIP_createTupleFromMixed(SkipRObj* mixed);

// Defined in prelude/Subprocess.sk
extern SkipRObj* SKIP_unsafeCreateSubprocessOutput(
    int64_t returnCode,
    SkipRObj* stdout,
    SkipRObj* stderr);
extern SkipRObj* SKIP_UInt8Array_create(int64_t capacity);

extern SkipRetValue SKIPC_iteratorNext(skip::RObj* iterator);

extern svmi::TypeTable* SKIPC_hhvmTypeTable();

// This value is a hash defined by the compiler and is based on the source code
// used to build the memoization tables.  It's used when serializing (and
// deserializing) the memoizer so we can tell if the saved cache is valid.
extern size_t SKIPC_buildHash();
} // extern "C"
