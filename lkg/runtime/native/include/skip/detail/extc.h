/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// extc utility definitions

// DEFINE_TYPE creates a "named" opaque type. In C code the type
// is a struct so it requires casting to other defined types. In the
// preamble it turns into void.
#ifdef GEN_PREAMBLE
// we are generating the preamble.ll that is seen by generated skip code
#define DEFINE_TYPE(NAME, CPP_NAME) using NAME = void
#define DEFINE_PTR_TYPE(NAME, CPP_NAME) using NAME = void*
#else
#define DEFINE_TYPE(NAME, CPP_NAME) using NAME = CPP_NAME
#define DEFINE_PTR_TYPE(NAME, CPP_NAME) using NAME = CPP_NAME
#endif

// force llvm to inline into generated skip code

#if defined(__GNUC__)
#define SKIP_INLINE inline __attribute__((__always_inline__))
#else
#define SKIP_INLINE inline
#endif

// This defines a name to be exported by gen_preamble's dynamic-list mode.
#ifndef SKIP_EXPORT
#define SKIP_EXPORT(sym)
#endif

using SkipBool = bool;
using SkipInt = long long;
using SkipFloat = double;
