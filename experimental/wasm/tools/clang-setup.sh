#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

#
# hack to workaround hideous bug in node-tar when extracting
# prebuilt binaries of clang with wasm32 target support.
# see:
# https://github.com/dcodeIO/webassembly/issues/6
#

DIR="$(dirname "$0")"
SKIP=$DIR/../../..

PLATFORM=darwin-x64
TARGETDIR=$SKIP/third-party/assemblyscript-runtime/node_modules/webassembly/tools/bin/$PLATFORM

if [ ! -d $TARGETDIR ]; then
  echo "Target directory $TARGETDIR not found"
  echo "Please update git submbodules and run npm install in third-party/assemblyscript-runtime ?"
  exit 1
fi

echo "Installing clang with wasm32 support to $TARGETDIR:"
PKGURL="https://github.com/dcodeIO/webassembly/releases/download/0.10.0/tools-$PLATFORM.tar.gz"
cd $TARGETDIR
wget -q --show-progress $PKGURL -O - | tar -xz
echo "done."
