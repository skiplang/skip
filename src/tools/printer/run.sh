#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

DIR="$(dirname "$0")"

ninja -C "$DIR/../../../build" skip_printer
"$DIR/../../../build/bin/skip_printer" "$@"
