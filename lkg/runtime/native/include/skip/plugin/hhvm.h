/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "skip/objects-extc.h"

#include "skip/Finalized.h"

namespace skip {

// This is a handle to a HHVM HeapObject*.  It's used to both provide a way to
// tell HHVM what objects to keep alive and provide a handle that HHVM's GC can
// modify without our knowledge (but that can only happen while HHVM has
// control).
struct HhvmHandle final : IntrusiveFinalized<HhvmHandle> {
  ~HhvmHandle();

  HhvmHeapObjectPtr heapObject() const {
    return m_heapObject;
  }

  // If you modify this reference make sure to also clear
  // ObstackDetail::m_hhvmHeapObjectMappingValid (or just call
  // Obstack::updateHhvmHeapObject).
  HhvmHeapObjectPtr& heapObjectRef() {
    return m_heapObject;
  }

  HhvmHandle(HhvmHeapObjectPtr ptr, bool incref);

  friend IntrusiveFinalized<HhvmHandle>;

 private:
  // This represents an HHVM HeapObject* (ObjectData* or ArrayData*)
  HhvmHeapObjectPtr m_heapObject;
};
} // namespace skip
