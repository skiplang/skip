#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


# This script "prepares" the binaries in the directory dist/

if ! [ $1 ]; then
    2>&1 echo "You must specify a release version"
    exit 2
fi

pathToSkip=$(dirname "${BASH_SOURCE[0]}")
distrib=$(uname -s)
installDir="$PWD/skip-$distrib-$1"

rm -Rf "$installDir"

mkdir "$installDir"
mkdir "$installDir/bin"
mkdir "$installDir/lib"

cp "$pathToSkip/sktools/install.sh" "$installDir/install.sh"
cp "$pathToSkip/sktools/INSTALL.md" "$installDir/INSTALL.md"
cp "$pathToSkip/sktools/sk" "$installDir/bin"

if ! [ -f "$pathToSkip/build/bin/static_skip_server" ]; then
    2>&1 echo "Missing static_skip_server. Did you run build-static-binaries.sh?"
fi

if ! [ -f "$pathToSkip/build/bin/static_skip_printer" ]; then
    2>&1 echo "Missing static_skip_printer. Did you run build-static-binaries.sh?"
fi

# We want to build a version of the skip_server with everything linked statically
cp "$pathToSkip/build/bin/static_skip_server" "$installDir/bin/skip_server"
cp "$pathToSkip/build/bin/static_skip_printer" "$installDir/bin/skip_printer"

cp -R src/runtime/prelude "$installDir/lib/prelude"

cp "$pathToSkip/build/src/runtime/native/lib/preamble.ll" "$installDir/lib"
cp "$pathToSkip/build/src/runtime/native/libskip_runtime.a" "$installDir/lib"
cp "$pathToSkip/build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/sk_standalone.cpp.o" "$installDir/lib"
cp "$pathToSkip/build/third-party/install/lib/libfolly.a" "$installDir/lib"

if [ $distrib == Linux ]; then
    cp "$pathToSkip/build/third-party/install/lib/libunwind.a" "$installDir/lib"
    cp "$pathToSkip/build/third-party/install/lib/libdouble-conversion.a" "$installDir/lib"
    cp "$pathToSkip/build/third-party/install/lib/libjemalloc_pic.a" "$installDir/lib"

    linuxGnuLibDir="/usr/lib/x86_64-linux-gnu"

    cp "$linuxGnuLibDir/libglog.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libgflags.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libpcre.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_thread.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_system.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_context.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_filesystem.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_chrono.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_date_time.a" "$installDir/lib"
    cp "$linuxGnuLibDir/libboost_atomic.a" "$installDir/lib"
fi

cp "$pathToSkip/sktools/sk" "$installDir/bin/sk"
cp -R "$pathToSkip/ide" "$installDir/ide"
