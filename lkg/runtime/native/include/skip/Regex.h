/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "Regex-extc.h"

namespace skip {

VTableRef Regex_static_vtable();

String Regex_get_pattern(const RObj& obj);
int64_t Regex_get_flags(const RObj& obj);
} // namespace skip
