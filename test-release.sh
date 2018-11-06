#!/bin/bash

usage() {
    2>&1 echo "Usage: test-release.sh path_to_skip_install path_to_skip_source"
    exit 2
}

if ! [ -d "$1" ]; then
    usage
fi

if ! [ -d "$2" ]; then
    usage
fi

bin="$1/bin"

testOK() {
    text=$(printf "TEST ${1}\n")
    passed=$(printf "\033[0;32mPASSED\033[0m")
    printf '%-70s %s\n' "$text" "$passed"
}

testFAILED() {
    text=$(printf "TEST ${1}\n")
    failed=$(printf "\033[0;31mFAILED\033[0m")
    printf '%-70s %s\n' "$text" "$failed"
    exit 2
}

# First, let's make sure no other server is running

pids=`ps xa | grep skip_server | grep -v "grep" | awk '{ print $1 }' 2> /dev/null`
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

echo "fun main(): void { print_string(\"Hello world!\") }" > "$dir/test.sk"
sync

# Make sure we get an error if the directory has not been initialized
if "$bin/sk" start "$dir" 2> /dev/null > /dev/null; then
    2>&1 echo \
    testFAILED "starting the server on an empty project should be an error!"
else
    testOK "init"
fi

"$bin/sk" init "$dir"
"$bin/sk" start "$dir" > /dev/null 2> /dev/null

# Leave a bit of time for the server to start
sleep 1

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    testOK "'check' command should not report an error"
else
    code=$?
    2>&1 echo "The server should NOT have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
    testFAILED "The server should not have reported an error"
fi

# Modify the file, check that there is an error

echo "fun main(): void { print_string(\"Hello world!\"); 0 }" > "$dir/test.sk"
sync

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    code=$?
    2>&1 echo "The server should have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
    testFAILED "The server should have reported an error"
else
    testOK "'check' command should report an error"
fi

# Remove the error, make sure the error goes away

echo "fun main(): void { print_string(\"Hello world!\") }" > "$dir/test.sk"
sync

if "$bin/sk" check "$dir" > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    testOK "'check' command error should have gone away"
else
    code=$?
    2>&1 echo "the server should NOT have reported an error"
    2>&1 echo "Instead, it produced this (on stdout): "
    2>&1 cat "$dir/check_stdout"
    2>&1 echo "Instead, it produced this (on stderr): "
    2>&1 cat "$dir/check_stderr"
    2>&1 cat "Exit code was: $code"
    testFAILED "The server should NOT have reported an error"
fi

# Link with a C file (using ccopts) make sure it works.

echo "@cpp_extern native fun hello(): void;" > "$dir/test.sk"
echo "fun main(): void { hello() }" >> "$dir/test.sk"
echo "#include<stdio.h>" > "$dir/hello.c"
echo "void SKIP_hello() { printf(\"Hello from C!\"); }" >> "$dir/hello.c"
echo "$dir/hello.o" > "$dir/.ccopts"
sync

gcc -c "$dir/hello.c" -o "$dir/hello.o"

"$bin/sk" build "$dir" > /dev/null

if "$bin/sk" run "$dir" | grep --quiet "Hello" 2> /dev/null > /dev/null; then
    testOK "C binding worked"
else
    testFAILED "failed to say hello from C"
fi


# Stop the server, make sure it stops
"$bin/sk" stop "$dir" > /dev/null

pids=`ps xa | grep skip_server | grep -v "grep" | awk '{ print $1 }' 2> /dev/null`
if [ -z pids ]; then
    testFAILED "the server should have stopped"
else
    testOK "'stop' command"
fi

# Run from start, make sure it does say hello
if "$bin/sk" run "$dir" | grep --quiet "Hello" 2> /dev/null > /dev/null; then
    testOK "'sk run *dir*' command should work from scratch"
else
    testFAILED "failed to say hello"
fi

# Stop the server
"$bin/sk" stop "$dir" > /dev/null

# Cwd into the directory and make sure everything works with the defaults.
cd "$dir"
if "$bin/sk" | grep --quiet "Hello"; then
    testOK "Test sk no args"
else
    testFAILED "failed to say hello"
fi

if "$bin/sk" check > "$dir/check_stdout" 2> "$dir/check_stderr"; then
    testOK "'check' command works with implicit directory"
else
    testFAILED "'check' command failed with implicit dir"
fi

# Checking that we can pass arguments to the binary

echo "fun main(): void { print_string(arguments()[0]) }" > "$dir/test.sk"
sync

if "$bin/sk" run "$dir" "Hello"| grep --quiet "Hello"; then
    testOK "Test sk with argument on command line"
else
    testFAILED "failed to say hello"
fi

if "$bin/sk" --args "Hello"| grep --quiet "Hello"; then
    testOK "Test sk with argument on command line"
else
    testFAILED "failed to say hello"
fi

# Stop the server
"$bin/sk" stop "$dir" > /dev/null
