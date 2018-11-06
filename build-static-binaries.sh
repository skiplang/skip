#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

###############################################################################
# This scripts builds a static version of skip_server and skip_printer.
# Static in the sense that the resulting binaries will not attempt to load
# any dynamic library (cf -static option of clang).
#
# The only reason to build those binaries is that they will be much easier
# to distribute. When you link with a dynamic library, you need the same
# library to be available on the system that will run the binary.
#
# That can be difficult to get right, especially when you have quite a few
# of them.
###############################################################################

buildStatic() {

    echo "Building: /tmp/$1.ll"
    rm -Rf "/tmp/$1.ll"

    cat ./build/src/runtime/native/lib/preamble.ll > "/tmp/$1.ll"
    ./build/lkg/bin/skip_to_llvm --export-function-as main=skip_main "$2" >> "/tmp/$1.ll"

    if [ -f "/tmp/$1.ll" ]; then
	echo "Written: /tmp/$1.ll"
    else
	exit 2
    fi

    echo "Building: /tmp/$1.o"
    rm -Rf "/tmp/$1.o"

    clang++ -c -O3 "/tmp/$1.ll" -o "/tmp/$1_full.o"
    strip --strip-symbol=exit "/tmp/$1_full.o" -o "/tmp/$1.o"

    if [ -f "/tmp/$1.o" ]; then
	echo "Written: /tmp/$1.o"
    else
	exit 2
    fi

    echo "Building: ./build/bin/static_skip_server"
    rm -rf "./build/bin/$1"

    clang++ -static -s -o "build/bin/$1" -g ./build/src/runtime/native/CMakeFiles/skip_runtime.src.dir/src/String.cpp.o "/tmp/$1.o" ./build/src/runtime/native/CMakeFiles/sk_standalone.src.dir/src/sk_standalone.cpp.o -L./build/third-party/install/lib/ -L/usr/lib/x86_64-linux-gnu ./build/src/runtime/native/libskip_runtime.a -fPIC -DPIC -Wl,-rpath,/usr/lib/x86_64-linux-gnu -lboost_thread -lboost_context -lboost_atomic -lrt -Wl,--gc-sections -DUSE_JEMALLOC -msse4.2 -std=c++14 -O3 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fdata-sections -ffunction-sections -DFOLLY_HAVE_MALLOC_H -lfolly -lglog -lgflags -ldouble-conversion -licuuc -licui18n -licuio -licutu -licudata -ljemalloc_pic -lpcre -lboost_system -lboost_filesystem -lboost_chrono -lboost_date_time -levent -ldl -lpthread -lunwind -static -lboost_context

    if [ -f "./build/bin/$1" ]; then
	echo "Written: $PWD/build/bin/$1"
    fi
}

if ! [ -f build/bin/skip_server ]; then
    2>&1 echo "Could not find ./build/bin/skip_server"
    2>&1 echo "1- This script only works when run from the root skip dir"
    2>&1 echo "2- skip_server must be built, $ cd build && ninja skip_server"
    exit 2
fi

if ! [ -f build/bin/skip_printer ]; then
    2>&1 echo "Could not find ./build/bin/skip_printer"
    2>&1 echo "1- This script only works when run from the root skip dir"
    2>&1 echo "2- skip_printer must be built, $ cd build && ninja skip_printer"
    exit 2
fi

buildStatic static_skip_server src/server
buildStatic static_skip_printer src/tools:skip_printer
