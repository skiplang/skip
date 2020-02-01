/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "config.h"
#include "compiler.h"

#include <cassert>
#include <cstdint>
#include <cstddef>

#ifndef GEN_PREAMBLE
#include <boost/intrusive_ptr.hpp>
#endif

namespace skip {

template <class T>
class intrusive_ptr {
 private:
  typedef intrusive_ptr this_type;

 public:
  typedef T element_type;

  constexpr intrusive_ptr() : px(0) {}

  template <class U>
  intrusive_ptr(intrusive_ptr<U> const& rhs) : px(rhs.get()) {
    if (px != 0)
      intrusive_ptr_add_ref(px);
  }

  intrusive_ptr(intrusive_ptr const& rhs) : px(rhs.px) {
    if (px != 0)
      intrusive_ptr_add_ref(px);
  }

  ~intrusive_ptr() {
    if (px != 0)
      intrusive_ptr_release(px);
  }

  intrusive_ptr(T* p, bool add_ref = true) : px(p) {
    if (px != 0 && add_ref)
      intrusive_ptr_add_ref(px);
  }

  intrusive_ptr(intrusive_ptr&& rhs) : px(rhs.px) {
    rhs.px = 0;
  }

  intrusive_ptr& operator=(intrusive_ptr&& rhs) BOOST_SP_NOEXCEPT {
    this_type(static_cast<intrusive_ptr&&>(rhs)).swap(*this);
    return *this;
  }

  template <class U>
  intrusive_ptr(intrusive_ptr<U>&& rhs)

      : px(rhs.px) {
    rhs.px = 0;
  }

  template <class U>
  intrusive_ptr& operator=(intrusive_ptr<U>&& rhs) {
    this_type(static_cast<intrusive_ptr<U>&&>(rhs)).swap(*this);
    return *this;
  }

  intrusive_ptr& operator=(intrusive_ptr const& rhs) {
    this_type(rhs).swap(*this);
    return *this;
  }

  intrusive_ptr& operator=(T* rhs) {
    this_type(rhs).swap(*this);
    return *this;
  }

  T* get() const {
    return px;
  }

  T* operator->() const {
    return px;
  }

  T& operator*() const {
    return *px;
  }

  void reset() {
    this_type().swap(*this);
  }

  void reset(T* rhs) {
    this_type(rhs).swap(*this);
  }

  explicit operator bool() const {
    return px != 0;
  }

  void swap(intrusive_ptr& rhs) {
    T* tmp = px;
    px = rhs.px;
    rhs.px = tmp;
  }

 private:
  T* px;
};

template <class T, class U>
inline bool operator==(intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) {
  return a.get() == b.get();
}

template <class T, class U>
inline bool operator==(intrusive_ptr<T> const& a, U* b) {
  return a.get() == b;
}

template <class T, class U>
inline bool operator!=(intrusive_ptr<T> const& a, intrusive_ptr<U> const& b) {
  return a.get() != b.get();
}

template <class T, class U>
inline bool operator!=(intrusive_ptr<T> const& a, U* b) {
  return a.get() != b;
}

template <class T>
inline bool operator!=(intrusive_ptr<T> const& a, std::nullptr_t b) {
  return a.get() != b;
}

using arraysize_t = uint32_t;
using Refcount = uint32_t;

template <typename ElementType>
struct AObj;
struct Awaitable;
struct Bucket;
struct Cell;
struct CleanupList;
struct Context;
struct MemoValue;
struct MutableCycleHandle;
struct MutableIObj;
struct Invocation;
struct LongString;
struct Obstack;
struct Process;
struct RObj;
template <typename T>
struct Refs;
struct RObjHandle;
struct String;
struct TarjanNode;
struct Transaction;
struct Type;
struct VTable;

using CycleHandle = const MutableCycleHandle;
using IObj = const MutableIObj;
#ifndef GEN_PREAMBLE
using IObjPtr = boost::intrusive_ptr<IObj>;
#endif

namespace detail {
template <typename T>
struct TOrFakePtr;
}

using RObjOrFakePtr = detail::TOrFakePtr<RObj>;
using MutableIObjOrFakePtr = detail::TOrFakePtr<MutableIObj>;
using IObjOrFakePtr = const detail::TOrFakePtr<const MutableIObj>;
} // namespace skip
