# Overview

This design doc describes how async/await will be implemented in Skip.

This work, and dependent tasks, can be found via master task [T24366119](https://our.intern.facebook.com/intern/tasks/?t=24366119).


# Awaitables

`Awaitable<T>`, which can be abbreviated as `^T` in Skip, is a future indicating a value being computed asynchronously. An Awaitable does not in any way "know" how to compute the value. It is simply a container into which the value will eventually be stored.

At the surface, Skip code can query this value (potentially blocking until it's ready) using the await operator. Underneath, each await operation on an Awaitable results in a continuation lambda: the Awaitable object hosts these lambdas, and executes them its value is ready.

## Language Rules

Awaitables challenge Skip's normal mutability rules. Before getting into why, it's helpful to think of two different levels of frozenness:

  - **Logically frozen**: from the user's perspective, the value never changes.
  - **Physically frozen**: the underlying physical representation never changes.

All physically frozen objects are logically frozen, but not vice versa. Today Skip's `frozen` keyword means "physically frozen", and there is no concept of "logically frozen", either in the language or the compiler's internal type system.

To guarantee deterministic results, Skip can only allow async or parallel tasks to capture logically frozen values, because otherwise the code could perceive order of operations.

In addition, various compiler optimizations such as garbage collection and escape analysis rely on the stronger guarantee of objects being physically frozen. The interner (which drives the reactive memoizer) also relies on objects being physically frozen, both because it "hash-conses" the raw bytes stored in an object, and because interned objects exist in a separate refcounted heap that's not allowed to reference the normal mutable Obstack heaps used by user code.

An Awaitable object is, fundamentally, logically frozen -- user code can never perceive its value changing, because the only thing it can do is wait for the value to appear then read the value. But in this design, Awaitables are not physically frozen, because their underlying state eventually changes to reflect the value appearing, and also because the Awaitable tracks await-resuming bookkeeping described later.

(Aside: other designs are possible which offload this state to runtime subsystems, so that the Awaitable value itself is pure. However colocating identity and state has practical advantages which have led us to its choice here.)

Awaitables have a few language-level requirements that challenge normal "frozen means physically frozen" rule:

  - Awaitable must be covariant (e.g. `^Cow` is a subtype of `^Animal`),
    Mutable objects do not allow covariance because it is unsound.
  - You must be able to pass Awaitables, and containers containing Awaitables,
    to async functions.
  - The physical identity of an Awaitable is important, because some external
    entity (e.g. a C++ packet handler) maintains a pointer to it so it can
    eventually update its state. Doing an `await` on a copy of an Awaitable
    could hang forever. But normal frozen objects have no identity and can
    freely get copied (e.g. interning copies to the intern heap).
  - A 'memoized' function should be able to return an Awaitable, despite
    the usual rule that only physically frozen interned objects can be
    returned by the memoizer.

It's tempting to try to extend the type system with a general concept of "logically frozen" that enables Awaitable's special niche in the ecosystem. But after much discussion we concluded that we should just treat Awaitable as special language magic with its own custom rules. This isn't particularly troubling, because it already has its own special syntax (`async`, `await`, the `^` operator). Its rules look like this:

  - The `^T` syntax means `mutable Awaitable<T>`. Except where blessed with
    special abilities, it acts like any other mutable type.
  - Covariance is allowed on Awaitable's type parameter, regardless of the
    Awaitable's mutability.
  - Attempts to manipulate non-`mutable` Awaitable, or freeze mutable
    Awaitables, are rejected in the same way they are for `->` lambdas.
    Most importantly, the `await` and `awaitSynchronously` operations are
    only available on mutable Awaitables (as if they were mutable methods
    on Awaitable). Because all "copying" operations, such as `freeze`, yield
    frozen objects, this prevents eternally awaiting a dead copy.
  - async functions only accept logically frozen ("LF") parameter types,
    with the following definition:
    1. Any `frozen` (i.e. physically frozen) type is LF (e.g. `Int`).
    1. Any Awaitable is LF if its type parameter is LF (e.g. `^Int`).
    1. Any unannotated generic type where all type parameters are LF is itself
       LF (e.g. `Map<Int, ^Int>` or `Vector<Map<Int, ^Int>>`)

    We can think of this as a generalization of the upcoming "chilled" type
    rule, where an unannotated generic is physically frozen if all type
    parameters are physically frozen, else it is LF if all type parameters
    are LF, else it is readonly. Whether we literally introduce this concept
    into the compiler's type system or do an ad hoc hack for async functions
    is an open question.
  - Memoized functions are allowed to return `^T`, if T is frozen.
    `T` may not have further mutable Awaitables embedded in it, only
    the top-level return value can be a mutable Awaitable.
  - The interner could encounter a frozen Awaitable field. Its physical
    state could be in flux at the moment the interner visits it. Rather than
    trying to intern the Awaitable based on its data at that moment, the
    interner simply discards it and substitutes a NULL pointer. This works
    because frozen Awaitables support no operations, so they are all
    indistinguishable [NOTE: We could do the same thing with `->` lambdas].
  - Because it has special abilities, The `Awaitable` type cannot be subclassed
    or extended with extension classes.
  - As with other mutable objects, a frozen lambda **cannot** capture a
    mutable `Awaitable`. There's nothing logically wrong with this, but the
    memory management challenges are too difficult for now -- a frozen
    lambda could get interned, then another thread could call it and
    `await` the captured `Awaitable` which exists on a different Obstack
    (indeed, on an Obstack that might have been freed already).

## Awaitable Implementation

An Awaitable object is manipulated by one *writer* and possibly multiple *readers*. Readers attempt to read the value, and if it's not ready, they write a callback called a `Continuation` that describes what the writer should do, then suspend. The writer writes the computed value and reads the list of `Continuation`s posted by suspended readers. Mediating races between readers and writers requires the Awaitable to be treated as a simple state machine.

An Awaitable can be in one of four states:

- `PENDING_NO_WAITERS`

   This is the initial state of an Awaitable. It holds no value, and
   no `await` has been done on it. Any `await` done on an Awaitable in this
   state will store a `Continuation` in the Awaitable, suspend the reader,
   and transition to the `PENDING_WAITERS` state.

- `PENDING_WAITERS`

   If an `await` happens before reaching one of the `READY` states, the
   Awaitable holds a linked list of `Continuation` objects indicating what extra steps to take once `READY`.

- `READY_VALUE`

  Computation of the Awaitable has succeeded and it holds a value of type `T`.
  Any `await` on this object will immediately yield its stored value.
  No further state transitions are possible.

- `READY_EXCEPTION`

  Computation of the Awaitable has finished via an exception, and it holds
  the `Exception` object. Any `await` on this object will immediately rethrow
  that exception. No further state transitions are possible.

An Awaitable's state is not user-visible; it if were, users could
perceive the order in which asynchronous operations complete and use
those to yield nondeterministic results, which is something we try to
avoid. "Awaiting" the value to be ready does not reveal when it became ready.

An Awaitable is not literally a C++ object, but if it were, its layout would look roughly like this:

```cpp
   template<typename T>
   struct Awaitable<T> final {
     // Holds both the state enum and the PENDING_WAITERS state (if any).
     std::atomic<TOrFakePtr<Continuation>> continuation;

     // READY_EXCEPTION value
     Exception* exception = nullptr;

     // READY_VALUE value (initially GC-safe garbage)
     T value;
   };
```

The `continuation` field is atomic to mediate a race between state transitions, where a reader would like to transition to `PENDING_WAITERS`, and the writer would like to transition to one of the `READY` states. It can hold one of four values, one for each state:

- `NULL`. This indicates the `PENDING_NO_WAITERS` state.
- `(Continuation*)-1`: `READY_VALUE`
- `(Continuation*)-3`: `READY_EXCEPTION`
- Other pointer: `PENDING_WAITERS`. This field points an intrusive linked list of `Continuation`s to run upon transitioning to `READY`.

The non-positive pointer values in the first three states are considered "fake" by the GC and ignored, but the `Continuation` in the `PENDING_WAITERS` state looks like a normal pointer and will be traced.

When a reader executes an `await` operation, it performs the following steps:

  1. Load the continuation field, using C++ `std::memory_order_acquire`.
  2. If it is negative, the Awaitable is in a `READY` state:
     -  If (-1), it is `READY_VALUE`, so load the value and proceed.
     -  Else (-3), it is `READY_EXCEPTION`, so load the exception and rethrow it.
  3. Else (the Awaitable is not ready):
     - Create and initialize a `Continuation` object, described later.
       This object is pinned and starts out with refcount 1, to prevent
       a race in writer step (2), where the `Continuation` could get GCd
       if a GC of the Awaitable happened the moment after the writer's
       atomic exchange completed.
     - Compare-and-exchange the `Continuation` into the `continuation` field,
       using `std::memory_order_acq_rel`, to atomically assert that the
       continuation has not entered a `READY` state. To support multiple
       waiters this chains the new `Continuation` to the old list head,
       thereby doing an atomic stack push.

       If the compare-and-exchange fails due to the Awaitable becoming
       `READY` (i.e. the continuation field becomes negative), we could
       legally go to step (1), but for simplicity we simply "resume"
       the `Continuation` ("resuming" is described later). Resuming is easy
       to implement in an await helper subroutine and avoids cluttering the
       caller with useless loopy control flow.
     - The reader jumps to the end of its async block, leaving the Awaitable
       it was trying to fulfill (which is different than the one it's waiting
       on) in one of the `PENDING` states. The `Continuation` it stored will
       know how to resume execution just after the await that caused the
       suspend.

When a writer provides a value for an Awaitable, it performs the following
steps:

  1. Assign to either the `value` or `exception` field as appropriate.
  2. Unconditionally atomically exchange into the continuation field
     either (-1) for `READY_VALUE` or (-3) for `READY_EXCEPTION`, as
     appropriate, using std::memory_order_acq_rel.
  3. The exchanged value (i.e. the old value of the `continuation` field)
     indicates what needs to be done next.
     - If `NULL`, the Awaitable was in `PENDING_NO_WAITERS`, and nothing
       further needs to be done.
     - If negative, that's a serious internal error as the Awaitable was
       already in a `READY` state.
     - Else, the Awaitable was in `PENDING_WAITERS` state, and the exchanged
       pointer contains a linked list of `Continuation` objects that need to
       be resumed. Walk through the list and resume each one (described below).

NOTE: There are some code sequences that could be made a bit tighter
by tweaking this design, at least from the reader's perspective. If `T`
is a non-null reference type, and we expect the common case is that
await immediately succeeds without an exception, a reader
sequence that loaded the value field (with `std::memory_order_acq`) and
compared it against `NULL` would execute fewer instructions in the fast path.

NOTE: We could use a GC-safe union to overlap the `value` and `exception`
fields, since they are mutually exclusive.


# Continuations

A `Continuation` tells an Awaitable writer what to do after storing the value.

As an optimization, there are two kinds of `Continuation`: `Thunk` and `Counter`.

## Thunks

The most general form of `Continuation` is a `Thunk`, which is an
arbitrary `() -> void` lambda to execute once the value is
ready. Typically the `Thunk` will just resume execution in a suspended
coroutine (described later).

When exactly does the `Thunk` run? We don't want the writer to execute the `Thunk` itself for several reasons, e.g.:

-  We want the writer to do a bounded amount of work when storing a value in an Awaitable, because it might have been in the middle of doing something important.
- The writer might hold some mutex that could deadlock if it wandered off and ran arbitrary code.
- Perhaps the `Continuation` needs to run on a specific thread other than the writer's.

As a result, writers simply hand Thunks off to the **Scheduler** (described later) and resume what they were doing.

NOTE: In the future we could add special trivial `Thunk`s we promise are safe for the writer to execute.

## Counters

Scheduling a `Thunk` has a high fixed cost because of the thread handoff and task queue manipulation, and there are some common cases where we don't need its full generality. If some reader is suspended awaiting an array of a hundred Awaitables, we don't want to wake up its thread each time one Awaitable becomes ready. A Counter solves this problem by maintaining a pair of an atomic counter that the writer decrements each time it "resumes" and another Continuation to resume when that counter hits zero.


# Event Loop

Programs need a way to synchronously block and wait for an Awaitable's
value. Otherwise, a chain of asynchronous computation may ultimately
just return from `main()` without actually doing anything. Of course,
this kind of blocking stalls a physical thread and should not be used
lightly.

Skip provides a special `awaitSynchronously` method on Awaitable that
stalls the calling thread until the Awaitable enters a `READY` state,
upon which the method either returns the Awaitable's value or throws its
exception as appropriate. Unlike `await`, `awaitSynchronously` does not
need to be called inside an async block.

Obviously if `awaitSynchronously` is invoked on an Awaitable already in
a `READY` state, it returns immediately. Otherwise, the thread adds a
`Continuation` to the Awaitable that will post an event to that thread's
event loop, then drops into the event loop to process events. Each time it
processes one event it checks to see if the specific Awaitable it was
awaiting is ready. If so, it exits the event loop and returns the
Awaitable's value. Otherwise, it stays in the loop waiting for another
event.

Importantly, the event loop doesn't just handle `awaitSynchronously`
events. The Scheduler will post arbitrary `Thunk`s to the event
loop as Awaitables become `READY`. Indeed, it is only when the thread is blocked in the event loop that these Awaitables get resumed.


# Scheduler

The Scheduler's job is to take a `Thunk` and ensure that some eligible thread will eventually run it. Some Thunks may be constrained to run on a particular thread, while others might run anywhere.

To run a Thunk in a particular thread, the Scheduler simply posts it to that thread's event queue. As described earlier, entries in that queue will get processed whenever that thread does an `awaitSynchronously`, and not before then.

Parallel threads running against the same Obstack data creates challenges:

- One thread might GC and relocate pointers while another thread is looking at them.
- HHVM can only be reentered from the request thread.

There are solutions to these problems (e.g. prevent GC in the region of the Obstack "above" the point visible to both threads, or use the interner to copy `Continuation` state off the Obstack), but for version 1.0 the Scheduler will only support running `Thunk`s in the same thread that created them.

NOTE: Eventually, we will want a better Scheduler that can handle multiple threads. I suspect the Cilk/Intel Threading Building Blocks "work-stealing" model is the right one, where `Thunk`s are posted in the queue of the thread that created them because the data they need is more likely to be in cache, but idle threads can "steal work" from the tail end of another thread's work queue. The idea is that if all threads are busy they process their own data, which is optimal, and if one runs out of work to do it steals a "tail" task, which won't be run for the longest time and so will benefit the least from cache warmness.



# Memory management

`Continuation`s and Awaitables form a chain of heap-allocated objects
completely separate from the program stack (i.e. "stackless coroutines").
This is good because their lifetimes naturally may outlive the
activation record that produced them.

Ultimately an async operation will "bottom out" in system code
somewhere, perhaps waiting for a network packet. That code needs to
hold on to the Awaitable object for later, so it can store the result
when it's ready. This raises two substantial challenges:

- We need to support long-lived pointers from C++ structures into the Obstack, in a way that the GC understands.

- We need a way for an I/O thread to examine and update the state of an Awaitable even while it's being manipulated in a different thread, including GC running.

The solution is to introduce *pinned* pointers to the GC, as described in [T25667026](https://our.intern.facebook.com/intern/tasks/?t=25667026), and use them for Awaitable and `Continuation` objects. This allows event-handling threads to do some basic operations on these objects without taking any locks.



# Code generation

Skip supports both async functions and async blocks within functions. Since an async function is really just a function with an async block wrapped around its entire body, we apply that desugaring early and only concern ourselves with the design of async blocks.

An `async` block produces an Awaitable as its value and allows the `await` operator within.

## Coroutines

A coroutine is a function that can (logically) restart in the middle, picking up where it left off. Each coroutine is a subclass of `Thunk`, implemented as something much like a `() -> void` lambda. Calling the lambda "wakes it up" just after the last `await` that suspended it.

To make this work, the coroutine's "captured" state contains several pieces of information:

- A pointer to the Awaitable whose value is being computed. The coroutine's job is ultimately to transition it to a `READY` state.
- A "state" integer indicating which `await` was most recently suspended.
- The values of local variables that are live after that `await` resumes.

On entry to the function, the coroutine does a `switch` on its state integer to jump to the right code block that wakes up from the correct suspend. This code block loads all live local variables for that point in the code from the coroutine state, then jumps to the code block that handles the `await` succeeding.

When an `await` finds the Awaitable's value is ready, it loads it and execution proceeds normally. When it's not ready, it suspends:

- store the integer ID if the stalling `await` into the coroutine state
- store the value of all live local variables into the coroutine state
- record the coroutine state (a subclass of `Thunk`) into the stalling Awaitable's `continuation` field (which is a different Awaitable from the one ultimately being computed by this coroutine)
- return from the coroutine

The next code to invoke the coroutine will pick up where it left off, as described above.

When the coroutine would naturally `return` a value, instead it records the value in the Awaitable and resumes any waiting `Continuations` (as described earlier for writers), then returns.

The entire body of the coroutine is wrapped in a `try` to report exceptions to the Awaitable. Exceptions broadly follow the same `resume` path as successful returns.

## Fast path

We expect, based on past experience, that `await` usually succeeds immediately. So as an optimization we clone each async block into two copies: a "fast path" optimistic copy inlined into the original function and a "slow path" external coroutine that logically computes the same value but supports suspend/resume. The hope is that we can avoid allocating that coroutine state in the common case.

If the "fast path" needs to suspend, it allocates coroutine state on the heap and then suspends it as described for coroutines. But rather than returning from the containing function, it just jumps to the end of the async block and continues executing the containing function. In other words, the "fast path" supports doing one suspend, but cannot resume -- the resume will "wake up" in the slow path coroutine.

The fast path and slow path must "line up" in that the fast path needs to suspend using the same coroutine state expected by the slow path. To make this happen in the face of compiler optimizations, we only "clone" the async block late in the optimization process. Each async block is initially desugared into a call to an external function containing the body of that block. The external function takes as parameters all values captured by the async block and returns an Awaitable. We optimize this function normally, and later, after it's fully optimized, we determine the format of the coroutine state then clone and transform it into an `@always_inline` function for the fast path, and a proper coroutine for the slow path.


# Memoization

The C++ memoizer's operation is inherently asynchronous, in part because if two threads try to compute the same value at the same time, rather than computing it multiple times, it only computes it once and uses callbacks to notify everyone waiting for a value.

At a high level, it's reasonable to think of every memoized function as actually producing an Awaitable under the covers, and then the compiler does an `awaitSynchronously` on it. Of course, waiting synchronously would be foolish if the memoized function returned an Awaitable, since we wouldn't want to block synchronously to get the memoized value just to store in an Awaitable -- we'd much rather set up the `Continuation` to fill in our Awaitable when the memoized function is ready, so we need to special case memoized functions that return Awaitables.

A more intriguing case is calling a memoized function that returns a non-Awaitable like `Int` inside a async block. Should that implicitly wait for the memoized function with an `await` rather than `awaitSynchronously`?

The current C++ implementation actually uses `folly::Future` instead of Awaitable, so we'll need some work to tie all of this together.

Note that, in C++, the actual reactive memoization graph stores values of type `T`, not `Awaitable<T>`. The reason is that to short-circuit "false positive" recomputation later, we need to tell if some function returned the same value it did last time. That won't work if the true value is obscured by an Awaitable wrapper. Instead, Skip code calling a memoized Awaitable function will get its own private Awaitable object that gets notified by C++ when the memoized value is eventually ready. We can special-case allowing a memoized function to return a "top-level" Awaitable, but unlike async parameters we cannot handle mutable Awaitables buried inside the memoized value (e.g. `^Vector<Int>` could be returned by a memoized function, but not `Vector<^Int>`).


# Future thoughts

1. If we solve a few thread-related GC issues, we could pretty easily add an API that allows a user to spawn off async tasks that run in other threads and get back an Awaitable with the result. This would be a generalization of the current parallel support.

1. Memoization is useful for parallelism. Parallelizing some programs, like a compiler, is tricky because different threads may need to compute the same result. Since we don't allow parallel threads to view the same mutable data, they can't update a data structure like a shared hash table. But the memoizer does provide a way to mediate access to the same stored result and only compute it once, as long as you can express the computation you want to do as a memoized function call.


# Questions

1. Is restricting async functions to only take logically frozen parameters acceptable? We need this for determinism, but it this acceptable to users?

1. Is the "fast path"/"slow path" optimization that expects Awaitables are usually ready actually worth it?

1. Do we extend the compiler's internal type system to know about "logically frozen"?

1. Should we delete the `Awaitable` class and only have `^` magic syntax, putting Awatiables on similar footing to function types?

1. We already have `async` blocks, why not `memoized` blocks as well? The same basic compiler mechanism could handle both.

1. **Tail awaits**?
 In theory we could optimize an async block that itself ends in an `await` to simply forward on the Awaitable created for the outer block rather than chaining in a new one. But doing this would seem to require a messy calling convention change or something.

1. HHVM and Skip each have their own Awaitable and event loops, how do they interact? Do we need to lash them together somehow?

1. I don't think it would be hard for the compiler to parallelize multiple `await` statements that have no data dependency. There are cases where maybe you don't want auto-parallelization (e.g. maybe you think the first one might throw an exception), but wouldn't it be better to provide some kind of `awaitBarrier()` intrinsic than force everyone to tediously lash together their awaits into containers?
