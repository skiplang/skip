/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/objects.h"

#include "skip/memoize.h"
#include "skip/set.h"
#include "skip/String.h"

namespace skip {

// Make sure there is no local state, these are supposed to be empty dummy
// classes. Note that the size of an empty class is never zero in C++.
// It happens to be four in this case because of an alignas(4) directive.
static_assert(sizeof(IObj) <= 4, "vtable or something crept in!");

Type& VTableRef::type() const {
  if (isLongString()) {
    return LongString::static_type();
  } else {
    return asPtr()->type();
  }
}

bool VTableRef::isArray() const {
  return !isFakePtr() && (type().kind() == Type::Kind::array);
}

VTable::FunctionPtr VTableRef::getFunctionPtr() const {
  if (auto p = asPtr()) {
    return p->getFunctionPtr();
  } else {
    return nullptr;
  }
}

Type& RObj::type() const {
  return vtable().type();
}

size_t RObj::hash() const {
  auto& type = this->type();
  switch (type.kind()) {
    case Type::Kind::string:
      return static_cast<const LongString*>(this)->metadata().m_hash;
    case Type::Kind::cycleHandle:
      return static_cast<const CycleHandle*>(this)->m_hash;
    default:
      return hashMemory(this, userByteSize(), vtable().unfrozenBits());
  }
}

size_t RObj::userByteSize() const {
  auto& type = this->type();
  switch (type.kind()) {
    case Type::Kind::string: {
      auto& str = *static_cast<const LongString*>(this);
      return str.paddedSize();
    }

    case Type::Kind::array: {
      auto& arr = *static_cast<const AObjBase*>(this);
      return arr.arraySize() * type.userByteSize();
    }

    default:
      return type.userByteSize();
  }
}

void* RObj::interior() {
  // all RObj metadata are guaranteed to be at least the size of a vtable ref;
  // return a valid pointer to the start or interior of our header.
  return mem::sub(this, sizeof(VTableRef));
}

const void* RObj::interior() const {
  return mem::sub(this, sizeof(VTableRef));
}

MutableIObj::MutableIObj() {
  assert(Arena::getMemoryKind(this) == Arena::Kind::iobj);
}

namespace {
const VTableRef static_cycleHandleVTable() {
  static auto singleton = RuntimeVTable::factory(CycleHandle::static_type());
  static VTableRef vtable{singleton->vtable()};
  return vtable;
}
} // anonymous namespace

Type& MutableCycleHandle::static_type() {
  static auto singleton = Type::factory(
      typeid(MutableCycleHandle).name(),
      Type::Kind::cycleHandle,
      sizeof(CycleHandle),
      // Create one reference from the "root" field.
      {offsetof(CycleHandle, m_root)},
      nullptr,
      sizeof(IObjMetadata),
      sizeof(IObjMetadata));
  return *singleton;
}

CycleHandle& MutableCycleHandle::factory(size_t hash, IObj& root) {
  const size_t metaSize = static_type().internedMetadataByteSize();
  const size_t size = metaSize + sizeof(MutableCycleHandle);
  const auto mem = static_cast<char*>(Arena::calloc(size, Arena::Kind::iobj));
  assert(mem != nullptr);

  auto pThis = reinterpret_cast<MutableCycleHandle*>(mem + metaSize);
  IObjMetadata& metadata = reinterpret_cast<IObjMetadata*>(pThis)[-1];
  new (&metadata) IObjMetadata(1, static_cycleHandleVTable());
  metadata.m_next.m_obj = nullptr;

  pThis->m_root = &root;
  pThis->m_hash = hash;

  return *pThis;
}

void CycleHandle::verifyInvariants() const {
#if SKIP_PARANOID
  assert(m_root->isCycleMember());
  assert(&m_root->refcountDelegate() == this);

  bool sawRootPredecessor = false;

  // Track which nodes we have visited.
  skip::fast_set<IObj*> seen{m_root};

  for (std::vector<IObj*> stack{1, m_root}; !stack.empty();) {
    IObj& n = *stack.back();
    stack.pop_back();

    bool sawCycleSuccessor = false;

    n.eachValidRef([&](IObj* ref) {
      if (&ref->refcountDelegate() == this) {
        sawCycleSuccessor = true;
        sawRootPredecessor |= (ref == m_root);

        if (seen.insert(ref).second) {
          stack.push_back(ref);
        }
      }
    });

    // Every cycle member must point to another member of the cycle.
    assert(FORCE_IOBJ_DELEGATE || sawCycleSuccessor);
  }

  assert(FORCE_IOBJ_DELEGATE || sawRootPredecessor);

  // TODO: We could verify that the hash still matches, I suppose.
#endif
}

void MutableIObj::verifyInvariants() const {
  assert(currentRefcount() < kDeadRefcountSentinel);
  assert(isInterned());

  const bool fully = isFullyInterned();

  eachValidRef([&](IObj* ref ATTR_UNUSED) {
    if (fully) {
      assert(ref->isFullyInterned());
    } else {
      assert(ref->isInterned());
    }
  });

  auto& type = this->type();
  switch (type.kind()) {
    case Type::Kind::string: {
      static_cast<const LongString*>(static_cast<const RObj*>(this))
          ->verifyInvariants();
      break;
    }

    case Type::Kind::invocation: {
      Invocation::fromIObj(*this).verifyInvariants();
      break;
    }

    case Type::Kind::cycleHandle: {
      static_cast<const CycleHandle*>(this)->verifyInvariants();
      break;
    }

    default:
      break;
  }
}

void pushIObj(IObj*& stack, IObj& obj) {
  obj.next() = stack;
  stack = &obj;
}

IObj& popIObj(IObj*& stack) {
  IObj& top = *stack;
  stack = top.next();
  top.next() = nullptr; // Technically unnecessary, but let's be paranoid.
  return top;
}
} // namespace skip
