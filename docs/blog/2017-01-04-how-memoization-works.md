---
title: Reactivity Overview
author: Mat Hostetter
---

TL;DR: The runtime for the Skip programming language memoizes (caches) function return values. In addition to traditional memoization, it tracks dependencies on mutable state, such as database values, and automatically updates the memoization cache when values that affected a function's return value change. Multiversion concurrency control (MVCC) allows thousands of parallel tasks to use the cache without any global locks.

## Overview

As an optimization, Skip caches certain function call results, a process called "memoization". Memoization sometimes allows function calls to be bypassed by looking up the arguments in a table and substituting the value found there rather than running the code.

Memoizing a function call's return value based on its arguments is straightforward, and many programming languages (such as PHP) provide some way to do this. Writing a memoization decorator is even a common Python interview question. Skip's memoizer goes much farther and not only memoizes a function's return value, but also makes the cached value "depend on" all mutable state that function examined when it ran. The literature variously calls this adaptive computation or incremental computation.

In addition to keeping its memoization cache up to date, Skip allows arbitrary callbacks to run whenever those cached values change. This would facilitate, for example, data to be "pushed" to a mobile device only when necessary.

## Dependency Graphs

It's important to understand that the Skip runtime is much more than a simple database cache. For example, if a Skip program reads data from a reactive data source and analyzes it to compute some information (e.g. can the user see some content), Skip will memoize those computed results, not just the raw database result. So if the same information is computed again, it will be instantly satisfied from the memoization cache without re-running any code. The key to making this work correctly is automatically updating the cache when data changes.

Skip uses a dependency graph to identify when memoized values become invalid and to recompute them efficiently when they do. This graph is created by observing and recording at runtime which memoized functions are called by other memoized functions (and with what parameters, and in what order), as well as what pieces of mutable state those calls actually examined. The graph is related to [ADAPTON's Demanded Computation Graph](https://www.google.com/search?q=adapton+demanded+computation+graph) (DCG), but extended with multiversion concurrency control (MVCC) and incremental coarsening of dependency information to reclaim memory.

Memoization, dependency tracking, invalidation and recomputation are handled automatically by the Skip runtime, but they do constrain the user's programming model. In order for memoization to work, memoized functions need to be transitively side-effect-free, which the Skip compiler enforces statically. Skip is not a functional programming language, which would have been one way to achieve these guarantees; instead, objects marked `mutable` can be modified as in other languages, but the compiler does not allow such side effects to cross memoization points.

## Side Effect Management

A Skip program is logically partitioned into two layers: a memoizing inner "Core", where "reader tasks" compute values in parallel, and an external, non-memoizing "Mutator" that is allowed to change specially designated mutable objects called Cells. A Cell might be a simple wrapper around a number or string, but could also be a more complex data structure like an array or hash table.

When a Core function reads a Cell in the course of computing a result, the runtime automatically creates an edge in the dependency graph between that Cell and the function's memoized result value. Similarly, when one memoized function calls another (directly or even indirectly through a chain of non-memoized functions), the runtime connects them in the dependency graph. This way, when the Mutator assigns a new value to a Cell, the runtime can find all of the memoized values that transitively depend on it.

The Mutator can be written in Skip, but can also be written in another language, like PHP, Javascript or C++. It might use the Core Skip program as a reactive caching library, or might itself be a library used by the Core, such as a reactive database access layer.

## Scalability, Atomicity and MVCC

The memoization cache may have millions or billions of cached values in it, and many thousands of parallel readers both reading the cache and adding new memoized entries to it. And while readers are running, the Mutator may be making millions of Cell changes whose effects ripple through the dependency graph.

This high level of concurrency raises not only scalability concerns (we can't afford to "lock the world" to change a Cell), but semantic questions as well. What does it mean to memoize computed values when the Cells they are based on are constantly changing? Can the Mutator change multiple Cells atomically? Can multiple reader tasks be guaranteed to see exactly the same Cell values as each other (e.g. for computing different parts of the same UI in parallel)?

Skip uses [multiversion concurrency control](https://en.wikipedia.org/wiki/Multiversion_concurrency_control) (MVCC), a popular database implementation technique, to answer all these questions.

Each Mutator change is performed as a transaction commit, an atomic change to any number of Cells. Each commit makes its changes, propagates invalidations through the dependency graph, then increments a global integer transaction ID to indicate that a new "version of the world" is available to reader tasks.

Each reader task (typically just making one function call) latches the global transaction ID when it starts and uses that throughout its entire run. It only "sees" Cell and memoized values "as of" that transaction ID, ignoring any subsequent changes. To make this work, each Cell and each memoization cache entry records not just a single value, but a linked list of "revisions", each consisting of a value annotated with a non-overlapping [begin, end) transaction ID "lifespan". Looking up a memoized value for a specific transaction ID requires scanning the revision list, but the most recent revision is at the head, so the search usually terminates immediately. As old tasks exit, revisions corresponding to transaction IDs that no tasks care about any more get garbage collected, keeping the lists very short in practice.

It's clear how MVCC provides atomicity -- latching a transaction ID "freezes" the version of the world that a reader task sees -- but it also provides scalability. The challenge when committing an atomic change that affects thousands of dependency graph nodes is making sure that readers always see a coherent snapshot of the cache. MVCC solves this in a scalable way without any global locks. The key insight is that, if the current global transaction ID is T, then no reader cares about the state of the cache at T+1, because when they scan revision lists they will only consider entries that match the transaction ID they latched, which must be less than T+1. So the Mutator can update thousands of revision lists to indicate information about T+1, but locking only one object at a time. This leaves the "snapshot" for T+1 temporarily but harmlessly incomplete while it is making all the updates. Only when the commit is fully finished does the Mutator assign T+1 to the global transaction ID, suddenly making that version visible to newly-spawned readers. Existing readers still running at T (or T-1, or T-100...) continue running unaffected, since those values continue to exist in the revision lists until the garbage collector is able to remove them.

## Current Status

Version 1 of the Skip runtime is ready to be hooked up to a new, LLVM-based compiler back end currently being implemented. (Note that Skip's existing PHP back end supports the full Skip language but does not have memoization support).

## Future Topics

There are quite a few Skip runtime topics that could be discussed, but this post is already long enough! Some ideas for future posts:

- How revision lists get updated during a commit.
- How Skip uses "traces" to avoid unnecessary recomputation caused by "false positives", where a function's return value does not change even though some dependency changed (in what turned out to be an irrelevant way).
- How the dependency graph gets "incrementally coarsened" to recover memory, trading speed for space based on LRU heuristics.
