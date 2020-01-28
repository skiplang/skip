/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "fwd.h"

#include "memoize.h"
#include "Obstack.h"
#include "objects.h"

#include <boost/intrusive_ptr.hpp>

namespace skip {

template <typename Derived>
struct IntrusiveFinalized : MutableIObj {
  template <typename... ARGS>
  static Derived* createNew(Obstack& obstack, ARGS... args) {
    Type& t = static_type();
    const size_t metadataSize = t.internedMetadataByteSize();
    assert(metadataSize >= sizeof(IObjMetadata));

    // We can't have a userByteSize of 0 - if we did then when we asked the
    // memory subsystem if the first non-metadata address is interned we'd
    // actually be asking about memory which is off the allocated object.
    const size_t ubs = std::max<size_t>(t.userByteSize(), 8);

    const size_t numBytes = (ubs + metadataSize);
    void* const raw = Arena::alloc(numBytes, Arena::Kind::iobj);
    FOLLY_SAFE_CHECK(raw != nullptr, "Out of memory.");

    // Initialize the metadata before we initialize the class
    auto metadata = mem::add(raw, metadataSize - sizeof(IObjMetadata));
    new (metadata) IObjMetadata(0, static_vtable());

    auto p = boost::intrusive_ptr<Derived>{
        new (mem::add(raw, metadataSize)) Derived(std::forward<ARGS>(args)...)};
    obstack.registerIObj(p.get());
    return p.get();
  }

  static Type& static_type() {
    static const std::unique_ptr<Type> singleton =
        Type::avoidInternTable(Type::classFactory(
            typeid(Derived).name(),
            sizeof(Derived),
            {},
            0,
            &IntrusiveFinalized::static_onStateChange));
    return *singleton;
  }

  static const VTableRef static_vtable() {
    static const auto singleton = RuntimeVTable::factory(static_type());
    static const VTableRef vtable{singleton->vtable()};
    return vtable;
  }

  static void static_onStateChange(IObj* obj, Type::StateChangeType type) {
    if (type == Type::StateChangeType::finalize) {
      const_cast<MutableIObj*>(obj)->cast<Derived>().~Derived();
    }
  }

 protected:
  IntrusiveFinalized() = default;
  IntrusiveFinalized(const IntrusiveFinalized&) = delete;
  IntrusiveFinalized(IntrusiveFinalized&&) = delete;
};

template <typename CppClass>
struct Finalized : IntrusiveFinalized<Finalized<CppClass>> {
  CppClass m_cppClass;

 private:
  template <typename... ARGS>
  Finalized(ARGS... args) : m_cppClass(std::forward<ARGS>(args)...) {}

  friend struct IntrusiveFinalized<Finalized<CppClass>>;
};
} // namespace skip
