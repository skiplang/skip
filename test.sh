#!/bin/bash -e

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

dir=$(dirname "$0")

if [[ $BACKEND == "" ]]; then
  BACKEND="js"
fi
FILES=$(cd $dir; find . \! \( \( -path ./build -o -path ./native -o -path ./.git \) -prune \)  -not -name .#\* -name '*.sk')
TARGETS=$(cd $dir/build; ninja -t targets | sed -e 's/:.*//')
RES=''
for ARG in "$@"; do
  if [[ $ARG == test_* ]]; then
    RES="$RES $ARG"
  else
    MATCHES=$(echo "$FILES" | grep `echo "$ARG" | sed -e "s/\.exp$//"` | tr / . | sed -e "s/\.\./test_$BACKEND./" | sed -e "s/\.sk//")
    if [[ $MATCHES == "" ]]; then
      echo "Cannot find any tests that match $ARG"
      exit 1
    fi
    RES="$RES $MATCHES"
  fi
done;

if [[ $RES == "" ]]; then
  echo "No tests to run. Some examples you may want to run:"
  echo
  echo "All the js tests, this is likely what you want to run before sending a PR"
  echo " ./test.sh test_js"
  echo
  echo "All the tests, very very long..."
  echo " ./test.sh test"
  echo
  echo "Run and re-record the test results"
  echo "UPDATE_BASELINE=1 ./test.sh test"
  echo
  echo "A specific test using the js backend"
  echo " ./test.sh tests/src/frontend/typechecking/type_const_double_alias.sk"
  echo
  echo "A specific test using the native backend"
  echo " BACKEND=native ./test.sh tests/src/frontend/typechecking/type_const_double_alias.sk"
  echo
  echo "A specific test using the ninja syntax"
  echo " ./test.sh test_js.tests.src.frontend.typechecking.type_const_double_alias"
  echo
  echo "All the tests that match a part of the path"
  echo " ./test.sh type_const_double_alias"
  exit 1
fi

for TARGET in $RES; do
  if [[ $(echo "$TARGETS" | grep $TARGET) == "" ]]; then
    echo "$TARGET is not a registered target by ninja. If you added that test, you need to re-run cmake to get it picked up."
    exit 1
  fi
done

cd $dir/build

ninja $RES
