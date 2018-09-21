/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "objects-extc.h"

DEFINE_TYPE(SkipAwaitable, skip::Awaitable);

extern "C" {

// See Awaitable.sk for docs.
extern SkipBool SKIP_awaitableReadyOrThrow(SkipAwaitable* awaitable);
extern void SKIP_awaitableSyncOrThrow(SkipAwaitable* awaitable);
extern void SKIP_awaitableNotifyWaitersValueIsReady(SkipAwaitable* awaitable);
extern void SKIP_awaitableThrow(SkipAwaitable* awaitable, SkipRObj* exception);
extern void SKIP_awaitableSuspend(SkipAwaitable* waiter, SkipAwaitable* waitee);
} // extern "C"
