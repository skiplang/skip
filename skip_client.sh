#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

errfile=/tmp/errors.txt

# Make sure this file exists so we can watch it.
touch $errfile

# Launch the waiter before we ask for type checking, to reduce the
# window for the race condition where the type checker finishes before
# we start waiting.
if [ Linux == "`uname -s`" ]; then
    inotifywait --quiet -e close_write $errfile > /dev/null &
else
    # If this fails to run on Mac, do "brew install fswatch".
    fswatch --one-event --event=Updated $errfile > /dev/null &
fi

echo "TYPECHECK" > /tmp/input_skip_server

# Wait for $errfile to show up.
wait

cat $errfile
