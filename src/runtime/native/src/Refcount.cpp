/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Refcount.h"

#include "skip/intern.h"
#include "skip/InternTable.h"
#include "skip/memoize.h"
#include "skip/objects.h"

namespace skip {

void incref(IObj* obj) noexcept {
  AtomicRefcount& refcount = obj->refcountDelegate().refcount();
  Refcount rc ATTR_UNUSED = refcount.fetch_add(1, std::memory_order_relaxed);
  assert(rc < kMaxRefcount);
}

bool decrefToNonZero(AtomicRefcount& refcount) {
  auto rc = refcount.load(std::memory_order_relaxed);

  // This cannot be a special kind of object (e.g. cycle member, non-interned).
  assert(rc <= kMaxRefcount);

  // Optimistically try to decrement the refcount to a nonzero value.
  while (rc > 1) {
    if (LIKELY(refcount.compare_exchange_weak(
            rc, rc - 1, std::memory_order_release))) {
      return true;
    }
  }

  assert(rc > 0);

  return false;
}

bool increfFromNonZero(AtomicRefcount& refcount) {
  auto rc = refcount.load(std::memory_order_relaxed);

  // Optimistically try to increment the refcount from a nonzero value.
  while (LIKELY(rc > 0 && rc != kDeadRefcountSentinel)) {
    assert(rc < kMaxRefcount);

    if (LIKELY(refcount.compare_exchange_weak(
            rc, rc + 1, std::memory_order_relaxed))) {
      return true;
    }
  }

  return false;
}

/**
 * Decrement's obj's refcount.
 *
 * @returns true iff the refcount hit zero, in which case it is removed from
 * the intern table but the caller still needs to reclaim its memory.
 */
static bool decrefNonCycleMember(IObj& obj) noexcept {
  assert(!obj.isCycleMember());

  AtomicRefcount& refcount = obj.refcount();

  if (decrefToNonZero(refcount)) {
    return false;
  }

  if (!obj.typeUsesInternTable()) {
    return refcount.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  auto& internTable = getInternTable();

  // We hold the last strong ref to obj, but the intern table still holds
  // a weak ref so it is possible for another thread to find obj there.
  // We cannot free obj until we know that's impossible, otherwise that other
  // thread might find obj, we free the memory, then the other thread increfs
  // a dangling pointer.
  //
  // Locking obj's intern table bucket prevents another thread from finding it.
  Bucket& bucket = internTable.lockObject(obj);

  // Atomically decrement the refcount.
  auto rc = refcount.fetch_sub(1, std::memory_order_acq_rel) - 1;

  if (LIKELY(rc == 0)) {
    // The object is dead. Remove it from the intern table.
    internTable.eraseAndUnlock(bucket, obj);
    return true;
  } else {
    // Some other thread jumped in and took a ref before we could lock
    // the intern table bucket. The object remains alive, so do nothing.
    internTable.unlockBucket(bucket);

    // Make sure we did not somehow wrap around the refcount.
    assert(rc < kMaxRefcount);

    return false;
  }
}

void decref(IObj* objOrCycleMember) noexcept {
  IObj& obj = objOrCycleMember->refcountDelegate();

  if (!decrefNonCycleMember(obj)) {
    // Object is still alive, do nothing.
    return;
  }

  // Stack of dead objects whose refs need to be dropped.
  IObj* scanStack = nullptr;
  pushIObj(scanStack, obj);

  // TODO: If we want to reduce pause times, we can limit the outer
  // loop to stop once it has freed at least a certain number of objects.
  // The remaining entries in scanStack can be added to a global list that
  // must be be partially processed each time any thread allocates memory,
  // so it gets incrementally cleaned up.

  while (scanStack != nullptr) {
    // Stack of objects to scan for refs before we free() anything.
    IObj* scanBeforeFreeStack = nullptr;
    pushIObj(scanBeforeFreeStack, popIObj(scanStack));

    // Stack of objects to free() once scanning inner loop is done.
    IObj* freeStack = nullptr;

    // Inner loop: eagerly chase down all intra-cycle refs before freeing.
    while (scanBeforeFreeStack != nullptr) {
      // PopIObj the next object to scan.
      IObj& dead = popIObj(scanBeforeFreeStack);
      assert(!dead.isCycleMember());

      // Remember to free() it later.
      pushIObj(freeStack, dead);

      // Drop all refs.
      dead.eachValidRef([&](IObj* ref) {
        if (ref->isCycleMember()) {
          auto& cycleHandle = ref->refcountDelegate();

          if (cycleHandle.currentRefcount() == 0) {
            // This object is a member of an already-dead cycle and we need
            // to free it. Give it an "infinite" refcount so it no longer looks
            // like a cycle member and so that further intra-cycle refs
            // harmlessly decref it with no effect.
            ref->setRefcount(kMaxRefcount);

            // Be sure to scan its refs before anything gets freed.
            pushIObj(scanBeforeFreeStack, *ref);

            return; // continue to next ref
          }

          // Delegate refcounting to the cycle handle.
          ref = &cycleHandle;
        }

        if (decrefNonCycleMember(*ref)) {
          // Another dead object to clean up in the outer loop.
          pushIObj(scanStack, *ref);
        }
      });
    }

    // free() dead memory now. We could instead collapse the inner and outer
    // loops into one and only free() anything after everything has
    // been visited, but it seems better to free objects soon after
    // we scan them so they are still warm in the cache+TLB.
    while (freeStack != nullptr) {
      freeInternObject(popIObj(freeStack));
    }
  }
}
} // namespace skip
