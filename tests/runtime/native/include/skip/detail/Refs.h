/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "FakePtr.h"

namespace skip {

namespace detail {

constexpr size_t kBitsPerRefMask = sizeof(SkipRefMaskType) * 8;

template <typename T>
struct RefsTraits;

template <>
struct RefsTraits<RObj> {
  using TOrFakePtr = RObjOrFakePtr;
};

template <>
struct RefsTraits<const RObj> {
  using TOrFakePtr = const RObjOrFakePtr;
};

template <>
struct RefsTraits<MutableIObj> {
  using TOrFakePtr = MutableIObjOrFakePtr;
};

template <>
struct RefsTraits<IObj> {
  using TOrFakePtr = IObjOrFakePtr;
};
} // namespace detail
} // namespace skip
