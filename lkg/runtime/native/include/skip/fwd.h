/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "config.h"
#include "compiler.h"

#include <cassert>
#include <cstdint>
#include <cstddef>

#ifndef GEN_PREAMBLE
#include <boost/intrusive_ptr.hpp>
#endif

namespace skip {

using arraysize_t = uint32_t;
using Refcount = uint32_t;

template <typename ElementType>
struct AObj;
struct Awaitable;
struct Bucket;
struct Cell;
struct CleanupList;
struct Context;
struct MemoValue;
struct MutableCycleHandle;
struct MutableIObj;
struct Invocation;
struct LongString;
struct Obstack;
struct Process;
struct RObj;
template <typename T>
struct Refs;
struct RObjHandle;
struct String;
struct TarjanNode;
struct Transaction;
struct Type;
struct VTable;

using CycleHandle = const MutableCycleHandle;
using IObj = const MutableIObj;
#ifndef GEN_PREAMBLE
using IObjPtr = boost::intrusive_ptr<IObj>;
#endif

namespace detail {
template <typename T>
struct TOrFakePtr;
}

using RObjOrFakePtr = detail::TOrFakePtr<RObj>;
using MutableIObjOrFakePtr = detail::TOrFakePtr<MutableIObj>;
using IObjOrFakePtr = const detail::TOrFakePtr<const MutableIObj>;
} // namespace skip
