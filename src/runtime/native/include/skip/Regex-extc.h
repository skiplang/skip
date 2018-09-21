/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "String-extc.h"

extern "C" {

extern SkipRObj* SKIP_Regex_initialize(SkipString pattern, SkipInt flags);

extern SkipRObj*
SKIP_String__matchInternal(SkipString str, SkipRObj* regex, SkipInt index);
} // extern "C"
