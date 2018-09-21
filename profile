#!/bin/bash -e

DIR=$(dirname "$0")
PROFILER="$DIR/tools/profiler"

if echo "$OSTYPE" | grep -q "linux"; then
    "$PROFILER/linux_profile" "$@"
elif echo "$OSTYPE" | grep -q "darwin"; then
    "$PROFILER/osx_profile" "$@"
else
    echo "Operating system not supported."
fi
