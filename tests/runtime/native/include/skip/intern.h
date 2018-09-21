/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "String.h"

namespace skip {

IObj* intern(const RObj* obj) noexcept;

StringPtr intern(String s) noexcept;

/**
 * Frees an object created by shallowCloneObjectIntoIntern().
 *
 * Does not decref() any referenced objects.
 */
void freeInternObject(IObj& obj);

/// The result of a deepCompare(), < 0, == 0, > 0 indicate relative ordering.
using DeepCmpResult = intptr_t;

/**
 * Returns a number less than, equal to, or greater than zero.
 *
 * If the graph rooted at r1 is considered structurally less than, equal to,
 * or greater than the graph rooted at r2, respectively. The ordering is
 * arbitrary except that it's transitive:
 *
 * If A < B and B = C then A < C
 * If A < B and B < C then A < C
 * ...etc.
 */
DeepCmpResult deepCompare(IObj* root1, IObj* root2);

/**
 * Returns true iff the graphs rooted at r1 and r2 are completely isomorphic.
 *
 * This is equivalent to calling deepCompare() and checking for 0, but faster.
 */
bool deepEqual(IObj* root1, IObj* root2);

/**
 * Calling this with true enters a special test mode where a different
 * "local hash" algorithm is used during interning, namely, one that always
 * returns zero. This helps unit tests check difficult-to-test cases.
 *
 * ONLY FOR USE BY UNIT TESTS!
 * @returns the old value of the flag.
 */
bool forceFakeLocalHashCollisionsForTests(bool forceCollisions);

/**
 * Allocates a shallow clone of obj into the intern heap, where obj must be of
 * this type, giving it with refcount 1. This does not affect the refcount of
 * any referenced objects (those refs may currently be to non-Heap objects, so
 * it would not be appropriate).
 *
 * Although this returns IObj, has the memory representation of one, and is in
 * the intern heap, it is not yet interned. That's the caller's job.
 */
IObj& shallowCloneIntoIntern(const RObj& obj);

void dumpInternStats(bool sortByCount = true);

extern const bool kEnableInternStats;
} // namespace skip
