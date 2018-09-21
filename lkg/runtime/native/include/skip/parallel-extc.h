/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>

#include "objects-extc.h"

extern "C" {

extern SkipRObj* SKIP_parallelTabulate(SkipInt count, SkipRObj* closure);
extern SkipInt SKIP_numThreads(void);
} // extern "C"
