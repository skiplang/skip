/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <stdexcept>

namespace skip {

extern void initializeSkip(int argc, char** argv);
extern void initializeNormalThread();
extern void initializeThreadWithPermanentProcess();

// Deprecated alias for initializePersistentThread().
// Delete once plugin updated to use the new name.
inline void initializeThread() {
  initializeThreadWithPermanentProcess();
}

struct SkipExitException : public std::exception {
  SkipExitException(int status) : m_status(status) {}
  const char* what() const noexcept override;
  int m_status;
};

// get unique ID used to identify this process:
ssize_t getSkid();
} // namespace skip
