/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "String-extc.h"

extern "C" {

extern SkipString SKIP_open_file(SkipString path);
extern SkipString SKIP_read_stdin(void);
extern SkipString SKIP_read_stdin_bytes(SkipInt byts);
extern SkipString SKIP_read_line(void);
extern void SKIP_string_to_file(SkipString content, SkipString path);
extern void SKIP_FileSystem_appendTextFile(SkipString path, SkipString content);

extern void SKIP_FileSystem_ensure_directory(SkipString path);
extern SkipBool SKIP_FileSystem_exists(SkipString path);
extern SkipBool SKIP_FileSystem_is_directory(SkipString path);
extern SkipRObj* SKIP_FileSystem_readdir(SkipString path);
} // extern "C"
