/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/String.h"
#include "skip/external.h"
#include "skip/objects.h"
#include "skip/util.h"

#include <cstdarg>
#include <limits>
#include <deque>

namespace {

struct PosClass1 {
  int8_t a;
  int64_t b;
  skip::String c;
  SkipFloat d;
  bool e;
};

struct PosClass2 {
  bool a;
  bool b;
  int64_t c;
};

struct NamedClass1 {
  skip::String c;
  int64_t b;
  int8_t a;
  bool e;
  SkipFloat d;
};

struct ValueDefeatClass1 {
  skip::String a;
};
} // anonymous namespace

extern "C" {

void SKIP_PosLayout_reportClass1(skip::RObj* obj);
void SKIP_PosLayout_reportClass1(skip::RObj* obj) {
  auto* p = reinterpret_cast<PosClass1*>(obj);
  skip::String::CStrBuffer buf;
  printf(
      "PosLayout.Class 1: %zd/%zd bytes, a: %d, b: %lld, c: '%s', d: %g, e: %d\n",
      obj->userByteSize(),
      sizeof(PosClass1),
      p->a,
      (long long)p->b,
      p->c.c_str(buf),
      p->d,
      p->e);
}

void SKIP_PosLayout_reportClass2(skip::RObj* obj);
void SKIP_PosLayout_reportClass2(skip::RObj* obj) {
  auto* p = reinterpret_cast<PosClass2*>(obj);
  printf(
      "PosLayout.Class 2: %zd/%zd bytes, a: %d, b: %d, c: %lld\n",
      obj->userByteSize(),
      sizeof(PosClass2),
      p->a,
      p->b,
      (long long)p->c);
}

void SKIP_NamedLayout_reportClass1(skip::RObj* obj);
void SKIP_NamedLayout_reportClass1(skip::RObj* obj) {
  auto* p = reinterpret_cast<NamedClass1*>(obj);
  skip::String::CStrBuffer buf;
  printf(
      "NamedLayout.Class 1: %zd/%zd bytes, a: %d, b: %lld, c: '%s', d: %g, e: %d\n",
      obj->userByteSize(),
      sizeof(NamedClass1),
      p->a,
      (long long)p->b,
      p->c.c_str(buf),
      p->d,
      p->e);
}

void SKIP_ValueDefeat_reportClass1(skip::RObj* obj);
void SKIP_ValueDefeat_reportClass1(skip::RObj* obj) {
  auto* p = reinterpret_cast<ValueDefeatClass1*>(obj);
  skip::String::CStrBuffer buf;
  printf(
      "ValueDefeat.Class 1: %zd/%zd bytes, s: '%s'\n",
      obj->userByteSize(),
      sizeof(ValueDefeatClass1),
      p->a.c_str(buf));
}
} // extern "C"
