#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# set -e

DIR="$(dirname "$0")"
cd "$DIR/../../../build"

ninja skip_printer

for file in `find ../tests/src ../src ../apps -name '*.sk' | grep -v /invalid | grep -v /todo`; do
  echo "../../"$file
  ./bin/skip_printer $file > tmp1
  ./bin/skip_printer tmp1 > tmp2
  git diff --no-index tmp1 tmp2
done
