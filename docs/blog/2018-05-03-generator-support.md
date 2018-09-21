---
title: Generator Support
author: Mat Hostetter
---

TL;DR: Skip now has generators!

If you've ever used the `yield` keyword in Python, Hack, Javascript, C#, etc., you know how handy generators can be. And now Skip has them!

While implementing `async`/`await`, I noticed that the coroutine support I was adding to the compiler would make generators easy to implement. So I took a brief detour and added them to the language too.

## A few examples

Suppose you want to write a function that produces a stream of square numbers, but without actually allocating a container to hold them all. You can now write:

```
fun squares(count: Int): mutable Iterator<Int> {
  foreach (i in range(0, count)) {
    yield i * i
  }
}

// Enabling users to write something like this:
foreach (n in squares(100)) { ... }
```

That’s a bit contrived, so let's look at a real example of how generators can improve Skip's standard library.
Skip Iterators support chaining into pipelines, such as:

```
    x.values()
      .filter(a -> a.isValid())
      .map(b -> b.name)
```

Previously, `Iterator`'s `filter()` method created an instance of a custom `FilterIterator` class that wraps its own value stream:

```
  overridable mutable fun filter(p: T -> Bool): mutable Iterator<T> {
    mutable FilterIterator(this, p)
  }
```

By itself that looks fine, but of course it requires writing a separate `FilterIterator` class, and that class does a bunch of messy stuff (don’t worry about the details):

```
  mutable fun next(): Option<T> {
    found: Option<T> = None();
    while_loop(() ->
      this.base.next() match {
      | Some(x) ->
        !this.p(x) ||
          {
            !found = Some(x);
            false
          }
      | None() -> false
      }
    );
    found
  }
```

That code was written before Skip supported early `return`, so it could be made more concise. But with generators, we can go much farther than incremental tweaking; in fact, the `FilterIterator` class is now completely gone, leaving us with only this method on Iterator:

```
  overridable mutable fun filter(p: T -> Bool): mutable Iterator<T> {
    foreach (v in this) {
      if (p(v)) yield v
    }
  }
```

I think you'll agree that's much easier to read!

## Implementation details

Skip's generators are implemented in basically the same way that other languages do.

For each function containing a `yield`, the compiler creates a custom `Iterator` subclass whose `next()` method holds a transformed version of the the user’s function. The original function is replaced with a stub that simply returns a new instance of that subclass.

The transformed `next()` method weaves a simple state machine into the user's code, with states corresponding to "initial call", "done", or which specific `yield` was executed most recently. On entry, `next()` does a `switch` on the state number field, resuming execution where it last left off.

Each `yield x` saves that `yield`'s state number and any live local variables into fields, so they can be restored on the next call to `next()`, then returns `Some(x)`. When `next()` is completely finished, it returns `None()`.

The Skip compiler primarily targets LLVM, although there is also Peter Hallam’s Javascript back end, which transpiles to native Javascript generators. I looked into leveraging LLVM's coroutine support, but I couldn't use it because I need to control the coroutine suspend state in order to describe it to Skip's garbage collector. But even if I could solve that, the strategy of desugaring generators into normal Skip objects allows us to apply our  standard mid-level optimizations to that code before handing it to LLVM, including potentially inlining all of the generator code and removing the heap allocation.

Next up, async/await!
