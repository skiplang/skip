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
struct OptString;
} // namespace skip

extern "C" {

// These entrypoints are exported by the plugin.

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
  } value;
  // This is actually a bool wrapped in an int64_t for calling convention
  // purposes
  RetType type;
};

} // extern "C"
