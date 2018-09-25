# Instructions For Building With cmake

## Table of Contents

1. [Prerequisites](#install-prerequisites)
    1. [Centos](#fb-centos-facebook-only)
    1. [Ubuntu 14.04](#ubuntu-1404)
    1. [OS X](#os-x)
1. [Configure And Build](#configure-and-build-the-repo)
1. [Adding New Source Files](#adding-new-source-files-or-targets)
1. [Cleaning Up](#cleaning-up)
1. [Compiler Development Workflow](#compiler-development-workflow)
1. [Running Unittests](#running-unittests)
    1. [Compiler](#compiler)
    1. [Runtime](#runtime)
1. [Installing](#installing)
1. [External LKG](#external-lkg)
1. [Profiling Generated Binaries](#profiling-generated-binaries)
1. [Nuclide](#nuclide)


## Install Prerequisites

### Ubuntu 14.04
    apt-get install software-properties-common wget
    wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
    add-apt-repository ppa:ubuntu-toolchain-r/test
    wget https://deb.nodesource.com/setup_7.x ; bash setup_7.x
    apt-add-repository "deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-6.0 main"
    apt-get update
    apt-get install ninja-build g++-6 libgflags-dev nodejs clang-6.0 autoconf libdwarf-dev libelf-dev libssl-dev libgoogle-glog-dev libboost1.55-all-dev libevent-dev libtool pkg-config
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-6 60 --slave /usr/bin/g++ g++ /usr/bin/g++-6
    wget https://cmake.org/files/v3.11/cmake-3.11.3-Linux-x86_64.sh
    sh cmake-3.11.3-Linux-x86_64.sh --prefix=/usr/local

### OS X

Install Homebrew
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
Install packages
```
brew tap hhvm/hhvm
brew install git-lfs ninja cmake dwarfutils libelf boost libevent gflags glog jemalloc node autoconf automake pkg-config libtool hhvm clang-format
git lfs install
```

> If any of the Homebrew formulae are already installed, ensure that your versions are: `cmake` >= 3.5.0, `jemalloc` >= 4.5.0, `node` >= 6.0.0.

Make sure there is nothing out of the ordinary that occurred in your
installation (e.g., lack of symlink creations, etc.) and do a sanity
check:

```
brew doctor
```

Make sure you are not using the version of folly installed by brew. If you see an error that says "could not find TARGET folly_sub", do the following:
```
brew unlink folly
git clean -dfx
git submodule foreach git clean -dfx
mkdir build
cd build
cmake ..
```

## Configure And Build The Repo

First, clone the repository (or if you plan to submit pull-requests, fork and then clone your fork). We use `cmake` to configure with an out-of-source build tree and `ninja` to build
it.  From the top-level directory of the repository run:

    skip$ mkdir build
    skip$ cd build
    build$ cmake ..
    <cmake output>

By default cmake will build an optimized debug build.  You can use the
following commands to change that:

    build$ cmake -DCMAKE_BUILD_TYPE=Release ..
    build$ cmake -DCMAKE_BUILD_TYPE=Debug ..

Then you should be able to run `ninja` in the `build` directory to
build or rebuild:

    build$ ninja
    <ninja output>

Often cmake will detect that dependent files have changed and will
rerun itself - but sometimes when you add or remove source files
you'll need to tell cmake to re-run explicitly.  To do this just rerun
cmake in the build directory and it will remember any options you
previously gave it:

    build$ cmake .

---
# Adding New Source Files Or Targets

See the individual CMakeLists.txt for examples on adding new files.

---
# Cleaning Up

Since we're using an out-of-source build directory cleanup is easy - just delete
the `build` directory:

    skip$ rm -rf build

---

# Compiler Development Workflow

Parts of the compiler are used during the build process for the compiler.
This adds extra steps to the development process of the compiler.
The source of truth for the compiler is in the `src/` directory.

The typical development process for the compiler is:
- edit compiler code
- run compiler tests with `ninja test_compiler`
- repeat until all compiler tests are passing
- once you are satisfied that the compiler is in a good state, update the
compiler used as part of the build process with `ninja update_lkg`. LKG stands
for Last Known Good. Only promote your code into the LKG once you are confident
that it works. If you do promote a buggy compiler into the lkg, use git to revert all
changes to the `lkg` directory.
- Before submitting a pull request, run `ninja test`. This includes all tests
in `ninja test_compiler` as well as checking that the lkg and runtimes are up to date.
- `ninja check_lkg` will tell you if your LKG is out of sync with your compiler.

# Making Language Breaking Changes

Making breaking changes to the language requires an extra step in the update_lkg process.
The update_lkg process has 2 parts, update_lkg_compiler and update_lkg_depends.
When making breaking changes to the language, these 2 steps must be done separately.

The process for making breaking language changes is:
- edit compiler code
- run compiler tests with `ninja test_compiler`
- repeat until all compiler tests are passing
- once you are satisfied that the compiler is in a good state, update the
compiler used as part of the build process with `ninja update_lkg_compiler`.
Do NOT do `ninja update_lkg` or `ninja update_lkg_depends` yet.
- now you have an LKG compiler which will require changes in src. Make those changes
now until you get `ninja skip_depends skip_native` building again.
- Now you have an LKG compiler which accepts the new language, and the compiler source
is written in that new language.
- Now do a `ninja update_lkg`. Alternatively you can get just `ninja skip_depends`
building, and then `ninja update_lkg_depends`.

Now you have your entire repo ported to the new version of the language. Proceed
with the normal developer work flow.

# Making Breaking Project System changes

Similar to breaking language changes, breaking changes to the format of `skip.project.json`
files also requires a 2 step LKG update process.

- edit the project system code at `src/project`
- Now you have src which accepts the new format, but lkg/skip_depends takes the old format.
- `ninja update_lkg_depends` to make lkg/skip_depends accept the new format.
- Update all the `skip.project.json` files in the repo to the new format.

Now all parts of the repo are using the new `skip.project.json` file format.

# Runtime Development Workflow

The source of truth for the runtimes is in the `tests/runtime/`
directory but the compiler uses a copy of the runtime to build itself
so editing code in the `tests/runtime/` directory requires an extra
step.

* The runtime in `lkg/runtime/` is used to compile the lkg (last known good) compiler.
* The runtime in `src/runtime/` is used to compile the compiler.
* The runtime in `tests/runtime/` is used to compile tests.


The development process of `tests/runtime/` code is:
- edit the runtime code
- run the tests with `ninja test_compiler`
- repeat until all compiler tests are passing
- once you are satisfied that the code in `tests/runtime/` is solid, promote it
to the compiler source. (See Syncing Runtimes below)
- Now go back to the standard Compiler Development Workflow above.
- `ninja check_runtimes` will tell you if your runtimes are out of sync.
- `ninja test` will fail if your runtimes are out of sync.

### Syncing Runtimes

If you've only modified `tests/runtime` you can use rsync to copy
(note that the '/' after the source filename is important):

    $ rsync -a --delete tests/runtime/ src/runtime

There's also a script in `tools/sync-runtime` but it's not very smart
 and will get either deleted or improved soon.

Otherwise you'll need to ensure they match up by hand.  A recursive
diff should show no difference between tests/runtime and src/runtime:

    $ diff --brief -r -X .gitignore tests/runtime src/runtime && echo match || echo broken
    match

### Multiple Runtimes Are Confusing

Since we have multiple copies of the runtime it can be confusing to
know which one you should be modifying.

**The general rule is that you should modify the runtime that lives with the code being modified.**

You'll never have to edit the lkg version of runtime directly unless
things have gone horribly wrong.

If you're making changes to tests and those changes require
corresponding changes to the prelude then the version of the prelude
you should be modifying is in the `tests/runtime` directory.

If you're making changes to the compiler and those changes require
corresponding changes to the prelude then the version of the prelude
you should be modifying is in the `src/runtime` directory.

**There are some weird cases.**

If you're making changes to the compiler which outputs code that
requires changes to the native runtime (such as adding a new parameter
to the SKIP_throwException call) then even though you're modifying
compiler files the runtime you should modify lives in the
`tests/runtime` directory.

If you're making changes to the prelude and you want to both write
tests for those changes and use them in the compiler then you need to
make parallel changes to both the `tests/runtime` AND `src/runtime`
directories - the compiler will use the version in `src/runtime` and the
tests will use the version in `tests/runtime`.

**It can get even weirder.**

It's possible to be in a situation where our directory structure
doesn't actually work.  For example, if you need to make changes to
the language and you want to modify the compiler to use those changes
and test that the new compiler actually passes tests you need a second
compiler directory.

For this case you need an "external LKG" (see
[External LKG](#external-lkg)).  In this model you
set up two repos - one where you build compiler 'A' (built with A's
LKG) and a second where you build compiler 'B' (built with compiler
'A') and tests (built with compiler 'B').  Then the runtime you modify
can be any of three different runtimes in two different repos,
depending on what you need to modify.

# Running Unittests

## Compiler

All the compiler tests run as part of 'ninja test'.  You can also run the tests
for a specific backend as with 'test_<backend>':

    build$ ninja test_js

Individual tests can be run by adding the test path using dotted notation.  For
example - to run the test tests/src/frontend/runtime/parser.sk using the JavaScript
backend do:

    build$ ninja test_js.tests.src.frontend.runtime.parser

Tests for each backend will only be rerun if they failed or source changes -
passing tests will only be run once.

To (re-)record all the .exp files, you can run

    build$ UPDATE_BASELINE=1 ninja test

Tests can also be filtered to only include a subset of tests.  To do this pass
-DFILTER=<regex> to cmake and then only tests passing that filter will be
included in 'make test':

    build$ cmake . -DFILTER=top

To disable the filter and re-enable all tests re-run cmake with a blank filter:

    build$ cmake . -DFILTER=


## Runtime

Unittests are defined such that the target name builds and runs the test.  To
just build the test precede the target name with '_build_'.

So to run the unittest 'test_intern':

    build$ ninja test_intern

To only build 'test_intern':

    build$ ninja _build_test_intern

---
# Installing

When doing development you'll be running directly out of the build directory off
the latest sources but sometimes you need an install directory.  The install
prefix can be specified by setting CMAKE_INSTALL_PREFIX at cmake time.  You'll
probably also want to make a release build as well:

    build$ cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/install
    build$ ninja install

This will leave an install/ directory in the build tree which will have the
usual bin/, lib/ and include/ directories under it containing Skip files.

---
# External LKG

Normally you'll want to use the lkg that was included in the repo.  But
sometimes it's useful to use a separate repo as your lkg.  An example of this is
if you want to debug the process of compiling the compiler itself.

To do this modify the EXTERN_LKG option to cmake to point to your alternate lkg:

    build$ cmake .. -DEXTERN_LKG=/home/me/other_repo/build

Now when you build, the src compiler from /home/me/other_repo/build will be
used to build the src compiler in your repo, instead of the lkg compiler.

Note that no dependencies are set up between the two repos - if you modify code
in other_repo you must explicitly build 'skip_to_llvm' in other_repo before you
build your main repo.

# Profiling Generated Binaries

Use the 'perf' tool on linux to profile skip native binaries:

    perf record --call-graph dwarf -F 25000 --event cycles <command-line>
    perf report --children

# Debugging generated binaries with gdb on OSX

Note that lldb is the preferred debugger on OSX. If you are a gdb relic like me
here's some tips to get it working:

One time setup:

    echo set startup-with-shell off >> ~/.gdbinit

Each time you run gdb, you must start with 'sudo' otherwise you will see
errors like:

    Unable to find Mach task port for process-id 95691: (os/kern) failure (0x5).
      (please check gdb is codesigned - see taskgated(8))

# Nuclide

To setup Skip support in Nuclide:
- Open 'Nuclide Settings' with Cmd-Opt-,
- Set 'Directory of skip binaries' to something like: `/Users/aorenste/github/skip/nuclide`

Build and install the Nuclide binaries with: `tools/update_nuclide`.

This will build the nuclide binaries from your src/ directory and copy them to
the nuclide/ directory. Typically you will `tools/update_nuclide`
after each pull from master.

Note that you must manually `tools/update_nuclide`. `ninja all` will not update
your nuclide install. This prevents your nuclide support from breaking if you
introduce an error into your src/ dir.
