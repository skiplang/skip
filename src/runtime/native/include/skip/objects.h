/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"
#include "objects-extc.h"

#include "Arena.h"
#include "InternTable.h"
#include "Refcount.h"
#include "Type.h"
#include "VTable.h"

#include <boost/intrusive_ptr.hpp>

namespace skip {

/*
 *
 * Skip object references are always represented as a pointer to the first
 * field of the object (for objects), to the first array entry (for arrays) or
 * first character (for strings). This pointer is called the "object pointer".
 *
 * Assorted metadata precedes the "object pointer". Exactly which metadata is
 * present depends on the type of object, which can always be determined by the
 * VTableRef that immediately precedes the object pointer.
 *
 * Methods compute refs to each of these metadata fields:
 *
 * Here is the memory layout:
 *
 * <---- Raw malloc() storage start here.
 *
 * [Various fields are only present here in a Invocation; see Invocation].
 *
 * LongStrings have a size_t hash field here.
 *
 * All objects have a pointer-sized "union NextPtr" field here, used for
 * different things at different times.
 *
 * All Heap objects have a 32-bit refcount:
 *     std::atomic<Refcount> refcount;
 *
 * The 32-bit size field is only used for strings and arrays. It specifies
 * the number of array elements, not the byte size.
 *     arraysize_t size;
 *
 * All objects have this 64-bit field, from which additional type information
 * can be derived:
 *     VTableRef vtable;
 *
 *   * NOTE: Possible future optimization: If we can ensure that all vtable
 *     pointers are within the low 4GB then we can represent the VTable with 32
 *     bits and shrink the metadata size.
 *
 * <---- The object pointer (IObj* if interned) points here.
 *
 * Finally "user data", as defined by the compiler, starts here:
 *     - If an array, the first array entry (if any).
 *     - If a string, the first character (if any).
 *     - Else (object) the first field (if any).
 *
 */

/**
 * Maybe a pointer to a skip object, or maybe a pointer to a value class
 * embedded in some other object.
 */

#define WithVTable(MetadataType)                          \
  MetadataType& metadata() const {                        \
    return const_cast<MetadataType&>(                     \
        reinterpret_cast<const MetadataType*>(this)[-1]); \
  }                                                       \
                                                          \
  const VTableRef vtable() const {                        \
    return reinterpret_cast<const VTableRef*>(this)[-1];  \
  }                                                       \
                                                          \
  VTableRef& vtable() {                                   \
    return reinterpret_cast<VTableRef*>(this)[-1];        \
  }

// IMPORTANT NOTE: It's important that all metadata objects end with m_vtable
// after ALL padding.
struct RObjMetadata {
  VTableRef m_vtable;

  explicit RObjMetadata(const VTableRef vtable) : m_vtable(vtable) {}

  void clearFrozen() {
    if (!m_vtable.isLongString()) {
      m_vtable.setSBits(m_vtable.sbits() & ~VTable::kFrozenMask);
    }
  }

  void setFrozen() {
    if (!m_vtable.isLongString()) {
      m_vtable.setSBits(m_vtable.sbits() | VTable::kFrozenMask);
    }
  }

 protected:
  RObjMetadata() = default;
  RObjMetadata(const RObjMetadata&) = default;
};

static_assert(sizeof(RObjMetadata) == 8, "Unexpected size.");

/// A skip object of ref type. Not necessarily frozen.
struct RObj {
  WithVTable(RObjMetadata);
  using MetadataType = RObjMetadata;

  /**
   * Returns true iff this object has been interned. This is of course
   * not necessarily valid during the interning process itself.
   */
  inline bool isInterned() const;

  inline bool isFrozen() const {
    return vtable().isFrozen();
  }

  /**
   * Returns true iff obj has been "fully interned". Objects in the process
   * of being interned by definition return false, even though they may return
   * true for isInterned(), because they have the layout and address of an
   * interned object.
   *
   * If an object is "fully interned" we know it is the canonical version of
   * that object. Anything else could theoretically get thrown away as redundant
   * during interning.
   */
  inline bool isFullyInterned() const;

  size_t hash() const;

  // Downcasts to IObj* if this object is interned, otherwise returns nullptr.
  inline IObj* asInterned() const;

  size_t userByteSize() const;

  Type& type() const;

  // Call fn(RObjOrFakePtr&) on each ref slot.
  template <typename FN>
  void forEachRef(FN fn, size_t stripe = kSkipGcStripeIndex) {
    type().forEachRef(*this, fn, stripe);
  }

  // Call fn(const RObjOrFakePtr&) on each ref slot.
  template <typename FN>
  void forEachRef(FN fn, size_t stripe = kSkipGcStripeIndex) const {
    type().forEachRef(*this, fn, stripe);
  }

  // Call fn on each ref slot; stop and return the first truthy value returned
  // by fn, or return falsy if that never happens.
  template <class T, class FN>
  T anyRef(T falsy, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    return type().anyRef(*this, stripe, falsy, fn);
  }

  // Call fn(const RObj*&) on every slot containing a valid ref. A const
  // reference to the slot is passed to fn; taking its address is allowed.
  // Returns the first truthy value returned by fn, or else falsy.
  template <class T, class FN>
  T anyValidRef(T falsy, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    return anyRef(
        falsy,
        [&](const RObjOrFakePtr& r) {
          if (r.isPtr()) {
            if (auto t = fn(r.unsafeAsPtr())) {
              return t;
            }
          }
          return falsy;
        },
        stripe);
  }

  // call FN(RObj* const &) on every slot containing a valid ref.
  template <class FN>
  void eachValidRef(FN fn, size_t stripe = kSkipGcStripeIndex) const {
    anyValidRef(
        false, [&](const RObj* const& r) { return fn(r), false; }, stripe);
  }

  // call FN(RObj*&) on every slot containing a valid ref.
  template <class FN>
  void eachValidRef(FN fn, size_t stripe = kSkipGcStripeIndex) {
    forEachRef(
        [&](RObjOrFakePtr& r) {
          if (r.isPtr())
            fn(r.unsafeAsPtr());
        },
        stripe);
  }

  template <typename T>
  T& cast() {
    return static_cast<T&>(*this);
  }

  template <typename T>
  T& cast() const {
    return static_cast<T&>(*this);
  }

  // RObj may point exactly at the end of allocated memory, when there is
  // a header but no body. Provide an interior address safe to use with
  // address range calculations.
  void* interior();
  const void* interior() const;

 protected:
  RObj() = default;
  RObj(const RObj&) = default;
  RObj& operator=(const RObj&) = default;
  RObj(RObj&&) = default;
  RObj& operator=(RObj&&) = default;
};

#define WithArraySize()                 \
  arraysize_t arraySize() const {       \
    return metadata().m_arraySize;      \
  }                                     \
                                        \
  void setArraySize(arraysize_t size) { \
    metadata().m_arraySize = size;      \
  }

// IObjMetadata "inherits" from RObjMetadata but since it's before the object
// pointer it grows "down" - so the last fields are aligned.
struct IObjMetadata {
  // Each interned object starts with room for a pointer, which is used for
  // different purposes at different times.
  union IObjNextPtr {
    // Objects not in the InternTable can use this. Cycle members use it to
    // point to their cycle handle, and objects in the process of being freed
    // are gathered together into a linked list via this field.
    IObj* m_obj;

    // Objects currently in the InternTable use this to find the next
    // object in the same bucket.
    InternPtr m_internPtr;

    // While interning, we reuse "next" to point to more info about interning.
    TarjanNode* m_tarjanNode;

    // For toFreeStack().
    Invocation* m_invocation;
  } m_next;
  std::atomic<Refcount> m_refcount;
  arraysize_t m_arraySize;
  VTableRef m_vtable;

  IObjMetadata(Refcount refcount, const VTableRef vtable)
      : m_refcount(refcount), m_vtable(vtable) {}

  /* implicit */ operator RObjMetadata&() {
    return reinterpret_cast<RObjMetadata*>(this + 1)[-1];
  }

  /* implicit */ operator const RObjMetadata&() const {
    return reinterpret_cast<const RObjMetadata*>(this + 1)[-1];
  }

  void setFrozen() {
    if (!m_vtable.isLongString()) {
      m_vtable.setSBits(m_vtable.sbits() | VTable::kFrozenMask);
    }
  }
};

static_assert(sizeof(IObjMetadata) == 24, "Unexpected size.");

/// Interned object.
struct MutableIObj : RObj {
  WithVTable(IObjMetadata);
  WithArraySize();

  MutableIObj();

  // call FN(IObjOrFakeRef&) on every ref slot, without checking if
  // the slot contains a valid pointer.
  template <class T, class FN>
  T anyRef(T falsy, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    return type().anyRef(*this, stripe, falsy, fn);
  }

  template <class T, class FN>
  T anyValidRef(T falsy, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    return anyRef(
        falsy,
        [&](const IObjOrFakePtr& r) {
          if (r.isPtr()) {
            if (auto t = fn(r.unsafeAsPtr())) {
              return t;
            }
          }
          return falsy;
        },
        stripe);
  }

  // call FN(IObj* const &) on every valid ref slot
  template <class FN>
  void eachValidRef(FN fn, size_t stripe = kSkipGcStripeIndex) const {
    anyValidRef(false, [&](IObj* const& r) { return fn(r), false; }, stripe);
  }

  bool isCycleMember() const {
    return currentRefcount() == kCycleMemberRefcountSentinel;
  }

  IObj& refcountDelegate() const {
    // Cycle members delegate refcounting to the cycle handle, which is
    // stored in the next() field (which otherwise would go unused,
    // since cycle members are not stored in the intern table directly).
    return *(isCycleMember() ? next() : this);
  }

  bool typeUsesInternTable() const {
    return type().typeUsesInternTable();
  }

  AtomicRefcount& refcount() const {
    return metadata().m_refcount;
  }

  void setRefcount(Refcount rc) const {
    refcount().store(rc, std::memory_order_relaxed);
  }

  Refcount currentRefcount() const {
    return refcount().load(std::memory_order_relaxed);
  }

  InternPtr& internNext() const {
    return metadata().m_next.m_internPtr;
  }

  IObj*& next() const {
    return metadata().m_next.m_obj;
  }

  TarjanNode*& tarjanNode() const {
    return metadata().m_next.m_tarjanNode;
  }

  void verifyInvariants() const;
};

void intrusive_ptr_add_ref(IObj* iobj);
void intrusive_ptr_release(IObj* iobj);

/**
 * The special object to which all members of a cycle delegate their
 * reference counts, so the entire cycle lives and dies as one.
 */
struct MutableCycleHandle final : MutableIObj {
  /// The "root" of the cycle, i.e. a designated cycle member universally
  /// agreed as the starting point for hashing/equality for any isomorphic
  /// cycle.
  IObj* m_root;

  /// Hash for the entire cycle (cached since very expensive to compute).
  size_t m_hash;

  static CycleHandle& factory(size_t hash, IObj& root);

  // Helper function for Type::verifyInvariants() -- do not call directly.
  void verifyInvariants() const;

  static Type& static_type();
};

inline bool RObj::isInterned() const {
  return Arena::getMemoryKind(this) == Arena::Kind::iobj;
}

inline IObj* RObj::asInterned() const {
  return isInterned() ? static_cast<IObj*>(this) : nullptr;
}

inline bool RObj::isFullyInterned() const {
  if (!isInterned()) {
    return false;
  }
  const Refcount rc = static_cast<IObj*>(this)->currentRefcount();
  return rc != kBeingInternedRefcountSentinel;
}

void pushIObj(IObj*& stack, IObj& obj);
IObj& popIObj(IObj*& stack);

struct AObjMetadata {
  // Explicit padding to ensure that m_vtable appears at the same location
  // relative to the END of the structure as RObjMetadata.
  arraysize_t _dummy;
  arraysize_t m_arraySize;
  VTableRef m_vtable;

  explicit AObjMetadata(const VTableRef vtable) : m_vtable(vtable) {}

 protected:
  AObjMetadata() = default;
  AObjMetadata(const AObjMetadata&) = default;
};

static_assert(sizeof(AObjMetadata) == 16, "Unexpected size.");

struct AObjTraits {
  using Base = RObj;
  using MetadataType = AObjMetadata;
};

struct AObjBase : RObj {
  WithVTable(AObjMetadata);
  WithArraySize();

 protected:
  AObjBase() = default;
  AObjBase(const AObjBase&) = default;
};

/// A skip array. Not necessarily frozen.
template <typename ElementType>
struct AObj : AObjBase {
  ElementType& unsafe_at(arraysize_t index) {
    return reinterpret_cast<ElementType*>(this)[index];
  }
  const ElementType& unsafe_at(arraysize_t index) const {
    return reinterpret_cast<const ElementType*>(this)[index];
  }

  ElementType& at(arraysize_t index) {
    if (UNLIKELY(index >= this->arraySize()))
      fatal("Out of bounds");
    return unsafe_at(index);
  }
  const ElementType& at(arraysize_t index) const {
    if (UNLIKELY(index >= this->arraySize()))
      fatal("Out of bounds");
    return unsafe_at(index);
  }

  ElementType& operator[](arraysize_t index) {
    return at(index);
  }
  const ElementType& operator[](arraysize_t index) const {
    return at(index);
  }

  ElementType* begin() {
    return &unsafe_at(0);
  }
  const ElementType* begin() const {
    return &unsafe_at(0);
  }
  ElementType* end() {
    return &unsafe_at(arraySize());
  }
  const ElementType* end() const {
    return &unsafe_at(arraySize());
  }

 protected:
  AObj() = default;
  AObj(const AObj&) = default;
};
} // namespace skip
