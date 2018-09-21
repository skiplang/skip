#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

DIR=$(dirname "$0")
cd "$DIR/../build/"
ninja skip_docgen && ./bin/skip_docgen --binding backend=native ../src/runtime/prelude > ../website/stdlib-metadata.json
echo "Written $DIR/stdlib-metadata.json"
