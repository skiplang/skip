/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "String-extc.h"
#include "detail/extc.h"

#include <cstdint>

extern "C" {

extern SkipInt SKIP_Reactive_reactiveTimer(
    SkipString id,
    SkipFloat intervalInSeconds);

extern SkipInt SKIP_Reactive_nextReactiveGlobalCacheID(void);

extern void SKIP_Reactive_reactiveGlobalCacheSet(
    SkipInt id,
    SkipString key,
    SkipRObj* value);

extern SkipIObj* SKIP_Reactive_reactiveGlobalCacheGet(
    SkipInt id,
    SkipString key);

extern void SKIP_Reactive_withTransaction(SkipRObj* callback);
extern SkipInt SKIP_Reactive_unsafe(SkipRObj* value);
}
