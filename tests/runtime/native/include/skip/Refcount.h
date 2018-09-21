/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include <atomic>

namespace skip {

using AtomicRefcount = std::atomic<Refcount>;

/**
 * Atomically decrement the refcount if it can do so without hitting zero
 * and returns true.
 *
 * Else leaves the refcount alone and returns false.
 */
bool decrefToNonZero(AtomicRefcount& refcount);

/**
 * Atomically increment the refcount, but only if it is not zero, and
 * returns true.
 *
 * Else leaves the refcount alone and returns false.
 *
 * This may sound like a silly thing to do -- after all, when the refcount
 * hits zero, isn't it dead? But this solves a race where object A points
 * to B with a "strong" pointer and B points back to A with a weak pointer.
 * Each has a lock protecting its pointer. When A's refcount hits zero it
 * locks B and clears its weak pointer. But B may itself be locked and trying
 * to create a strong reference back to A by incrementing A's refcount.
 * If B finds that A's refcount has already hit zero, it knows A is waiting
 * on B's lock to null it out, so it simply treats that weak pointer as if
 * it were already nullptr.
 */
bool increfFromNonZero(AtomicRefcount& refcount);

/**
 * Debugging value used for freed memory.
 */
constexpr Refcount kDeadRefcountSentinel = (Refcount)-55;

/**
 * A magic refcount that means "this is object has been allocated in memory
 * reserved for interned objects, but we have not yet established whether
 * it is the canonical interned copy or not."
 */
constexpr Refcount kBeingInternedRefcountSentinel = (Refcount)-66;

/**
 * A magic refcount that means "this object is part of a cycle, and
 * its refcounting is delegated to a special CycleHandle object."
 */
constexpr Refcount kCycleMemberRefcountSentinel = (Refcount)-99;

/**
 * The maximum legal refcount (inclusive).
 */
constexpr Refcount kMaxRefcount = kCycleMemberRefcountSentinel - 1;

/**
 * Increments the refcount for the given interned object.
 */
void incref(IObj* obj) noexcept;

/**
 * Decrements the refcount for the given interned object, freeing
 * it if it hits zero.
 */
void decref(IObj* objOrCycleHandle) noexcept;
} // namespace skip
