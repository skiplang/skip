---
title: Parallelism Support
author: Mat Hostetter, Aaron Orenstein
---

TL;DR: Skip (a new reactive programming language formerly called "Reflex") now supports parallel processing!

## What's new?

Over Thanksgiving weekend Aaron Orenstein and I engaged in an impromptu bicoastal hackathon to add parallelism to Skip.

The first question we had to answer was what primitive parallel operation to provide. We settled on tabulate:

```js
fun tabulate<T>(count: Int, f: Int ~> T): mutable Vector<T> {
```

This generic function takes a count and a function f and produces the array:

```js
[ f(0), f(1), f(2), ..., f(count - 1) ]
```

by invoking f in parallel.

If this API seems a bit odd, just consider it a building block to implement other parallel algorithms. For example, the Skip standard library now provides a traditional parallelMap method that applies a function g to an array, returning:
```js
[ g(array[0]), g(array[1]), ..., g(array[array.size() - 1]) ]
```

The implementation simply calls tabulate with a wrapper lambda that captures g and array.

Where are the mutexes and atomics?

## You don't need them!

To facilitate reactive memoization, Skip's type system tracks when an object must be transitively immutable (which we call "frozen"). By using the `~>` (“tilde”) rather than `->` (“dash”) lambda syntax in its type declaration, tabulate constrains f to be a frozen lambda, which means it can only capture frozen objects.

Because Skip programs have no mutable global state, and f is frozen, it's impossible for any two invocations of f to modify the same data. This not only makes race conditions impossible, it also makes tabulate's return value deterministic. The strong guarantees provided by the type system are what made it possible to implement a foolproof parallelism API in a weekend.

Of course, parallel algorithms that update shared mutable state are undeniably useful, but they open a giant can (a barrel?) of worms. Skip doesn't support them, but there's always next Thanksgiving!

## How is parallelism implemented?

The Skip compiler uses LLVM to produce optimized machine code. (It also has Javascript and Hack back ends, but they don't support parallelism). The compiled Skip program is linked against a C++ runtime library that provides traditional features like garbage collection and Unicode support as well as reactive memoization. We extended the runtime library to add a new parallelTabulate entry point.

When a thread (which we'll call the "launching thread") invokes parallelTabulate, it posts tasks to a folly `CPUThreadPoolExecutor` to enlist worker threads to help out. Rather than posting one task per array entry, it posts `min(array_size, num_threads) - 1` tasks that each may compute many array entries. Each task runs a loop that grabs the next unprocessed array index using an atomic increment, computes and records the value for that index, and repeats until the index hits the end.

After posting tasks, the launching thread also runs the task loop. This way, even if no worker threads are available (perhaps due to a nested tabulate) it will still finish the job itself, and in any case it might as well do something useful. Once its loop finishes, the launching thread synchronizes by waiting only for threads that started computing something, rather than waiting for all posted tasks to complete. If a worker thread later picks up a task for a completed parallelTabulate call, it discards it.

## Does it work?

Yes! To test threading we parallelized one important loop in Skip's LLVM back end which applies various optimization passes to each function being compiled. This code can now optionally use parallelMap instead of map:

```js
newFuns = if (parallel) {
  funs.parallelMap(optimizeFun)
} else {
  funs.map(optimizeFun)
};
```

That was easy. We benchmarked this change on my 24-core dev VM by compiling the compiler with itself using varying numbers of threads:

<center><img src="/blog/assets/parallelism-speedup.jpg" width="500" height="470" /></center>

As you can see, the speedup starts out nearly linear but starts to tail off around 5 cores or so, and by the time it hits 24 cores we're at a 12x speedup. That's a pretty typical-looking scaling curve, especially for a memory-intensive program like a compiler. We don't know what the bottlenecks are — we haven't tuned this at all, this is just what happened when we tried parallelMap.

## Conclusion

The strong immutability guarantees of Skip's type system made it surprisingly easy to add safe, easy-to-use parallelism. And unlike a functional language, you can still use mutable data structures when you need to.

Although this was a fun hack, the next big exciting change won't involve parallelism, and will take a lot longer than a weekend — it's making the compiler incremental, using Skip's own reactive memoizer to only recompile code affected by modified files. Stay tuned!
