#!/bin/bash

bin="$1/bin"

# First, let's make sure no other server is running

pids=`ps xa | grep skip_server | grep -v "grep" | awk '{ print $1 }' 2> /dev/null`
if [ -z pids ]; then
    kill -9 $pids
fi

pids=`ps xa | grep skserver | grep -v "grep" | awk '{ print $1 }' 2> /dev/null`
if [ -z pids ]; then
    kill -9 $pids
fi

pids=`ps xa | grep sleep | grep -v "grep" | awk '{ print $1 }' 2> /dev/null`
if [ -z pids ]; then
    kill -9 $pids
fi

dir=$(mktemp -d)
rm -Rf "$dir"
mkdir "$dir"

echo "fun main(): void { print_string(\"Hello from $i\") }" > "$dir/test.sk"
sync

"$bin/sk" init "$dir"
"$bin/sk" start "$dir"



# Leave a bit of time for the server to start
sleep 1

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    :
else
    code=$?
    2>&1 echo "FAILURE: the server should NOT have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
fi

# Modify the file, check that there is an error

echo "fun main(): void { print_string(\"Hello from $i\"); 0 }" > "$dir/test.sk"
sync

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    code=$?
    2>&1 echo "FAILURE: the server should have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
fi

# Remove the error, make sure the error goes away

echo "fun main(): void { print_string(\"Hello from $i\") }" > "$dir/test.sk"
sync

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    :
else
    code=$?
    2>&1 echo "FAILURE: the server should NOT have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
fi

# Stop the server, make sure it stops
