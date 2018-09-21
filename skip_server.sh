#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

rm -f /tmp/input_skip_server
mkfifo /tmp/input_skip_server
sleep 10000 > /tmp/input_skip_server&
build/bin/skip_server $1 < /tmp/input_skip_server
