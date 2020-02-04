/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "Obstack-extc.h"
#include "memory.h"
#include "Task.h"
#include "detail/Refs.h"

#include <functional>
#include <memory>
#include <mutex>

namespace skip {

/*
 * Holder for an RObj* that keeps it alive as a root, without preventing
 * the underlying object from being moved. Also encapsulates the owner
 * obstack, so objects can be migrated between obstacks transparently.
 */
struct RObjHandle final : private skip::noncopyable {
  ~RObjHandle();

  // the pointer returned is only valid until the next garbage collection.
  RObjOrFakePtr get() const;

  // Splice out of whatever doubly-linked list contains it.
  void unlink();

  // Safely post a lambda to the owning Process.
  template <typename T>
  void schedule(T&& function) {
    scheduleTask(std::make_unique<LambdaTask<T>>(std::move(function)));
  }

  void scheduleTask(std::unique_ptr<Task> task);

  bool isOwnedByCurrentProcess();

 private:
  friend struct TestPrivateAccess;
  friend struct ObstackDetail;
  friend struct Obstack;
  RObjHandle(RObjOrFakePtr, ProcessPtr owner);
  RObjOrFakePtr m_robj;
  RObjHandle *m_next, *m_prev;
  ProcessPtr m_owner;

  // This field only protects m_owner. The other fields cannot be examined
  // without owning the entire Process whose Obstack contains this handle.
  // The reasoning is that an external Process only needs to figure out which
  // Process to call scheduleTask on.
  std::mutex m_ownerMutex;
};

/*
 * The Obstack is a heap which grows incrementally.  Instead of freeing
 * individual allocations locations are 'noted' and allocations are freed by
 * clearing to a previous note.
 */
struct Obstack : ObstackBase {
  // C++ api
  ~Obstack();
  Obstack(); // create & initialize detail
  explicit Obstack(std::nullptr_t) noexcept; // construct uninitialized
  explicit Obstack(SkipObstackPos); // construct worker obstack
  Obstack(Obstack&&) noexcept; // move constructor
  Obstack& operator=(Obstack&&) noexcept; // move assignment
  Obstack(const Obstack&) = delete; // no copy constructor
  Obstack& operator=(const Obstack&) = delete; // no copy assignment

  // This must be called by every thread before it starts using
  // Obstack the first time, to initialize tl_obstack.
  static void threadInit();

  // Return the Obstack for the current thread (tl_obstack)
  static Obstack& cur();

  // If it has already been initialized, return the Obstack for the current
  // thread; otherwise return nullptr.
  static Obstack* curIfInitialized();

  // swap tl_obstack with obstack.
  static void swapCur(Obstack& obstack);

  // is this Obstack initialized?
  explicit operator bool() const;

  // Allocate sz bytes of memory.  If no memory is available throws
  // std::bad_alloc(). calloc() zeros the memory before returning.
  void* alloc(size_t sz) _MALLOC_ATTR(1);
  void* calloc(size_t sz) _MALLOC_ATTR(1);
  void* allocPinned(size_t sz) _MALLOC_ATTR(1);

  // Allocate instances of RObj, initializing metadata
  template <typename T, typename... ARGS>
  T* allocObject(ARGS... args);
  template <typename T, typename... ARGS>
  T* allocPinnedObject(ARGS... args);

  // Note a location on the obstack.  When this Pos is collected, any memory
  // created after it will be freed.
  SkipObstackPos note();

  // Returns memory usage since the given note.
  size_t usage(SkipObstackPos note) const;

  // delegate to detail
  IObjOrFakePtr intern(RObjOrFakePtr obj);
  IObjOrFakePtr registerIObj(IObjOrFakePtr obj);
  RObjOrFakePtr freeze(RObjOrFakePtr obj);
  void collect();
  void collect(SkipObstackPos note);
  void collect(SkipObstackPos note, RObjOrFakePtr* roots, size_t rootSize);

  // An RAII struct for managing noted sections.
  struct PosScope {
    ~PosScope() {
      // If we're in the middle of handling an exception don't clear the obstack
      // because it's likely that the exception lives on the obstack that's
      // about to be cleared.
      if (!std::uncaught_exception()) {
        m_ob.collect(m_pos);
      }
    }
    explicit PosScope(Obstack& ob) : m_ob(ob), m_pos(ob.note()) {}
    PosScope(Obstack& ob, SkipObstackPos note) : m_ob(ob), m_pos(note) {}
    PosScope() : PosScope(Obstack::cur()) {}

   private:
    Obstack& m_ob;
    const SkipObstackPos m_pos;
  };

#ifndef NDEBUG
  // Return true if the pointer is part of the current live set.  Note that for
  // large or iobj allocations it will only return true if the pointer is in the
  // Obstack itself and not a previous generation.
  bool DEBUG_isAlive(const RObj* p);
  size_t DEBUG_getSmallAllocTotal();
  size_t DEBUG_getLargeAllocTotal();
  size_t DEBUG_getIobjCount();
  size_t DEBUG_allocatedChunks();
  void verifyInvariants() const;
  void TEST_stealObjects(SkipObstackPos note, Obstack& source);
#endif

  // Handles allow external code to safely reference an obstack object
  // that may move. Multiple handles for the same object are allowed.
  // Handles are GC roots; as long as a handle to the object exists, it will
  // not be garbage-collected, but it may be moved.
  //
  // You can only make a Handle for the currently active Process.
  std::unique_ptr<RObjHandle> makeHandle(RObjOrFakePtr);

  // Are there any registered RObjHandles?
  bool anyHandles() const;

  // are there any non-fake/null handles?
  bool anyValidHandles() const;

 private:
  friend struct ObstackDetail;
  // allocate a small block, no trash-fill
  void* allocSmall(size_t sz) _MALLOC_ATTR(1);
  void init(void* nextAlloc);
};

extern const uint64_t kMemstatsLevel;

// C++ allocators for derived RObj with metadata
template <typename T, typename... ARGS>
T* Obstack::allocObject(ARGS... args) {
  auto& type = T::static_type();
  const size_t metadataSize = type.uninternedMetadataByteSize();
  assert(sizeof(T) <= type.userByteSize());
  const size_t size = metadataSize + type.userByteSize();
  void* const mem = alloc(size);
  auto ret = new (mem::add(mem, metadataSize)) T(std::forward<ARGS>(args)...);
  new (&ret->metadata()) typename T::MetadataType(T::static_vtable());
  return ret;
}

template <typename T, typename... ARGS>
T* Obstack::allocPinnedObject(ARGS... args) {
  auto& type = T::static_type();
  const size_t metadataSize = type.uninternedMetadataByteSize();
  assert(sizeof(T) <= type.userByteSize());
  const size_t size = metadataSize + type.userByteSize();
  void* const mem = allocPinned(size);
  auto ret = new (mem::add(mem, metadataSize)) T(std::forward<ARGS>(args)...);
  new (&ret->metadata()) typename T::MetadataType(T::static_vtable());
  return ret;
}
} // namespace skip
