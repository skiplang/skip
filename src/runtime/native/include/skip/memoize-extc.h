/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Async-extc.h"
#include "String-extc.h"
#include "objects-extc.h"

DEFINE_TYPE(SkipMemoValue, skip::MemoValue);

extern "C" {

extern void SKIP_memoValueBoxFloat(SkipMemoValue* mv, SkipFloat n);
extern void SKIP_memoValueBoxInt(SkipMemoValue* mv, SkipInt n);
extern void SKIP_memoValueBoxString(SkipMemoValue* mv, SkipString n);
extern void SKIP_memoValueBoxObject(SkipMemoValue* mv, SkipRObjOrFakePtr p);

extern void SKIP_memoizeCall(
    SkipAwaitable* resultAwaitable,
    SkipRObj* uninternedInvocation);

extern SkipInt SKIP_Debug_getLeakCounter(SkipString classname);
} // extern "C"
