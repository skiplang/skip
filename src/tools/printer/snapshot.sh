#!/bin/bash

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

DIR="$(dirname "$0")"
cd "$DIR/../../../build"

# To run all the tests
# ./snapshot.sh
#
# To re-record all the tests:
# ./snapshot.sh -u
#
# To only run tests that match a pattern (with grep)
# ./snapshot.sh pattern
#
# To only run tests that match a pattern and update those snapshots
# ./snapshot.sh -u pattern

GREP="$1"
UPDATE=0
if [ "$1" == "-u" ]; then
  UPDATE=1
  GREP="$2"
fi

ninja skip_printer && \
for file in `find ../src/tools/printer/tests -name '*.sk.in' | grep "$GREP"`; do
  snap=`echo "$file" | perl -pe 's/.sk.in/.snap.sk/'`
  echo $file | perl -pe 's/.+tests/tests/'
  ./bin/skip_printer $file > tmp1;
  if [ \( ! -f "$snap" \) -o \( "$UPDATE" == 1 \) ]; then
    IGNORE=`diff $snap tmp1`
    if [ ! "$?" == "0" ]; then
      mv tmp1 $snap
      echo "Snapshot updated!"
    fi
  else
    git diff --no-index $snap tmp1
  fi;
done
