/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/LockManager.h"

namespace skip {

// NOTE: I tried making a non-pointer version of this but the constructor
// did not get run on mac, leaving the object in a corrupt state (all zero
// bits). However a thread_local constructor does get run for a small test
// case so it's not obvious what is different.
static thread_local ThreadLocalLockManager* t_lockManager;

// Even though thread_local sometimes appears to not run constructors
// on Mac, we also store the pointer here so it gets cleaned up on thread
// exit for platforms where thread_local works properly. This assumes
// that uninitialized zero bits is a safe state for a std::unique_ptr
// on the Mac but that seems likely. Using two variables, rather than only
// a std::unique_ptr, provides an extra check to see if the std::unique_ptr
// was constructed in this thread on each access.
static thread_local std::unique_ptr<ThreadLocalLockManager> t_destroyManager;

ThreadLocalLockManager& lockManager() {
  auto t = t_lockManager;

  if (UNLIKELY(t == nullptr)) {
    t = new ThreadLocalLockManager();
    t_lockManager = t;
    t_destroyManager.reset(t);
  }

  return *t;
}

ThreadLocalLockManager::ThreadLocalLockManager() = default;

bool ThreadLocalLockManager::deferWork() const {
  return m_numLocksHeld != 0;
}

void ThreadLocalLockManager::safeDecref(IObj& obj) {
  m_objDecrefs.push_back(&obj);
  doDeferredWorkIfSafe();
}

void ThreadLocalLockManager::safeDecref(Revision& rev) {
  m_revisionDecrefs.push_back(&rev);
  doDeferredWorkIfSafe();
}

std::vector<UpEdge>& ThreadLocalLockManager::invalidationStack() {
  // The caller should only ask for this while work is currently deferred,
  // or any appended work may never get done.
  assert(deferWork());

  return m_invalidations;
}

std::vector<InvalidationWatcher::Ptr>&
ThreadLocalLockManager::invalidationWatchersToNotify() {
  return m_invalidationWatchersToNotify;
}

#if SKIP_DEBUG_LOCKS
/// Is 'ptr' in the array of currently held locks?
bool ThreadLocalLockManager::inArray(const void* ptr) const {
  // Iterate through all of the clear bits in m_freeSlot, i.e. the slots
  // that are currently in use.
  for (auto n = ~m_freeSlot; n != 0; n &= n - 1) {
    auto index = folly::findFirstSet(n) - 1;
    if (m_locksArray[index] == ptr) {
      return true;
    }
  }
  return false;
}
#endif

bool ThreadLocalLockManager::debugIsLockHeld(const void* p ATTR_UNUSED) const {
#if SKIP_DEBUG_LOCKS
  return inArray(p) || m_locksSet.count(p);
#else
  return true;
#endif
}

/// Assert that the object at address 'ptr' is currently locked.
void ThreadLocalLockManager::assertLockHeld(const void* ptr ATTR_UNUSED) const {
#if SKIP_DEBUG_LOCKS
  if (!debugIsLockHeld(ptr)) {
    fatal("Lock is not held that should be.");
  }
#endif
}

/// Remember that the object at address 'ptr' is currently locked.
/// The LockToken later needs to be passed to noteUnlocked().
LockToken ThreadLocalLockManager::noteLocked(const void* ptr ATTR_UNUSED) {
#if !(SKIP_DEBUG_LOCKS)
  ++m_numLocksHeld;
  return static_cast<LockToken>(0);
#else
  assert(ptr != nullptr);

  if (LIKELY(!inArray(ptr))) {
    if (LIKELY(m_freeSlot != 0)) {
      // Find an array entry from the freelist and store the pointer there.
      auto index = folly::findFirstSet(m_freeSlot) - 1;
      m_freeSlot &= m_freeSlot - 1;
      m_locksArray[index] = ptr;
      ++m_numLocksHeld;
      return static_cast<LockToken>(index);
    }

    if (m_locksSet.insert(ptr).second) {
      // Array full, so put it in the slower set and return a sentinel.
      ++m_numLocksHeld;
      return LockToken::kInSet;
    }
  }

  fatal("Internal error: should have been self-deadlock acquiring lock.");
#endif
}

/// Forget that the object at address 'ptr' is currently locked.
void ThreadLocalLockManager::noteUnlocked(
    const void* ptr ATTR_UNUSED,
    LockToken token ATTR_UNUSED) {
#if SKIP_DEBUG_LOCKS
  bool success = false;

  auto index = static_cast<int>(token);

  if (LIKELY(token < LockToken::kLocksArraySize)) {
    // Put the array index back on the freelist.
    auto mask = 1u << index;
    if (LIKELY((m_freeSlot & mask) == 0 && m_locksArray[index] == ptr)) {
      m_locksArray[index] = nullptr;
      m_freeSlot |= mask;
      success = true;
    }
  } else if (token == LockToken::kInSet) {
    // Remove the pointer from the set.
    success = (m_locksSet.erase(ptr) != 0);
  }

  if (!success) {
    fatal("Internal error: unlocking incorrectly registered lock.");
  }
#endif

  --m_numLocksHeld;
  doDeferredWorkIfSafe();
}

void ThreadLocalLockManager::doDeferredWorkIfSafe() {
  if (deferWork()) {
    // Don't free things while locks are held, due to reentrancy complexity
    // and scalability of deleting huge numbers of objects with locks held.
    return;
  }

  // Lie and claim we are holding a lock, so that any recursive
  // attempts to decref more objects will instead get pushed onto our
  // stacks and get processed by this loop. This avoids some cases where
  // the program stack might otherwise get very deep with a decref chain.
  ++m_numLocksHeld;

  while (!m_objDecrefs.empty() || !m_revisionDecrefs.empty() ||
         !m_invalidations.empty()) {
    // Process this array in a way that works even if decref reenters this.
    while (!m_objDecrefs.empty()) {
      auto iobj = m_objDecrefs.back();
      m_objDecrefs.pop_back();
      decref(iobj);
    }

    while (!m_revisionDecrefs.empty()) {
      auto rev = m_revisionDecrefs.back();
      m_revisionDecrefs.pop_back();
      rev->decrefAssumingNoLocksHeld();
    }

    while (!m_invalidations.empty()) {
      UpEdge edge = m_invalidations.back();
      m_invalidations.pop_back();

      Revision* target = edge.target();
      target->invalidate(edge.index());
      target->decref();
    }
  }

  // If the stacks have gotten unreasonably huge, throw them away, so
  // we don't keep a huge high water mark forever.
  if (m_objDecrefs.capacity() > 1000) {
    m_objDecrefs.shrink_to_fit();
  }

  if (m_revisionDecrefs.capacity() > 1000) {
    m_revisionDecrefs.shrink_to_fit();
  }

  if (m_invalidations.capacity() > 1000) {
    m_invalidations.shrink_to_fit();
  }

  // Undo fake lock grab above.
  --m_numLocksHeld;

  assert(m_numLocksHeld == 0);

  // Allow tests to insert a hook that gets called when all locks are dropped.
  if (UNLIKELY(m_callTestOnUnlockHook)) {
    Context::Guard ctxGuard(nullptr);
    m_callTestOnUnlockHook = false;

    m_testOnUnlockHook();

    m_callTestOnUnlockHook = bool(m_testOnUnlockHook);
  }
}

TestHookGuard::~TestHookGuard() {
  lockManager().setTestOnUnlockHook(std::move(m_oldHook));
}
} // namespace skip
