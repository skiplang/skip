/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "objects.h"

namespace skip {

// WARNING: These must match their Skip equivalents.
enum { kAwaitableValueMarker = -1, kAwaitableExceptionMarker = -3 };

// Stored in m_exception to indicate that this Awaitable is one that
// the containing Process's Context is waiting for.
enum { kContextIsAwaitingThis = -1 };

// WARNING: Must line up with Skip's AwaitableBase type.
// See documentation in docs/developer/Awaitable-internals.md
struct Awaitable : RObj {
  // Linked list of waiters or completion status (see Awaitable-internals.md).
  RObjOrFakePtr m_continuation;

  // next in intrusive linked list (e.g. for a m_continuation waiters list).
  RObj* m_nextAwaitable;

  // If m_continuation is kAwaitableExceptionMarker, this field holds
  // the Exception that was thrown.
  //
  // Else it is either nullptr (the initial state), or kContextIsAwaitingThis,
  // indicating that the Context for the containing Process is waiting on
  // this Awaitable to be complete.
  RObjOrFakePtr m_exception;
};

Awaitable* callMemoizeThunk(const RObj* obj);

Awaitable* handleToAwaitable(RObjHandle& handle);
} // namespace skip
