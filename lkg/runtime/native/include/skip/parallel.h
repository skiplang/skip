/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

namespace skip {

// Returns the number of worker threads we're allowed to use.  Since this
// includes the thread we're currently running on it can't be less than one.
size_t getNumThreads();

// Set the number of worker threads we're allowed to use.  Since this includes
// the thread we're currently running on it can't be less than one.
void setNumThreads(size_t sz);

// Compute (from scratch) the number of CPUs available to this process.  This
// can be forced by setting the SKIP_NUM_THREADS environment variable.
size_t computeCpuCount();
} // namespace skip
