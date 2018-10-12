#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

get_script_dir () {
     SOURCE="${BASH_SOURCE[0]}"
     # While $SOURCE is a symlink, resolve it
     while [ -h "$SOURCE" ]; do
          DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
          SOURCE="$( readlink "$SOURCE" )"
          [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
     done
     DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
     echo "$DIR"
}

root=$(get_script_dir)


# Checking that we have everything
system=$(uname -s)

if [ "Linux" == $system ]; then
    linuxGnuLibDir="/usr/lib/x86_64-linux-gnu"

    if ! [ -f "/usr/bin/clang++" ]; then
	>&2 echo "Error: could not find clang++"
	exit 8
    fi

    if ! [ -f "$linuxGnuLibDir/libglog.so" ]; then
	>&2 echo "Error: could not find libglog.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libgflags.a" ]; then
	>&2 echo "Error: could not find libgflags.a"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libpcre.a" ]; then
	>&2 echo "Error: could not find libpcre.a"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_thread.so" ]; then
	>&2 echo "Error: could not find libboost_thread.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_system.so" ]; then
	>&2 echo "Error: could not find libboost_system.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_context.so" ]; then
	>&2 echo "Error: could not find libboost_context.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_filesystem.so" ]; then
	>&2 echo "Error: could not find libboost_filesystem.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_chrono.so" ]; then
	>&2 echo "Error: could not find libboost_chrono.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_date_time.so" ]; then
	>&2 echo "Error: could not find libboost_date_time.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libboost_atomic.so" ]; then
	>&2 echo "Error: could not find libboost_atomic.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libevent.so" ]; then
	>&2 echo "Error: could not find libevent.so"
	exit 8
    fi
    if ! [ -f "$linuxGnuLibDir/libdl.so" ]; then
	>&2 echo "Error: could not find libdl.so"
	exit 8
    fi

    if [ -z "$1" ]; then
	dest="/usr"
    else
	dest="$1"
    fi

    if ! [ -d "$dest" ]; then
	mkdir "$dest"
    fi;

    if ! [ -d "$dest/lib" ]; then
	mkdir "$dest/lib"
    fi;

    if ! [ -d "$dest/bin" ]; then
	mkdir "$dest/bin"
    fi;

    rm -Rf "$dest/lib/skip"
    cp -R "$root/dist/Linux/lib/" "$dest/lib/skip"

    echo "#!/bin/bash" > "$dest/bin/sk"

    echo "clangpp=\"/usr/bin/clang++\"" >> "$dest/bin/sk"
    echo "skserver=\"$dest/bin/skserver\"" >> "$dest/bin/sk"
    echo "preamble=\"$dest/lib/skip/preamble.ll\"" >> "$dest/bin/sk"
    echo "standalone=\"$dest/lib/skip/sk_standalone.cpp.o\"" >> "$dest/bin/sk"
    echo "prelude=\"$dest/lib/skip/prelude\"" >> "$dest/bin/sk"
    echo "thirdPartyLibDir=\"$dest/lib/skip\"" >> "$dest/bin/sk"
    echo "linuxGnuLibDir=\"$dest/lib/x86_64-linux-gnu\"" >> "$dest/bin/sk"
    echo "skipRuntimeLib=\"$dest/lib/skip/libskip_runtime.a\"" >> "$dest/bin/sk"
    cat "$root/sktools/sk" >> "$dest/bin/sk"
    chmod 777 "$dest/bin/sk"

    echo "#!/bin/bash" > "$dest/bin/skserver"
    echo "skip_server=\"$dest/bin/skip_server\"" >> "$dest/bin/skserver"
    cat "$root/sktools/skserver" >> "$dest/bin/skserver"
    chmod 777 "$dest/bin/skserver"

    cp "$root/dist/Linux/bin/skip_server" "$dest/bin/skip_server"
    chmod 777 "$dest/bin/skip_server"
fi
