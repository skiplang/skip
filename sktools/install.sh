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
system=$(uname -s)

# If we have the exact version we want, pick that.
if [ -f "/usr/bin/clang++-6.0" ]; then
    clangpp="/usr/bin/clang++-6.0"
else
    # Otherwise, error.
    2>&1 echo "Skip needs clang++ version 6 (clang++-6.0)"
    exit 2
fi

if [ -z "$1" ]; then
    if [ "Linux" == $system ]; then
	dest="/usr"
    else
        if [ "Darwin" == $system ]; then
            dest="/usr/local"
        else
            2>&1 echo "Unsupported system"
            exit 2
        fi
    fi
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
cp -R "$root/lib/" "$dest/lib/skip"

cp "$root/bin/skip_server" "$dest/bin/skip_server"
cp "$root/bin/skip_printer" "$dest/bin/skip_printer"

if [ $system == "Darwin" ]; then

  localLibDir="/usr/local/lib"
  libDir="/usr/lib"

  if ! [ -f "$localLibDir/libglog.dylib" ]; then
      >&2 echo "Error: could not find $localLibDir/libglog.dylib"
      exit 8
  fi

  if ! [ -f "$localLibDir/libgflags.dylib" ]; then
      >&2 echo "Error: could not find $localLibDir/libgflags.dylib"
      exit 8
  fi

  if ! [ -f "$localLibDir/libpcre.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libpcre.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_thread-mt.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_thread-mt.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_system.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_system.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_context-mt.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_context-mt.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_filesystem-mt.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_filesystem-mt.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_chrono-mt.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_chrono-mt.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_date_time.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_date_time.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libboost_atomic-mt.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libboost_atomic-mt.a"
      exit 8
  fi

  if ! [ -f "$localLibDir/libevent.a" ]; then
      >&2 echo "Error: could not find $localLibDir/libevent.a"
      exit 8
  fi

  if ! [ -f "$libDir/libdl.dylib" ]; then
      >&2 echo "Error: could not find $libDir/libdl.dylib"
      exit 8
  fi

  icuLib=`find /usr -name "libicuuc.a" 2> /dev/null | head -n 1`
  icuLibDir=$(dirname "$icuLib")

  if ! [ -f $icuLib ]; then
      >&2 echo "Could not find libicuuc.a"
      exit 8
  fi
fi

echo "#!/bin/bash" > "$dest/bin/sk"

echo "clangpp=\"$clangpp\"" >> "$dest/bin/sk"
echo "preamble=\"$dest/lib/skip/preamble.ll\"" >> "$dest/bin/sk"
echo "standalone=\"$dest/lib/skip/sk_standalone.cpp.o\"" >> "$dest/bin/sk"
echo "prelude=\"$dest/lib/skip/prelude\"" >> "$dest/bin/sk"
echo "thirdPartyLibDir=\"$dest/lib/skip\"" >> "$dest/bin/sk"
echo "linuxGnuLibDir=\"$dest/lib/x86_64-linux-gnu\"" >> "$dest/bin/sk"
echo "skipRuntimeLib=\"$dest/lib/skip/libskip_runtime.a\"" >> "$dest/bin/sk"
echo "skip_server=\"$dest/bin/skip_server\"" >> "$dest/bin/sk"
echo "icuDir=\"$icuLibDir\"" >> "$dest/bin/sk"

cat "$root/bin/sk" >> "$dest/bin/sk"
chmod 755 "$dest/bin/sk"
