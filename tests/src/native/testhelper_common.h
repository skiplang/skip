/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>

#include <boost/format.hpp>
#include <boost/intrusive_ptr.hpp>

#include <folly/Format.h>

// Since we're running a test here force assert to always check
#undef assert
#define assert(chk) assert_(__FILE__, __LINE__, (chk), #chk)

namespace {

void assert_(const char* file, int line, bool cond, const char* what) {
  if (!cond) {
    fprintf(stderr, "%s@%d: Assertion failed! %s\n", file, line, what);
  }
}

FOLLY_MAYBE_UNUSED void fail(const char* msg, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));
FOLLY_MAYBE_UNUSED void fail(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  abort();
}

FOLLY_MAYBE_UNUSED SkipRetValue ret_garbage() {
  SkipRetValue v;
  memset(&v, 0xcd, sizeof(v));
  return v;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_void() {
  return ret_garbage();
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_null() {
  auto ret = ret_garbage();
  ret.type = RetType::null;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_bool(SkipBool b) {
  auto ret = ret_garbage();
  ret.value.m_boolean = b;
  ret.type = RetType::boolean;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_int64(SkipInt i) {
  auto ret = ret_garbage();
  ret.value.m_int64 = i;
  ret.type = RetType::int64;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_float64(SkipFloat f) {
  auto ret = ret_garbage();
  ret.value.m_float64 = f;
  ret.type = RetType::float64;
  return ret;
}

FOLLY_MAYBE_UNUSED SkipRetValue svmi_string(const std::string& s) {
  auto ret = ret_garbage();
  ret.value.m_string = skip::String{s};
  ret.type = RetType::string;
  return ret;
}

__attribute__((__used__)) std::string repr(SkipRetValue v) {
  switch (v.type) {
    case RetType::null:
      return "null";
    case RetType::boolean:
      return v.value.m_boolean ? "true" : "false";
    case RetType::int64:
      return folly::to<std::string>(v.value.m_int64);
    case RetType::float64:
      return folly::to<std::string>(v.value.m_float64);
    case RetType::string:
      return "\"" + skip::String{v.value.m_string}.toCppString() + "\"";
    case RetType::object:
      return "object";
    case RetType::array:
      return "array";
    default:
      return folly::format("unknown type: {}", (int)v.type).str();
  }
}

skip::fast_map<std::string, size_t> computeClassMapping(
    const skip::AObj<svmi::Class*>* classes) {
  skip::fast_map<std::string, size_t> mapping;
  for (size_t i = 0; i < classes->arraySize(); ++i) {
    auto* cls = classes->at(i);
    mapping.insert(std::make_pair(cls->name.toCppString(), i));
  }
  return mapping;
}

std::vector<const svmi::Field*> computeFieldMapping(
    const skip::AObj<svmi::Class*>* classes,
    bool wantShape) {
  std::vector<const svmi::Field*> mapping;
  for (int i = 0; i < classes->arraySize(); ++i) {
    auto* cls = classes->at(i);
    if (cls->kind == svmi::ClassKind::base)
      continue;
    bool isShape =
        ((cls->kind == svmi::ClassKind::copyShape) ||
         (cls->kind == svmi::ClassKind::proxyShape));
    if (isShape != wantShape)
      continue;
    for (int j = 0; j < cls->fields->arraySize(); ++j) {
      auto* fld = cls->fields->at(j);
      mapping.push_back(fld);
    }
  }
  return mapping;
}

} // anonymous namespace

#ifdef TESTHELPER_VALUES

namespace {

std::deque<SkipRetValue> s_pendingResults;
int64_t s_resultLock = 0;

SkipRetValue nextValue() {
  if (s_pendingResults.empty()) {
    fprintf(stderr, "expected s_pendingResults to not be empty\n");
    abort();
  }
  auto next = s_pendingResults.front();
  if (!s_resultLock) {
    s_pendingResults.pop_front();
  }
  return next;
}

__attribute__((__used__)) SkipRetValue nextValue(RetType expectedType) {
  auto next = nextValue();
  if (next.type != expectedType) {
    fprintf(
        stderr,
        "Invalid type - expected %d but got %d\n",
        (int)expectedType,
        (int)next.type);
    abort();
  }
  return next;
}

std::vector<int64_t> s_gatherBuffer;

ATTR_UNUSED void pushGatherBool(bool b) {
  s_gatherBuffer.push_back(b ? 1 : 0);
}
ATTR_UNUSED void pushGatherInt(int64_t i) {
  s_gatherBuffer.push_back(i);
}
ATTR_UNUSED void pushGatherFloat(double d) {
  union {
    double d;
    int64_t i;
  } u;
  u.d = d;
  s_gatherBuffer.push_back(u.i);
}
ATTR_UNUSED void pushGatherString(std::string s) {
  s_gatherBuffer.push_back(skip::String{s}.m_sbits);
}
} // anonymous namespace

extern "C" {

void SKIP_internalPushRet(skip::RObj* value);
void SKIP_internalPushRet(skip::RObj* value) {
  s_pendingResults.push_back(SKIP_createTupleFromMixed(value));
}

void SKIP_internalPushRetUndefined();
void SKIP_internalPushRetUndefined() {
  SkipRetValue value;
  value.type = RetType::null;
  value.value.m_int64 = -1;
  s_pendingResults.push_back(value);
}

void SKIP_checkRetUsed();
void SKIP_checkRetUsed() {
  if (!s_pendingResults.empty()) {
    fprintf(stderr, "expected s_pendingResults to be empty\n");
    abort();
  }
}

void SKIP_internalSetRet(skip::RObj* value);
void SKIP_internalSetRet(skip::RObj* value) {
  s_pendingResults.push_front(SKIP_createTupleFromMixed(value));
  s_resultLock++;
}

void SKIP_internalUnsetRet();
void SKIP_internalUnsetRet() {
  s_pendingResults.pop_front();
  s_resultLock--;
}

} // extern "C"

#endif // TESTHELPER_VALUES
