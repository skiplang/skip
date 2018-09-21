#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Kill all the spawned processes on ctrl-c
trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

DIR="$(dirname "$0")"
cd $DIR;

which smcwhoami > /dev/null && \
echo Playground URL: `smcwhoami | egrep SMC_IPV6 | sed -e 's/[^:]*: \(.*\)/http:\/\/[\1]:8080/'`

yarn start &
cd playground; yarn start &
cd ..; node proxy.js
