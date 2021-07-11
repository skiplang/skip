/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "memory.h"

#include "detail/FakePtr.h"

#include <memory>

namespace skip {

/*
 * A VTable is a compiler-controlled structure which is generally used to store
 * pointers to virtual functions but can be used by the compiler to store any
 * per-type data it wants.
 *
 * In general the format is free-form but there are a few restrictions that the
 * runtime requires:
 *
 *   1. Object pointer bit kFrozenBit indictates a "frozen" object - so VTable
 *      jump tables must be sets of values interleaved between non-frozen and
 *      frozen functions (which may be the same).
 *
 *   2. For closures the first element of each interleaved VTable is a pointer
 *      to the closure function.  For non-closures the runtime puts no
 *      restriction on the value of the first element.
 *
 *   3. The interleaved VTables must both have a pointer to the Type as the
 *      second element.
 *
 */
struct alignas(128) VTable {
  using FunctionPtr = void (*)(RObj*);

  VTable(Type& type, FunctionPtr pFn) : m_pFn(pFn), m_type(&type) {}
  VTable(const VTable&) = delete;
  VTable& operator=(const VTable&) = delete;
  bool operator==(const VTable&) = delete;

  Type& type() const {
    return *m_type;
  }

  FunctionPtr getFunctionPtr() const {
    return m_pFn;
  }

  static constexpr size_t kFrozenBit = 8;
  static constexpr uintptr_t kFrozenMask = 1ULL << kFrozenBit;

 protected:
  VTable() = default;

 private:
  // A function pointer - closures put their code pointer here and are
  // passed their RObj.  Normal objects are free to use this as they
  // see fit.  Although we declare a return value of 'void' here the
  // specifics of the return value are up to the function definition
  // itself.
  FunctionPtr m_pFn = nullptr;
  // GC information. Note that this occupies the second VTable slot, not
  // the first, because offset 0 yields smaller machine code and we'd like
  // to use zero for common calls like closure "call" methods.
  Type* const m_type;
  // Private vtable data, filled in by the compiler
  void* m_extra[kFrozenMask / sizeof(void*) - 2] FIELD_UNUSED;
};

static_assert(sizeof(VTable) == VTable::kFrozenMask, "bad VTable size");

// This is a VTable reference which can either be a VTable* or a
// LongStringMetadata.
// TODO: If the runtime is spending a lot of checking isLongString() in cases
// where the underlying type is never a long string (i.e. an array or something)
// then we can add a VTableType to the metadata traits and specialize it.
struct VTableRef final : detail::TOrFakePtr<VTable> {
  explicit VTableRef(VTable& vtable) : detail::TOrFakePtr<VTable>(&vtable) {}
  explicit VTableRef(uintptr_t bits) : detail::TOrFakePtr<VTable>(bits) {}

  using detail::TOrFakePtr<VTable>::asPtr;
  //  VTable* asPtr() { return detail::TOrFakePtr<VTable>::asPtr(); }

  bool operator==(const VTableRef o) const {
    if (bits() == o.bits())
      return true;
    if (isPtr()) {
      // At least one is a pointer - equal needs to ignore frozen.
      return ((bits() ^ o.bits()) & ~VTable::kFrozenMask) == 0;
    }

    // Bits don't match and either it's (fake vs fake) or (fake vs ptr).
    return false;
  }
  bool operator!=(const VTableRef o) const {
    return !(*this == o);
  }

  // If the VTable is a FakePtr then return the bits of the FakePtr.  If the
  // VTable is a pointer return the bits of the unfrozen pointer.
  uintptr_t unfrozenBits() const {
    auto b = bits();
    return isFakePtr() ? b : (b & ~VTable::kFrozenMask);
  }

  bool isLongString() const {
    return isFakePtr();
  }

  // This is used by the GC to indicate an object which has already
  // been copied to a new location.
  RObj* asForwardedPtr() const {
    // A pointer is forwarded if it's NOT a FakePtr (which would indicate a
    // valid LongString) and the low bit is set (indicating an invalid actual
    // VTable pointer (which must be sizeof(void*) aligned)).
    if (bits() & 1) {
      if (auto p = asPtr()) {
        return static_cast<RObj*>(mem::sub(p, 1));
      }
    }

    return nullptr;
  }

  void setForwardedPtr(RObj* target) {
    setPtr(static_cast<VTable*>(mem::add(target, 1)));
  }

  bool isFrozen() const {
    // Need some magic to check for long string or kFrozenBit...
    return (bits() & (0x8000000000000000ULL | VTable::kFrozenMask)) != 0;
  }

  Type& type() const;
  bool isArray() const;
  VTable::FunctionPtr getFunctionPtr() const;

  friend struct RObjMetadata;
  friend struct IObjMetadata;
};

static_assert(sizeof(VTableRef) == sizeof(VTable*), "Invalid size");

// These are VTables which are constructed and used by the runtime internals or
// tests.  It has a Type* but no actual vtable data.
struct alignas(128) RuntimeVTable final {
  static std::unique_ptr<RuntimeVTable> factory(
      Type& type,
      VTable::FunctionPtr pFn = nullptr);

  VTable& vtable() {
    return m_vtable;
  }
  VTable& vtableFrozen() {
    return m_vtableFrozen;
  }

  void operator delete(void* p);

 private:
  VTable m_vtable FIELD_UNUSED;
  // Note that because of the alignas(128) on VTable it takes 128 bytes.
  VTable m_vtableFrozen FIELD_UNUSED;

  RuntimeVTable(Type& type, VTable::FunctionPtr pFn);
};
} // namespace skip

namespace std {
template <>
struct hash<skip::VTableRef> {
  size_t operator()(skip::VTableRef r) const {
    using T = decltype(r.unfrozenBits());
    return hash<T>()(r.unfrozenBits());
  }
};
} // namespace std
