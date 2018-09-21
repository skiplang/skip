/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "detail/FakePtr.h"
#include "detail/extc.h"
#include "skip/fwd.h"

extern "C" {

DEFINE_TYPE(SkipRObj, skip::RObj);
DEFINE_TYPE(SkipIObj, skip::IObj);

DEFINE_PTR_TYPE(SkipRObjOrFakePtr, skip::RObjOrFakePtr);
DEFINE_PTR_TYPE(SkipIObjOrFakePtr, skip::IObjOrFakePtr);
} // extern "C"
