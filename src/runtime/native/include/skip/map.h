/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

namespace skip {

// use this when you have non-movable values, need reference or iterator
// stability on rehash, or want pay-as-you-go allocation for large values.
template <class T, class U, class V = std::hash<T>, class W = std::equal_to<T>>
using node_map = std::unordered_map<T, U, V, W>;

// Use this when you don't require ref/iter stability, and want to let
// folly choose between F14ValueMap or F14VectorMap
template <class T, class U, class V = std::hash<T>, class W = std::equal_to<T>>
using fast_map = std::unordered_map<T, U, V, W>;
} // namespace skip
