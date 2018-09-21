/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "skip/Regex.h"
#include "skip/Regex-extc.h"

#include "skip/external.h"
#include "skip/String.h"
#include "skip/Finalized.h"
#include "skip/memoize.h"

#include <pcre.h>

namespace skip {

struct Regex final {
  Regex(const String& pattern, int64_t flags)
      : m_pattern(intern(pattern)), m_flags(flags) {
    const char* errorMessage;
    int errorOffset;
    String::CStrBuffer buf;

    m_regex = pcre_compile(
        pattern.c_str(buf),
        flags,
        &errorMessage,
        &errorOffset,
        nullptr // Pointer to character tables, or nullptr to use the built-in
                // default
    );

    if (m_regex == nullptr) {
      SKIP_throwInvariantViolation(String(errorMessage));
    }

    m_extra = pcre_study(m_regex, 0, &errorMessage);
    if (m_extra == nullptr && errorMessage != nullptr) {
      pcre_free(m_regex);
      m_regex = nullptr;
      SKIP_throwInvariantViolation(String(errorMessage));
    }
  }

  RObj* matchInternal(const String& str, int64_t index) {
    String::CStrBuffer buf;
    const char* c_str = str.c_str(buf);
    std::array<int, 99 * 3> ovector;

    int ret = pcre_exec(
        m_regex,
        m_extra,
        c_str,
        str.byteSize(),
        index,
        0, // options
        ovector.data(),
        ovector.size());

    if (ret == PCRE_ERROR_NOMATCH) {
      return SKIP_createIntVector(0);
    }

    if (ret == 0) {
      SKIP_throwInvariantViolation(
          String("pcre ovector buffer size too small"));
    }

    if (ret < 0) {
      SKIP_throwInvariantViolation(String("Internal pcre error"));
    }

    auto result = SKIP_createIntVector(ret * 2);
    auto& vec = *reinterpret_cast<AObj<int64_t>*>(result);
    for (int i = 0; i < ret; ++i) {
      vec[i * 2] = ovector[i * 2];
      vec[i * 2 + 1] = ovector[i * 2 + 1];
    }
    return result;
  }

  Regex(const Regex&) = delete;
  Regex& operator=(const Regex&) = delete;

  ~Regex() {
    if (m_extra != nullptr) {
      pcre_free_study(m_extra);
    }
    if (m_regex != nullptr) {
      pcre_free(m_regex);
    }
  }

  pcre* m_regex;
  pcre_extra* m_extra;
  StringPtr m_pattern;
  int64_t m_flags;
};

VTableRef Regex_static_vtable() {
  return Finalized<Regex>::static_vtable();
}

String Regex_get_pattern(const RObj& obj_) {
  auto& obj = static_cast<const Finalized<Regex>&>(obj_);
  auto& regex = obj.m_cppClass;
  return regex.m_pattern.get();
}

int64_t Regex_get_flags(const RObj& obj_) {
  auto& obj = static_cast<const Finalized<Regex>&>(obj_);
  auto& regex = obj.m_cppClass;
  return regex.m_flags;
}
} // namespace skip

using namespace skip;

RObj* SKIP_Regex_initialize(String pattern, SkipInt flags) {
  return Finalized<Regex>::createNew(Obstack::cur(), pattern, flags);
}

RObj* SKIP_String__matchInternal(String str, RObj* regex, SkipInt index) {
  auto internal = static_cast<Finalized<Regex>*>(regex);
  return internal->m_cppClass.matchInternal(str, index);
}
