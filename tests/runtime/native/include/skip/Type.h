/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "Type-extc.h"
#include "VTable.h"
#include "memory.h"
#include "util.h"

#include "detail/FakePtr.h"
#include "detail/Refs.h"

#include <boost/iterator/iterator_facade.hpp>

#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#include <atomic>
#include <vector>
#include <type_traits>

namespace skip {

/**
 * A "metatype". Instances of this class correspond to Skip types.
 *
 * Each type knows how to provide the runtime with the byte offsets to
 * its references so that interning and garbage collecting can work.
 */
struct Type final : private SkipGcType {
  enum class Kind {
    refClass = kSkipGcKindClass,
    array = kSkipGcKindArray,
    invocation = kSkipGcKindInvocation,
    string = kSkipGcKindString,
    cycleHandle = kSkipGcKindCycleHandle,
  };

  size_t userByteSize() const {
    return m_userByteSize;
  }

  size_t uninternedMetadataByteSize() const {
    return m_uninternedMetadataByteSize;
  }
  size_t internedMetadataByteSize() const {
    return m_internedMetadataByteSize;
  }

  size_t userPointerCount() const {
    return userByteSize() / sizeof(RObj*);
  }

  const SkipRefMaskType* refMask() const {
    return m_refMask;
  }
  uint8_t tilesPerMask() const {
    return m_tilesPerMask;
  }

  enum StateChangeType { initialize = 0, finalize = 1 };
  using StateChangeHandler = void(IObj*, StateChangeType type);
  static StateChangeHandler static_invocationOnStateChange;

  StateChangeHandler* getStateChangeHandler() const {
    return reinterpret_cast<StateChangeHandler*>(m_onStateChange);
  }

  bool operator==(const Type& o) const {
    return this == &o;
  }
  bool operator!=(const Type& o) const {
    return this != &o;
  }

  const char* name() const;

  bool hasRefs() const {
    return (m_refsHintMask & kSkipGcRefsHintMixedRefs) != 0;
  }
  bool isAllFrozenRefs() const {
    return (m_refsHintMask & kSkipGcRefsHintAllFrozenRefs) != 0;
  }
  bool hasNoMutableAliases() const {
    return (m_refsHintMask & kSkipGcRefsHintNoMutableAliases) != 0;
  }
  bool typeUsesInternTable() const {
    return (m_refsHintMask & kSkipGcRefsAvoidInternTable) == 0;
  }

  Kind kind() const {
    return (Kind)m_kind;
  }

  // Call fn(TOrFakePtr&) on each ref member, without inspecting its
  // contents. fn may modify the pointer in place if TOrFakePtr is non-const.
  // O is the object type, expected to be [const] RObj, MutableIObj, or IObj
  template <class O, class FN>
  void forEachRef(O& obj, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    using R = typename detail::RefsTraits<O>::TOrFakePtr;
    anyRef(obj, stripe, false, [&](R& ref) {
      fn(ref);
      return false;
    });
  }

  // Call fn(R&) on each non-fake ref member, where R is the non-fake pointer
  // type associated with O. e.g. RObj& -> RObj*&, etc. The reference passed
  // to fn is the slot in obj, and is mutable when obj is mutable.
  template <class O, class FN>
  void eachValidRef(O& obj, FN fn, size_t stripe = kSkipGcStripeIndex) const {
    using R = typename detail::RefsTraits<O>::TOrFakePtr;
    forEachRef(
        obj,
        [&](R& ref) {
          if (ref.isPtr())
            fn(ref.unsafeAsPtr());
        },
        stripe);
  }

  // Call fn(R&) on each ref slot, without inspecting the contents.
  // O is the object type, expected to be RObj, const RObj, or IObj
  // Stop iterating and return t if fn returns a value that converts to true;
  // otherwise return falsy.
  template <class O, class T, class FN>
  T anyRef(O& obj, size_t stripe, T falsy, FN fn) const {
    if (!hasRefs())
      return falsy;

    // ptr to refs starts at offset zero from the object, both for normal
    // objects and array objects.
    using R = typename detail::RefsTraits<O>::TOrFakePtr;
    auto refs = reinterpret_cast<R*>(&obj);
    const size_t slotCount = userPointerCount();
    size_t slotsPerMask;
    ssize_t remainingSlots;
    // this if/else diamond allows a single callsite to processSlotRefs().
    if (kind() == Kind::array) {
      slotsPerMask = slotCount * tilesPerMask();
      remainingSlots = slotCount * arraySize(obj);
    } else {
      slotsPerMask = slotCount;
      remainingSlots = slotCount;
    }
    while (remainingSlots > 0) {
      if (auto t = anySlotRefs(
              refs,
              stripe,
              std::min<size_t>(remainingSlots, slotsPerMask),
              falsy,
              fn)) {
        return t;
      }
      refs += slotsPerMask;
      remainingSlots -= slotsPerMask;
    }
    return falsy;
  }

  // Visit each pointer slot. R is the ptr-to-ref type, expected to be
  // either RObjOrFakePtr or const RObjOrFakePtr
  template <class R, class FN>
  void processSlotRefs(
      R* refs,
      const size_t stripe,
      const size_t slotCount,
      FN fn) const {
    anySlotRefs(
        refs, stripe, slotCount, false, [&](R& ref) { return fn(ref), false; });
  }

  // Visit each pointer slot. R is the ref type, expected to be
  // RObjOrFakePtr, const RObjOrFakePtr, or IObjOrFakePtr
  template <class R, class T, class FN>
  T anySlotRefs(
      R* refs,
      const size_t stripe,
      const size_t _slotCount,
      T falsy,
      FN fn) const {
    constexpr ssize_t kBitsPerMask = sizeof(SkipRefMaskType) * 8;
    auto maskPtr = &m_refMask[stripe];
    for (ssize_t slotCount = _slotCount; slotCount > 0;
         slotCount -= kBitsPerMask) {
      // If this is an array then mask might have another repeated tile.
      for (auto mask = slotCount < kBitsPerMask
               ? (*maskPtr & ((1ull << slotCount) - 1))
               : *maskPtr;
           mask != 0;
           mask &= mask - 1) {
        auto idx = __builtin_ctzll(mask);
        if (auto t = fn(refs[idx])) {
          return t;
        }
      }
      maskPtr += kSkipGcStripeCount;
      refs += kBitsPerMask;
    }
    return falsy;
  }

  static std::unique_ptr<Type> factory(
      const char* name,
      Kind kind,
      size_t userByteSize,
      const std::vector<size_t>& refOffsets,
      StateChangeHandler* onStateChange,
      size_t uninternedMetadataByteSize,
      size_t internedMetadataByteSize);

  static std::unique_ptr<Type> classFactory(
      const char* name,
      size_t userByteSize,
      const std::vector<size_t>& refOffsets,
      size_t extraMetadataSize = 0,
      StateChangeHandler* onStateChange = nullptr);

  // If you pass a non-null onStateChange then your state change handler is
  // responsible for calling InvocationType::static_invocationOnStateChange().
  static std::unique_ptr<Type> invocationFactory(
      const char* name,
      size_t userByteSize,
      const std::vector<size_t>& refOffsets,
      size_t extraMetadataSize = 0,
      StateChangeHandler* onStateChange = nullptr);

  // An array. Instances point to the first array entry, with metadata like the
  // VTable* immediately preceding that pointer.
  static std::unique_ptr<Type> arrayFactory(
      const char* name,
      size_t slotByteSize,
      const std::vector<size_t>& slotRefOffsets);

  static std::unique_ptr<char[]> makeArrayName(
      const char* arrayType,
      const char* elementType);

  static std::unique_ptr<Type> avoidInternTable(std::unique_ptr<Type> p);

  void operator delete(void*);

 private:
  Type() = delete;
  Type(const Type&) = delete;
  Type& operator=(const Type&) = delete;
  static size_t arraySize(const RObj& robj);
};

static_assert(sizeof(SkipGcType) == sizeof(Type), "");
} // namespace skip
