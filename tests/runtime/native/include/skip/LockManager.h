/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "memoize.h"
#include "set.h"

#include <utility>

namespace skip {

class ThreadLocalLockManager;

ThreadLocalLockManager& lockManager() __attribute__((const));

// For faster bookkeeping, when a lock is acquired it is given a LockToken
// that accelerates forgetting that it is currently locked.
enum class LockToken : uint8_t {
  // How many locks we can remember using a fast linear scan.
  // A Token < this value means it is stored in m_locksArray.
  kLocksArraySize = 32,

  // Token meaning "stored in m_lockSet, not m_locksArray".
  kInSet = 33,

  // Token meaning "not locked at all".
  kNotLocked = 34
};

namespace detail {

template <typename T>
struct GetLockKey {
  static const void* get(const T& t) {
    return &t.mutex();
  }
};

// Specialize for Revision, which might be locking the entire Invocation
// or might be locking just the Revision.
template <>
struct GetLockKey<const Revision> {
  static const void* get(const Revision& rev) {
    return rev.mutex().lockManagerKey();
  }
};
} // namespace detail

// Return a canonical pointer that indicates what was actually locked.
// This is needed to handle cases where locking one object delegates to
// something else -- we need to track that the delegated-to object is locked.
template <typename T>
const void* lockKey(const T& val) {
  // Canonicalize template specialization to "const T".
  return detail::GetLockKey<const T>::get(val);
}

// RAII lock guard, created by calling lockify().
template <typename T>
struct LockGuard final : private skip::noncopyable {
  LockGuard() = default;

  ~LockGuard() {
    if (owns_lock()) {
      unlock();
    }
  }

  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

  LockGuard(LockGuard&& other) noexcept : LockGuard() {
    *this = std::move(other);
  }

  LockGuard& operator=(LockGuard&& other) noexcept {
    if (owns_lock()) {
      unlock();
    }

    m_lockedObject = other.m_lockedObject;
    other.m_lockedObject = nullptr;

    m_lockToken = other.m_lockToken;
    other.m_lockToken = LockToken::kNotLocked;

    return *this;
  }

  // Name taken from std::unique_lock.
  bool owns_lock() const {
    return m_lockToken != LockToken::kNotLocked;
  }

  explicit operator bool() const {
    return owns_lock();
  }

  void unlock();

 private:
  friend class OwnerAndFlags;
  friend class ThreadLocalLockManager;
  friend struct LockGuard<const Revision>;

  void lock();

  void tryLock();

  explicit LockGuard(T& obj) : m_lockedObject(&obj) {
    lock();
  }

  // This converts an InvocationLock to a RevisionLock, for an attached
  // Revision. Locking an attach Revision actually locks the same underlying
  // mutex with the same key as the InvocationLock was using.
  //
  // WARNING: Do not call this directly, use Revision::convertLock, which
  // does some additional bookkeeping.
  template <
      typename Enable =
          typename std::enable_if<std::is_same<T, const Revision>::value>>
  LockGuard(const Revision& rev, InvocationLock&& steal)
      : m_lockedObject(&rev), m_lockToken(steal.m_lockToken) {
    steal.m_lockToken = LockToken::kNotLocked;
  }

  LockGuard(T& obj, std::try_to_lock_t) : m_lockedObject(&obj) {
    tryLock();
  }

  T* m_lockedObject = nullptr;
  LockToken m_lockToken = LockToken::kNotLocked;

  const void* key() const {
#if SKIP_DEBUG_LOCKS
    return lockKey(*m_lockedObject);
#else
    return nullptr;
#endif
  }
};

/**
 * An object that tracks which locks are held, for assertions.
 *
 * It also allows some work to be deferred until no locks are held.
 */
class ThreadLocalLockManager final : private skip::noncopyable {
  friend ThreadLocalLockManager& lockManager();

  template <typename T>
  friend struct LockGuard;

  // Only use lockManager(), do not make your own instance.
  ThreadLocalLockManager();

 public:
  // Will work enqueued now be deferred for later (because locks are held)?
  bool deferWork() const;

  template <typename T>
  LockGuard<T> lock(T& v) {
    return LockGuard<T>(v);
  }

  template <typename T>
  LockGuard<T> tryLock(T& v) {
    return LockGuard<T>(v, std::try_to_lock);
  }

  template <typename T>
  void assertLocked(const T& v) const {
    assertLockHeld(lockKey(v));
  }

  /// If SKIP_DEBUG_LOCKS is defined, returns true iff the object
  /// at address 'ptr' is currently locked by this thread.
  // If SKIP_DEBUG_LOCKS is not defined, always returns true.
  template <typename T>
  bool debugIsLocked(const T& v) const {
    return debugIsLockHeld(lockKey(v));
  }

  void safeDecref(IObj& obj);

  void safeDecref(Revision& rev);

  // Retrieve the invalidation stack for efficient pushing.
  std::vector<UpEdge>& invalidationStack();

  std::vector<InvalidationWatcher::Ptr>& invalidationWatchersToNotify();

  template <typename T>
  std::function<void()> setTestOnUnlockHook(T&& func) {
    std::function<void()> old = std::move(m_testOnUnlockHook);
    m_testOnUnlockHook = std::forward<T>(func);
    m_callTestOnUnlockHook = bool(m_testOnUnlockHook);
    return old;
  }

 private:
#if SKIP_DEBUG_LOCKS
  /// Is 'ptr' in the array of currently held locks?
  bool inArray(const void* ptr) const;
#endif

  /// If SKIP_DEBUG_LOCKS is defined, returns true iff the object
  /// at address 'ptr' is currently locked by this thread.
  // If SKIP_DEBUG_LOCKS is not defined, always returns true.
  bool debugIsLockHeld(const void* ptr) const;

  /// Assert that the object at address 'ptr' is currently locked.
  /// Does nothing if SKIP_DEBUG_LOCKS is not #defined.
  void assertLockHeld(const void* ptr) const;

  /// Remember that the object at address 'ptr' is currently locked.
  /// The LockToken later needs to be passed to noteUnlocked().
  LockToken noteLocked(const void* ptr);

  /// Forget that the object at address 'ptr' is currently locked.
  void noteUnlocked(const void* ptr, LockToken token);

  void doDeferredWorkIfSafe();

  // Stack of IObj to call decref() on when no locks are held.
  std::vector<IObj*> m_objDecrefs;

  // Stack of Revisions to call decrefAssumingNoLocksHeld() on.
  std::vector<Revision*> m_revisionDecrefs;

  // Stack of UpEdges to invalidate.
  std::vector<UpEdge> m_invalidations;

  // Revisions containing InvalidationWatchers that need to be notified.
  std::vector<InvalidationWatcher::Ptr> m_invalidationWatchersToNotify;

  uint32_t m_numLocksHeld = 0;

  // By default, there is no m_unlockHook, so it can't be called.
  // This flag also gets turned off to avoid reentering the hook while
  // calling the hook.
  bool m_callTestOnUnlockHook = false;

  // For tests: A hook that gets called whenever the number of locks
  // held transitions from nonzero to zero. This allows interesting side
  // effects to be injected at sensitive points in the code, simulating
  // multiple threads making changes but in a deterministic way.
  std::function<void()> m_testOnUnlockHook;

#if SKIP_DEBUG_LOCKS
  // Bit mask of which entries in m_locksArray are not in use.
  uint32_t m_freeSlot = ~0u;
  static_assert(
      (int)LockToken::kLocksArraySize == 32,
      "inArray() assumes all bits used.");

  // We expect to hold very few locks at a time, so for speed we use a
  // fixed-size array to remember which locks are held. We only fall back
  // to a set if the array fills up. The array stays small to avoid O(N^2).
  const void* m_locksArray[(int)LockToken::kLocksArraySize] = {
      nullptr,
  };

  // Fallback set if we somehow lock a huge number of objects at once.
  skip::fast_set<const void*> m_locksSet;
#endif
};

template <typename T>
void LockGuard<T>::lock() {
  m_lockedObject->mutex().lock();
  m_lockToken = lockManager().noteLocked(key());
}

template <typename T>
void LockGuard<T>::tryLock() {
  if (m_lockedObject->mutex().try_lock()) {
    m_lockToken = lockManager().noteLocked(key());
  }
}

template <typename T>
void LockGuard<T>::unlock() {
  assert(owns_lock());

  auto token = m_lockToken;
  m_lockToken = LockToken::kNotLocked;

  // NOTE: We must fetch the key before unlocking, as the key can
  // change while unlocked (in the case of Revision).
  auto k = key();

  m_lockedObject->mutex().unlock();
  lockManager().noteUnlocked(k, token);
}

template <typename T>
LockGuard<const T> lockify(T& v) {
  return lockManager().lock(static_cast<const T&>(v));
}

template <typename T>
LockGuard<const T> lockify(T* v) {
  assert(v != nullptr);
  return lockify(*const_cast<const T*>(v));
}

template <typename T>
LockGuard<const T> tryLockify(T& v) {
  return lockManager().tryLock(static_cast<const T&>(v));
}

template <typename T>
LockGuard<const T> tryLockify(T* v) {
  assert(v != nullptr);
  return tryLockify(*const_cast<const T*>(v));
}

template <typename R, typename T, typename F>
R withLockify(T* v, F f) {
  auto lock = lockify(v);
  return f();
}

template <typename R, typename T, typename F>
R withLockify(T& v, F f) {
  auto lock = lockify(v);
  return f();
}

template <typename T>
void assertLocked(const T& v ATTR_UNUSED) {
#if SKIP_DEBUG_LOCKS
  lockManager().assertLocked(v);
#endif
}

struct TestHookGuard {
  template <typename T>
  explicit TestHookGuard(T&& func) noexcept {
    m_oldHook = lockManager().setTestOnUnlockHook(std::forward<T>(func));
  }

  ~TestHookGuard();

 private:
  std::function<void()> m_oldHook;
};
} // namespace skip
