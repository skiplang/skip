#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

pathToSkip=$(dirname "${BASH_SOURCE[0]}")
distrib=$(uname -s)
installDir="/usr/local"

if ! [ -f "$pathToSkip/build/bin/skip_server" ]; then
    2>&1 echo "Missing skip_server. Did you run 'ninja skip_server'?"
fi

if ! [ -f "$pathToSkip/build/bin/skip_printer" ]; then
    2>&1 echo "Missing skip_printer. Did you run 'ninja skip_printer'?"
fi

cp "$pathToSkip/build/bin/skip_server" "$installDir/bin/skip_server"
cp "$pathToSkip/build/bin/skip_printer" "$installDir/bin/skip_printer"

mkdir -p "$installDir/lib/skip/"

cp -R src/runtime/prelude "$installDir/lib/skip/prelude"

cp "$pathToSkip/build/src/runtime/native/lib/preamble.ll" "$installDir/lib/skip"
cp "$pathToSkip/build/src/runtime/native/libskip_runtime.a" "$installDir/lib/skip"
cp "$pathToSkip/build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/sk_standalone.cpp.o" "$installDir/lib/skip"

if [ $distrib == Linux ]; then
    cp "$pathToSkip/build/third-party/install/lib/libjemalloc_pic.a" "$installDir/lib/skip"
fi

cp "$pathToSkip/sk" "$installDir/bin/sk"
chmod 755 "$installDir/bin/sk"
