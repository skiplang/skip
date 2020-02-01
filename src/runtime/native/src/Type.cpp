/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Type.h"

#include "skip/intern.h"
#include "skip/memoize.h"
#include "skip/objects.h"
#include "skip/String.h"
#include "skip/util.h"

#include <cassert>
#include <cstddef>

namespace skip {

namespace {

size_t
computeRefMaskByteSize(const size_t slots, bool hasRefs, bool isAllFrozenRefs) {
  if (!hasRefs) {
    return 0;
  }
  // Each slot is represented by a bit in the refmask.
  size_t maskSize = roundUp(slots, detail::kBitsPerRefMask) /
      detail::kBitsPerRefMask * (kSkipGcStripeCount * sizeof(SkipRefMaskType));
  if (maskSize != 0 && isAllFrozenRefs) {
    // The compiler doesn't emit the last mask word when everything is frozen,
    // as that word would hold data about unfrozen pointers. Since there
    // aren't any, that word won't be used by the runtime.
    maskSize -= sizeof(SkipRefMaskType);
  }
  return maskSize;
}

void initializeRefMask(
    SkipGcType& type,
    const std::vector<size_t>& refOffsets,
    const size_t slots) {
  const bool isArray = type.m_kind == kSkipGcKindArray;
  type.m_tilesPerMask = 0;
  do {
    for (auto refOffset : refOffsets) {
      const size_t slot = refOffset / sizeof(RObj*);
      assert(refOffset % sizeof(RObj*) == 0);
      assert(slot < slots);
      size_t index =
          (slot + type.m_tilesPerMask * slots) / detail::kBitsPerRefMask;
      size_t offset =
          (slot + type.m_tilesPerMask * slots) % detail::kBitsPerRefMask;
      type.m_refMask[index * kSkipGcStripeCount + kSkipGcStripeIndex] |=
          (SkipRefMaskType)1 << offset;
      type.m_refMask[index * kSkipGcStripeCount + kSkipFreezeStripeIndex] =
          type.m_refMask[index * kSkipGcStripeCount + kSkipGcStripeIndex];
    }

    ++type.m_tilesPerMask;
  } while (isArray &&
           (((type.m_tilesPerMask + 1) * slots) <= detail::kBitsPerRefMask));
}

/**
 * Ensure that the byte offsets array for refs in an object are
 * properly sorted, aligned and unique, and that they are "inside" the object.
 */
#ifndef NDEBUG
void verifyRefOffsets(const std::vector<size_t>& refOffsets, size_t byteSize) {
  size_t next = 0;
  for (size_t offset : refOffsets) {
    // Make sure all offsets are properly aligned.
    assert(offset % alignof(RObj*) == 0);

    // Make sure refs are sorted and unique.
    assert(offset >= next);
    next = offset + sizeof(RObj*);

    // Catch wraparound weirdness.
    assert(next > offset);
  }

  // Make sure refs are all inside the object.
  assert(next <= byteSize);
}
#endif
} // anonymous namespace

void Type::operator delete(void* p) {
  ::free(p);
}

std::unique_ptr<Type> Type::factory(
    const char* name,
    Kind kind,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    StateChangeHandler* onStateChange,
    size_t uninternedMetadataByteSize,
    size_t internedMetadataByteSize) {
#ifndef NDEBUG
  verifyRefOffsets(refOffsets, userByteSize);
#endif
  assert(uninternedMetadataByteSize >= sizeof(RObjMetadata));
  assert(internedMetadataByteSize >= sizeof(IObjMetadata));

  userByteSize = roundUp(userByteSize, sizeof(RObj*));
  const size_t slotPointerCount =
      (refOffsets.empty() ? 0 : userByteSize / sizeof(RObj*));

  size_t rawSize = sizeof(SkipGcType);
  rawSize +=
      computeRefMaskByteSize(slotPointerCount, !refOffsets.empty(), false);
  const size_t nameOffset = rawSize;
  rawSize += strlen(name) + 1;

  void* raw = ::calloc(rawSize, 1);
  if (!raw)
    throw std::bad_alloc();

  auto ctype = std::unique_ptr<SkipGcType, decltype(::free)*>(
      static_cast<SkipGcType*>(raw), ::free);

  // Copy the name to the end.
  strcpy(static_cast<char*>(raw) + nameOffset, name);
  ctype->m_hasName = true;

  ctype->m_kind = static_cast<uint8_t>(kind);

  ctype->m_refsHintMask = 0;
  if (slotPointerCount)
    ctype->m_refsHintMask |= kSkipGcRefsHintMixedRefs;

  ctype->m_userByteSize = userByteSize;
  ctype->m_uninternedMetadataByteSize = uninternedMetadataByteSize;
  ctype->m_internedMetadataByteSize = internedMetadataByteSize;
  ctype->m_onStateChange =
      reinterpret_cast<void (*)(IObj*, ESkipGcStateChangeType)>(onStateChange);

  if (slotPointerCount != 0) {
    initializeRefMask(*ctype, refOffsets, slotPointerCount);
  }

  // This can't use static_pointer_cast<> because of the deleter on ctype.
  auto p = std::unique_ptr<Type>(static_cast<Type*>(ctype.release()));
  assert(strcmp(name, p->name()) == 0);
  return p;
}

std::unique_ptr<Type> Type::classFactory(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    size_t extraMetadataSize,
    StateChangeHandler* onStateChange) {
  extraMetadataSize = roundUp(extraMetadataSize, sizeof(std::max_align_t));
  return factory(
      name,
      Kind::refClass,
      userByteSize,
      refOffsets,
      onStateChange,
      sizeof(RObjMetadata) + extraMetadataSize,
      sizeof(IObjMetadata) + extraMetadataSize);
}

std::unique_ptr<Type> Type::invocationFactory(
    const char* name,
    size_t userByteSize,
    const std::vector<size_t>& refOffsets,
    size_t extraMetadataSize,
    StateChangeHandler* onStateChange) {
  return Type::factory(
      name,
      Kind::invocation,
      userByteSize,
      refOffsets,
      onStateChange ? onStateChange : static_invocationOnStateChange,
      Invocation::kMetadataSize + extraMetadataSize,
      Invocation::kMetadataSize + extraMetadataSize);
}

std::unique_ptr<Type> Type::arrayFactory(
    const char* name,
    size_t slotByteSize,
    const std::vector<size_t>& slotRefOffsets) {
  assert(slotByteSize % sizeof(RObj*) == 0);
  return Type::factory(
      name,
      Kind::array,
      slotByteSize,
      slotRefOffsets,
      nullptr,
      sizeof(AObjMetadata),
      sizeof(IObjMetadata));
}

std::unique_ptr<Type> Type::avoidInternTable(std::unique_ptr<Type> p) {
  p->m_refsHintMask |= kSkipGcRefsAvoidInternTable;
  return p;
}

std::unique_ptr<char[]> Type::makeArrayName(
    const char* arrayType,
    const char* elementType) {
  const size_t arrayTypeLen = strlen(arrayType);
  const size_t elementTypeLen = strlen(elementType);
  const size_t len = arrayTypeLen + 1 + elementTypeLen + 1 + 1;
  auto res = std::make_unique<char[]>(len);
  snprintf(res.get(), len, "%s[%s]", arrayType, elementType);
  return res;
}

const char* Type::name() const {
  if (m_hasName) {
    // The name is stored just after the masks.
    auto maskSize = computeRefMaskByteSize(
        userPointerCount(), hasRefs(), isAllFrozenRefs());
    return reinterpret_cast<const char*>(mem::add(&m_refMask, maskSize));
  } else {
    // Multiple skip classes share the same GC info so it does not necessarily
    // have a useful name to provide.
    return "<skip class>";
  }
}

void Type::static_invocationOnStateChange(
    IObj* obj,
    Type::StateChangeType type) {
  if (type == Type::StateChangeType::finalize) {
    Invocation::fromIObj(*obj).~Invocation();
  }
}

size_t Type::arraySize(const RObj& robj) {
  return static_cast<const AObjBase&>(robj).arraySize();
}

void RuntimeVTable::operator delete(void* p) {
  ::free(p);
}

std::unique_ptr<RuntimeVTable> RuntimeVTable::factory(
    Type& type,
    VTable::FunctionPtr pFn) {
  void* p = allocAligned(sizeof(RuntimeVTable), VTable::kFrozenMask * 2);
  return std::unique_ptr<RuntimeVTable>(new (p) RuntimeVTable(type, pFn));
}

RuntimeVTable::RuntimeVTable(Type& type, VTable::FunctionPtr pFn)
    : m_vtable(type, pFn), m_vtableFrozen(type, pFn) {
  static_assert(sizeof(VTable) == VTable::kFrozenMask, "invalid vtable size");
  static_assert(offsetof(RuntimeVTable, m_vtable) == 0, "invalid offset");
  static_assert(
      offsetof(RuntimeVTable, m_vtableFrozen) == VTable::kFrozenMask,
      "invalid offset");
}
} // namespace skip

void SKIP_invocationOnStateChange(skip::IObj* obj, SkipInt changeType) {
  // Guarantee we run the Invocation destructor.
  skip::Type::static_invocationOnStateChange(
      obj, (skip::Type::StateChangeType)changeType);
}
