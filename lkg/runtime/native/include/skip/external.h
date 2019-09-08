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

extern "C" {

#define SKIP_NORETURN __attribute__((__noreturn__))

// T30334345: New symbols added to this file should be prefixed with 'SKIPC_'
// NOT 'SKIP_'!

// Defined in prelude/native/Runtime.sk
extern SkipString SKIP_getExceptionMessage(SkipRObj* skipException);
extern SkipRObj* SKIP_makeRuntimeError(SkipString message);
extern SKIP_NORETURN void SKIP_throwRuntimeError(SkipString message);
extern SkipRObj* SKIP_createStringVector(int64_t size);
extern SkipRObj* SKIP_createIntVector(int64_t size);
extern SKIP_NORETURN void SKIP_throwInvariantViolation(SkipString msg);
extern SKIP_NORETURN void SKIP_throwOutOfBounds(void);

// Defined in prelude/native/Awaitable.sk
extern void SKIP_awaitableResume(SkipRObj* awaitable);
extern void SKIP_awaitableFromMemoValue(
    const skip::MemoValue* mv,
    skip::Awaitable* awaitable);
extern void SKIP_awaitableToMemoValue(
    skip::MemoValue* mv,
    skip::Awaitable* awaitable);

// Defined in prelude/Subprocess.sk
extern SkipRObj* SKIP_unsafeCreateSubprocessOutput(
    int64_t returnCode,
    SkipRObj* stdout,
    SkipRObj* stderr);
extern SkipRObj* SKIP_UInt8Array_create(int64_t capacity);

extern SkipRetValue SKIPC_iteratorNext(skip::RObj* iterator);

// This value is a hash defined by the compiler and is based on the source code
// used to build the memoization tables.  It's used when serializing (and
// deserializing) the memoizer so we can tell if the saved cache is valid.
extern size_t SKIPC_buildHash();
} // extern "C"
