/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <stdexcept>

#include <folly/executors/ThreadPoolExecutor.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/IOThreadPoolExecutor.h>

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

std::shared_ptr<folly::ThreadPoolExecutor> getCPUExecutor();
std::shared_ptr<folly::IOThreadPoolExecutor> getIOExecutor();

// get unique ID used to identify this process:
ssize_t getSkid();
} // namespace skip
