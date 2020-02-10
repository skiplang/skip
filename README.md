[![CircleCI](https://circleci.com/gh/skiplang/skip.svg?style=svg)](https://circleci.com/gh/skiplang/skip)

## Getting started

### Ubuntu/Debian

```
sudo apt-get install git clang make python3 autoconf libtool gcc g++ bzip2
git clone http://github.com/skiplang/skip.git
cd skip/
git submodule update --init --recursive
./build_submodules.sh
./configure
make -j 16
sudo make install
```

### Centos

```
yum install -y git which clang make autoconf automake libtool python3 bzip2
git clone http://github.com/skiplang/skip.git
cd skip/
git submodule update --init --recursive
./build_submodules.sh
./configure
make -j 16
sudo make install
```

### Mac

```
git clone http://github.com/skiplang/skip.git
cd skip/
git submodule update --init --recursive
./build_submodules.sh
./configure
make -j 16
sudo make install
```

If you don't have clang installed (unlikely):
```
xcode-select --install
```

## Skip Overview

Skip is a general-purpose programming language that tracks side effects to provide caching with reactive invalidation, ergonomic and safe parallelism, and efficient garbage collection. Skip is statically typed and ahead-of-time compiled using LLVM to produce highly optimized executables.

### Caching with Reactive Invalidation

Skip's main new language feature is its precise tracking of side effects, including both mutability of values as well as distinguishing between non-deterministic data sources and those that can provide reactive invalidations (telling Skip when data has changed). When Skip's type system can prove the absence of side effects at a given function boundary developers can opt-in to safely memoizing that computation, with the runtime ensuring that previously cached values are invalidated when underlying data changes.

### Safe Parallelism

Skip supports two complementary forms of concurrent programming, both of which avoid the usual thread safety issues thanks to Skip's tracking of side effects. First, Skip supports ergonomic asynchronous computation with async/await syntax. Thanks to Skip's tracking of side effects, asynchronous computations cannot refer to mutable state and are therefore safe to execute in parallel (this means that independent async continuations can continue in parallel). Second, Skip has APIs for direct parallel computation, again using its tracking of side effects to prevent thread safety issues such as shared access to mutable state.

### Efficient and Predictable GC

Skip uses a novel approach to memory management that combines aspects of typical garbage collectors with more straightforward linear (bump) allocation schemes. Thanks to Skip's tracking of side effects the garbage collector only has to scan memory reachable from the root of a computation. In practical terms, this means that developers can write code with predictable GC overhead.

### Hybrid Functional/Object-Oriented Language

Skip features an opinionated mix of ideas from functional and object-oriented styles, all carefully integrated to form a cohesive language. Like functional languages, Skip is expression-oriented and supports abstract data types, pattern matching, easy lambdas, higher-order functions, and (optionally) enforcing pure/referentially-transparent API boundaries. Like imperative/OO languages, Skip supports classes with inheritance, mutable objects, loops, and early returns. Skip also incorporates ideas from “systems” languages to support low-overhead abstractions, compact memory layout of objects via value classes, and patterns that ensure code specialization with static method dispatch.

### Great Developer Experience

Skip was designed from the start to support a great developer experience, with a rapid iteration speed more commonly associated with dynamic languages. The compiler supports incremental type-checking (with alpha versions of IDE plugins providing near-instantaneous errors as you type), provides hints for common syntax mistakes and to help newcomers learn the language, recognizes small typos of method/class names, and even recognizes common alternatives to Skip's standard library method names and suggests the correct name in Skip. Skip also features a code-formatter to ensure consistent code style and a tool for running codemods.

### Built by a Team of Veterans

Skip was designed by an experienced team including senior contributors to ActionScript, C#, Flow, Hack, HHVM, Prettier, React Native, and Relay.

### Documentation is in the `docs/` directory.

[Instructions for building with `cmake`](docs/developer/README-cmake.md)

## Website

http://www.skiplang.com

### Playground

http://www.skiplang.com/playground

## License

Skip is [MIT licensed](./LICENSE).
