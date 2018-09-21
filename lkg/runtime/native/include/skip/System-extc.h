/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "String-extc.h"
#include "objects-extc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

// typeinfo for skip::SkipException
SKIP_EXPORT(_ZTIN4skip13SkipExceptionE)

extern void SKIP_print_raw(SkipString s);
extern void SKIP_flush_stdout();

extern SkipString SKIP_Int_toString(SkipInt i);
extern SkipString SKIP_Float_toString(SkipFloat d);

extern void SKIP_print_stack_trace(void);
extern void SKIP_debug_break(void);

extern void SKIP_print_last_exception_stack_trace_and_exit(void* exc)
    __attribute__((__noreturn__));

extern void SKIP_throw(SkipRObj* exc) __attribute__((__noreturn__));

extern void SKIP_profile_start(void);
extern SkipFloat SKIP_profile_stop(void);
extern void SKIP_profile_report(void);

extern SkipInt SKIP_nowNanos(void);

extern const SkipRObj* SKIP_arguments(void);

extern SkipString SKIP_getcwd(void);

extern SkipString SKIP_getBuildVersion(void);

extern SkipRObj* SKIP_Subprocess_spawnHelper(const SkipRObj* args);

extern void SKIP_internalExit(SkipInt result) __attribute__((__noreturn__));

extern void SKIP_unreachable(void) __attribute__((__noreturn__));
extern void SKIP_unreachableWithExplanation(const char* why)
    __attribute__((__noreturn__));
extern void SKIP_unreachableMethodCall(SkipString why, SkipRObj* robj)
    __attribute__((__noreturn__));

extern void SKIP_print_error(SkipString s);
extern void SKIP_Debug_printMemoryStatistics(void);
extern void SKIP_Debug_printBoxedObjectSize(SkipRObj* o);
extern SkipInt SKIP_Debug_getLeakCounter(SkipString classname);

extern void SKIP_llvm_memcpy(char* dest, const char* src, size_t len);
extern void SKIP_llvm_memset(char* dest, char val, size_t len);
} // extern "C"

SKIP_INLINE void SKIP_llvm_memcpy(char* dest, const char* src, size_t len) {
  // llvm.memcpy.p0i8.p0i8.i64 w/ alignment=1
  memcpy(dest, src, len);
}

SKIP_INLINE void SKIP_llvm_memset(char* dest, char val, size_t len) {
  // llvm.memset.p0i8.i64 w/ alignment=1
  memset(dest, val, len);
}
