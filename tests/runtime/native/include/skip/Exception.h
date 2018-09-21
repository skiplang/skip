/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "String.h"

#include <exception>

namespace skip {

// Wrapper around a Skip Exception object.
//
// WARNING: The layout of this class is hardcoded into the LLVM back end.
struct SkipException : std::exception {
  explicit SkipException(RObj* exc) : m_skipException(exc) {}

  const char* what() const noexcept override;

  RObj* m_skipException;
  mutable std::string m_whatBuffer;
};
} // namespace skip
