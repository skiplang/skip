---
title: Async/Await
author: Mat Hostetter
---

Skip now has async/await, which should look familiar to Hack and Javascript programmers:

```
async fun myFun(name1: String, name2: String): ^Int {
  value1 = await lookup(name1);
  value2 = await lookup(name2);
  value1 + value2
}
```
## Implementation details

Skip implements await in much the same way as generators, using heap-allocated coroutines. If you’re not familiar with coroutines, it’s an interesting exercise to work out how the compiler might transform your function containing await or yield into one that can return early (like when an await isn’t ready) and later resume where it left off.

Coroutines were challenging to add to the compiler, but that was only one piece of the puzzle. In addition to async/await, Skip also supports parallel execution, memoization and reactivity. Supporting two of those features at the same time isn't so bad, but when you mix all four together the implementation starts to get tricky.

## Processes
In order to make Skip's various forms of parallelism and concurrency mesh together, I extended Skip's C++ runtime with a concept that I grudgingly called Process. A Process consists of a private GC heap, an event queue and some reactive graph construction state.

At any given time a Process may or may not be “animated” by a thread; in particular, a Process which is stalled waiting for I/O will not consume a thread until it's ready to run again. This is analogous to OS processes that may or may not be running on a CPU core at any given moment.

A Skip program consists of multiple communicating Processes, but they are hidden from users. That said, you could imagine tasteful ways to expose them in the future or even distribute them across machines (Erlang, anyone?)

## Async + memoized, together at last

Before Skip had async/await, if two Processes tried to compute the same memoized value at the same time, the second Process would block and wait for the first to finish.  Avoiding redundant computation is good, but we never want threads to block, and we’ve seen this be a problem in practice. Observe the parallelism dips in Skip's type checker (written in Skip) where many parallelMap Processes all try to compute the same commonly-used memoized values, such as the Int type:

## Skip compiler parallelism over time
Finally we can fix this! A function can now be both async and memoized:
```
async memoized fun getSomething(name: String): ^Int
```
In the "contention" case described above, the second Process will immediately get back an Awaitable that will be ready when the first Process finishes. Because await is inherently non-blocking, this lets it do useful parallel work rather than getting stuck.

A memoized async function may of course do an await that needs to suspend, perhaps waiting for a network result. To implement this, I gave each memoized function invocation its own Process, which can be suspended during await and put on a shelf until it's ready to run again. Creating a Process is cheap, but not free, and it's possible we'll need to optimize this later.

When a suspended Process is ready to run again, it can resume running in any background thread. This means async computation can automatically proceed in parallel with other work being done in the main thread.

## Optimizations

HHVM's experience has been that the size of Awaitables is important, so I implemented a few compiler optimizations:

When an await suspends, we only save exactly those local variables ("SSA nodes" to you compiler hackers) which are "live" at that point in the function. Variables which are never live across an await do not consume any space in the Awaitable.

The compiler uses graph coloring to minimize how much storage it needs for suspended variables. If two local variables never need to be "suspended" by the same "await", they can share the same storage in the Awaitable's suspend state. This is not dissimilar to register allocation, a classic use of graph coloring. The end result is that the size of each Awaitable is determined by the maximum storage needed by any single "await".

## Future Work

## Language changes
To preserve determinism, an async function can only take frozen (deeply immutable) parameters or Awaitables (which, although their physical representation changes over time, are “logically frozen”). This rule is necessary because if two async functions were able to take the same mutable object as an argument, the program could perceive the order in which those async functions finished, which could vary randomly from run to run. Additionally, we wouldn’t be able to run async computation in background threads.

Determinism has many advantages, such as easier debugging and making the reactive cache’s "false positive" detection more efficient. But unfortunately the async parameter rule turns out to be too restrictive, preventing us from writing useful features like async iterators. Julien Verlaguet, Basil Hosmer and Todd Nowacki are looking into adding a dash of Rust-style unique references to the language to solve this problem.

## HHVM integration
A Skip program can already be statically compiled and loaded into HHVM as a shared library, where Skip code can interoperate with Hack code. That said, the interop is incomplete. We have not yet tied together Skip and HHVM’s async/await implementations, so a Skip program cannot await an HHVM object or vice versa. To fix this, we need to integrate Skip Awaitables with HHVM’s event loop by implementing one of their interfaces. Fortunately Jan Oravec has given us some handy tips on how to proceed.
