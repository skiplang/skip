#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Helper for tests/runtime/tools/run - compiles and runs a skip program unit
# or ad-hoc .sk file using a specified backend (default 'native'). If the given
# program is a .sk file it is run with tests/runtime/prelude included. If it is
# a program unit it is run using its declared dependencies.
#
# Run with `-h` or `--help` for more information.

set -o pipefail

DIR=$(dirname "$0")
RUNTIME="$DIR/tests/runtime"
TOOLS="$RUNTIME/tools"
BUILD="$DIR/build"
BIN="$BUILD/bin"

if [ -z "${NODE}" ]; then
    if [ -f "${DIR}/build/CMakeCache.txt" ]; then
        export NODE=$(grep 'NODE:FILEPATH=' "${DIR}/build/CMakeCache.txt" | sed -e 's/NODE:FILEPATH=\(.*\)/\1/')
    else
        export NODE=node
    fi
fi

VIA_BACKEND="${VIA_BACKEND:-"$BIN"}"
exec "$RUNTIME/tools/run" --via-backend "$VIA_BACKEND" "$@"
