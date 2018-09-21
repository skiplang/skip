# Skip Language Design Log

This document captures discussions regarding the design of the Skip language.
It is intended to capture our exploration of the design space, including
not only the design being executed on, but also the motivation of that design,
as well as discussions around alternatives not taken, and designs that are still
in progress.

## Document Format

Each entry is dated, and entries are sorted in chronological order.
Entries should include the status of the discussion: proposal, accepted, implemented.
Include attribution of comments/opinions where appropriate.
Include topic hashtags to enable easy cross referencing.

# May 3, 2018: yield/generators
Author: Mat Hostetter
Status: Implemented
Tags: #yield, #generators

If you've ever used the yield keyword in Python, Hack, Javascript, C#, etc., you know how handy generators can be. And now Skip has them!

While implementing async/await, I noticed that the coroutine support I was adding to the compiler would make generators easy to implement. So I took a brief detour and added them to the language too.

## A few examples
Suppose you want to write a function that produces a stream of square numbers, but without actually allocating a container to hold them all. You can now write:

```js
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
```js
    x.values()
      .filter(a -> a.isValid())
      .map(b -> b.name)
```

Previously, Iterator's filter() method created an instance of a custom FilterIterator class that wraps its own value stream:
```js
  overridable mutable fun filter(p: T -> Bool): mutable Iterator<T> {
    mutable FilterIterator(this, p)
  }
```
By itself that looks fine, but of course it requires writing a separate FilterIterator class, and that class does a bunch of messy stuff (don’t worry about the details):
```js
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

That code was written before Skip supported early return, so it could be made more concise. But with generators, we can go much farther than incremental tweaking; in fact, the FilterIterator class is now completely gone, leaving us with only this method on Iterator:
```js
  overridable mutable fun filter(p: T -> Bool): mutable Iterator<T> {
    foreach (v in this) {
      if (p(v)) yield v
    }
  }
```
I think you'll agree that's much easier to read!

## Implementation details
Skip's generators are implemented in basically the same way that other languages do.

For each function containing a yield, the compiler creates a customIterator subclass whose next() method holds a transformed version of the the user’s function. The original function is replaced with a stub that simply returns a new instance of that subclass.

The transformed next() method weaves a simple state machine into the user's code, with states corresponding to "initial call", "done", or which specific yield was executed most recently. On entry, next() does a switch on the state number field, resuming execution where it last left off.

Each yield x saves that yield's state number and any live local variables into fields, so they can be restored on the next call to next(), then returns Some(x). When next() is completely finished, it returns None().

The Skip compiler primarily targets LLVM, although there is also Peter Hallam’s Javascript back end, which transpiles to native Javascript generators. I looked into leveraging LLVM's coroutine support, but I couldn't use it because I need to control the coroutine suspend state in order to describe it to Skip's garbage collector. But even if I could solve that, the strategy of desugaring generators into normal Skip objects allows us to apply our  standard mid-level optimizations to that code before handing it to LLVM, including potentially inlining all of the generator code and removing the heap allocation.

# July 7, 2018: ?? Operator
Author: Christopher Chedeau
Status: Proposal
Tags: #expressions

Right now if we have an ?T, we can do `x.default(y)` but it eagerly evaluates y. It would be nice to have a version that doesn't.

In Hack and (very recently) JavaScript, there's now a ?? operator which lets us do that: `x ?? y`.
What do people think about bringing this to Skip?

Scott Wolchok: This operator is spelled ?: in a GNU extension to C and C++ and a bunch of other languages, see https://en.m.wikipedia.org/wiki/Elvis_operator .

Todd Nowacki: If we add it, I'd suggest making it more general than just option (like we do for our collection syntax). For example:
```
x ?? y
```
becomes

```
x.unbox() match {
| Some(v) -> v
| None() -> y
}
```

# July 18, 2018: Async/Await
Author: Mat Hostetter
Status: Implemented
Tags: #async, #await

Skip now has async/await, which should look familiar to Hack and Javascript programmers:

```js
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
```js
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

# July 20, 2018: Integral Types
Author: Aditya Durvasula
Status: Implemented
Tags: #types, #prelude

Skip now supports UInt8, UInt16, UInt32, Int8, Int16, Int32 along with the original Int (64 bit) type.

## HOW DOES THIS WORK?

The new integer types can be used to efficiently store a value in a specific range, while all operations always work by first converting to 64 bit Ints. Conversion back to a smaller integer type is explicit and the user can choose to truncate the value or fail if it is outside the representable range.

## EXAMPLES
```js
// Int literals are 64 bit
x = 20;    
y = 100;
// All operations return Int
z: Int = x + y;

// Other integer types are created with a method call:
x8 = UInt8::create(20);
y32 = Int32::create(100);
// Operations convert to Int first and return an Int:
z: Int = x8 + y32;

z8: UInt8 = UInt8::truncate(z); // ignore overflow
z8: UInt8 = UInt8::create(z);   // fail if overflow
```

## HOW DOES THIS WORK?

The original Int class is the workhorse that provides the implementations for all the useful math and comparison methods by compiling down to LLVM or JS. Each operation is specialized on the types of the arguments so that operations between two Ints incurs no additional overhead. Operations between other integer types will inline the conversion and operation together.

## WHAT NEXT?

Now that we have the UInt8 type we plan to add more features for working with binary data. Aaron Orenstein is already adding support for converting between String encodings and converting a String from utf8 bytes. We're also planning to add support for binary IO as well as serialization of Skip objects to/from binary formats such as Thrift.

# Jul 24, 2018: Macros
Author: Peter Hallam
Status: Implemented
Tags: #macros, #expressions

Skip now has a limited form of macros. They are particularly handy for default implementations of common traits.

For example:

```js
trait .Hashable {
  overridable macro fun hash(): Int {
    h = #thisClassName.hash();
    #forEachField (#field) !h = combine(h, this.#field);
    h
  }
}

class Person{name: String, age: Int} uses Hashable {
  // NOTE: No hash() implementation required.
}
```

In the above example the implementation of  Person.hash() is:

```js
  fun hash(): Int {
    h = "Person".hash();
    {
      !h = combine(h, this.name);
      !h = combine(h, this.age);
    };
    h
  }
```

Also note that the `"Person".hash()` expression is constant folded at compile time and initializes `h` to an integer literal.

## `macro` Methods

- All macros start with `#` to visually distinguish them from normal, non-macro code.
- Macros may only be used within methods with the `macro` modifier.
- `macro` methods may be defined on traits as well as on classes.
- `macro` methods are expanded in leaf classes.
- `macro` methods are expanded after parsing and before type checking.
- The restrictions on the use of macros ensure that macros are expanded into valid parse trees.
- You will see type errors in the expanded bodies of `macro` methods. These errors use the same reporting techniques we use for deferred methods, so the error reporting should be easy to follow.

## Macros

The macros currently implemented are:

`#thisClassName`

Expands to a string literal containing the fully qualified name of the leaf class the method is being expanded into. It may be used as an expression.

`#ThisClass`

Expands to the class type of the class being expanded into. It may be used as a type annotation as well as in pattern matching.

`#forEachField(#fieldIdentifier [, #fieldNameLiteral]) body`

`#forEachField` expands to an expression sequence where the `body` expression is expanded once for each field in the containing class. The expansion includes all fields in the class including those inherited from base classes.

Within `body` the `#fieldIdentifier` macro expands to an identifier equal to the name of the field being expanded. It may be used as an expression, or more commonly on the right hand side of the `.` member access operator.

If present, the `#fieldNameLiteral` macro is also available in the `body` expression and expands to a string literal containing the name of the field being expanded.

## Current Uses of Macros

Macros currently provide default implementations for the following traits:

- Hashable
- Equality
- Orderable

You should rarely need to provide manual implementations of these traits.

## Equality Example

The `Equality` implementation shows the use of the `#ThisClass` macro used in a pattern match:

```js
trait Equality {
  overridable macro readonly fun ==(other: inst): Bool {
    other match {
    | #ThisClass _ as otherTyped ->
      // silence unused variable warning in classes
      // with no fields.
      _ = otherTyped;

      #forEachField (#field) {
        if (this.#field != otherTyped.#field) {
          return false;
        }
      };
      true

    | _ -> false
    }
  }
}
```

## JSON Example

Here's an example using the field literal version of `#forEachField` to implement JSON serialization:

```js
trait Jsonable {
  overridable macro fun toJson(): JSON.Value {
    fields = mutable Map<String, JSON.Value>[];
    #forEachField (#field, #fieldName) {
      this.#field.toJson() match {
      | JSON.Null() -> void
      | value -> fields.set(#fieldName, value)
      }
    };

    JSON.Object(freeze(fields))
  }
}
```

# Jul, 30, 2018: Expose pointer comparisons of interned objects?

I need to start using strings as hash keys pretty heavily, and I can afford to intern them in advance. What would people think about a class that wrapped this this idiom, allowing simple, safe pointer compares of interned objects?

This does expose a lot of information, allowing you to detect whether interning decided two objects are structurally identical, which would tie our hands a bit -- we may or may not think two binary trees are structurally identical depending how they are balanced, etc. It also effectively lets you compare private fields you couldn't otherwise read.

I could restrict this just to String, but that seems a bit harsh too...what if you want (String, String) pairs or whatever?

```js
value class Interned<+T: frozen> private (
  value: T,
) uses Equality, Hashable[T: Hashable] {
  static fun create(value: T): Interned<T> {
    Interned(intern(value))
  }

  @intrinsic
  private static fun internEqual(a: T, b: T): Bool;

  fun ==<U: Equality>[T: U](other: Interned<U>): Bool {
    Interned<U>::internEqual((this : U), other)
  }

  fun hash[T: Hashable](): Int {
    // NOTE: Even though it would be faster, we don't hash the interned
    // pointer since that wouldn't be deterministic. If accessed only via
    // an insertion-order preserving Map, we could maybe get away with
    // hashing the pointer since the program couldn't tell, and currently
    // interned objects never move.
    this.value.hash()
  }
}
```

Basil Hosmer: Feels kind of leaky to allow it on objects with encapsulated representations... what about just value classes with all public fields? Bummer that that restriction isn't expressible in source, but that's secondary.

Peter Hallam: There are several places in the compiler where we are doing painful workarounds due to the lack of an identity comparison operator in the language. Identity comparison is something every programming language I know of has and programmers will expect it. This has been on my language wishlist since the beginning.

Mat Hostetter: Of course, we can only expose reference equality for mutable or interned objects. Otherwise interning an object (during memoization, say) could change its behavior.

Julien Verlaguet:
“every programming language I know has ....” sure, but it’s because we need it.
We need to be able to intern objects without changing the behavior of the program.

# Jul 31, 2018: Replacing as patterns with @
Author: Todd Nowacki
Status: Proposal
Tags: #as, #@, #patterns

Aditya just landed as expressions! (See here)

I had posted to the Skip Core discussion thread before Aditya started the work, expressing some concern about the double duty of as.

At the time, people did not seem bothered by the usage. And it wasn't obviously an issue of any confusing.
BUT, looking forward, I fear that this will be particularly confusing as more features are added to the language. Particularly, any other feature that uses patterns has to be concerned about this as-overloading, which doesn't feel like a great long term design decision.

In particular, irrefutable patterns are a feature that are much more attractive with as-casts, similarly as-casts are much more useful with irrefutable patterns.  But with irrefutable patterns, as-patterns become very gross, and confusing to read. On top of that, it might be problematic for the grammar (not sure on that one though).

## Quick recap on irrefutable patterns:

```js
value class Name(pos: FileRange, id: String)
...
cd: OuterIst.Class_Def
Name(pos, id) = cd.name;
```

The lvalue, would become a pattern, and it would be acceptable as long as the match is exhaustive (hence irrefutable).

Why is it more useful with as-casts?

The combo of the two features lets you do some quick destructuring and introduce the bindings into local scope, without having to nest matches, or dance around lifting the locals into an outer scope.

For example

```js
base class Tree {
  children = Node(left: Tree, value: Int, right: Tree) | Nil()
}
...
tree match {
| Node(left, _, right) ->
  <use left>
  <use right>
| Nil() -> invariant_violation("BAD")
}
or similarly
(left, right) = tree match {
| Node(left, _, right) ->
  (left, right)
| Nil() -> invariant_violation("BAD")
};
<use left>
<use right>
```

can become

```js
Node(left, _, right) = tree as Node _;
<use left>
<use right>
```

## What does this do for as-patterns?
This gets super weird if we have as overloaded for cats and patterns.
Consider:

```js
fun initBox(pointOpt: ?(Int, Int)): Box {
  Some((x, _) as point) = pointOpt as Some _;
  makeBox((x, 0), point)
}
```

Having as on either side of the binding, where the meanings don't line up, is super gross.
I could come up with more examples where I put as-cast next to as-pattern, but you get the point.
Replacing as-patterns?
I suggest we go the Haskell/Rust approach for as-pattern and use @
So
```
<pattern> as <ident>
```
becomes
```
<ident> @ <pattern>
```
I think its the best of all the options I could think of,
see all of them here:
https://fburl.com/q7ptu0mw
- @ beats out = becomes of some weirdness.
- = for patterns is non-symetrical with the corresponding bind (in that for bind, the LHS introduces bindings where the right is a value. For patterns both sides introduce bindings and neither is a pattern). And its completely incomprehensible with irrefutable patterns.

For symmetry

```js
opt = Some(a, b);
```
opt is the binding that is introduced, not a or b.

But if it were to be used as a pattern
```js
| opt = Some(a, b)
```
opt, a, and b are introduced.
For irrefutable patterns
```js
(point = (x, _)) =  getPoint()
```
I doubt anyone would read this and be able to correctly guess what it does.

## TL;DR
Overloading as for patterns and casts will limit the design of the language long term, and causes a few confusing situations.
as-cast is common in other languages, and now in Hack, so we should share that syntax as much as we can.
as-patterns are declared with @ in Haskell and Rust, with the form of point @ (x, _). This is a good alternative syntax that will be familiar to at least some users (just not OCaml users)


# Aug 2, 2018: Compile-time constant-sized Arrays?
Author: Joe Savona
Status: Proposal
Tags: #types

A common approach to implementing persistent vectors (radix balanced trees) is a tree with a 32-way branching. The basic structure in Skip is:

```js
const branchFactor: Int = 32;
base class Node<+T> {
  children =
  | Internal(childNodes: Array<?Node<T>>)
  | Leaf(size: Int, elements: Array<?T>)
}
```

This is inefficient for two reasons:
- Both Arrays will always have size=branchFactor, so storing the array's size field in the Array value is technically overhead.
- More importantly, we could inline the storage of the Array in the Internal/Leaf node to avoid another pointer indirection. The idea of using wide nodes is to reduce time spent chasing pointers, but the setup as illustrated means that for every Node accessed we have to follow the pointer to the node and then its array.
Given that we already allow non-bounds checked Unsafe.array_get that we can eventually lock down for use only by the compiler/standard library, what do people think of compile-time constant Arrays that can be inlined to avoid the pointer indirection?

Mat Hostetter: Sure. C++ has this. Edwin Smith and I discussed this in the context of porting F14 SIMD operations to Skip. Not sure what the syntax would be though, because (unlike C++) you can't use an Int as a tparam.

# Aug 6, 2018 - 'as' Expressions and '@' Patterns
Author: Aditya Durvasula
Status: Implemented
Tags: #as, #@, #expressions, #patterns

`as` does runtime casting from a base class to any of its children by attempting to match an object (lhs) with a pattern (rhs). If it succeeds, `as` returns the object casted to the type of the pattern. Otherwise, it throws an exception. `as` is really similar to the `is` expression which returns `true` or `false` instead of performing a cast.

Here's a quick overview:

```
base class Animal
class Dog(int: Int) extends Animal
class Cat(string: String) extends Animal

fun main(): void {
  x: Animal = Dog(0);
  ex1 = x as Dog(0); // type of ex1 is Dog!
  ex1Bool = x is Dog(0); // true
  ex2 = x as Dog(1); // will throw an exception, but ex2 : Dog
  ex2Bool = x is Dog(1); // false
  ex3 = x as Cat("foo"); // will throw an exception, but ex3 : Cat
  ex3Bool = x is Cat("foo") // false
}
```

## How do `as` expressions work?

The expression

```
x as <pattern>
```

is desugared to

```
x match {
| tmp @ <pattern> -> tmp   // 'tmp @' is an "at pattern" that introduces the local 'tmp' in that branch, bound to the value matched at that position in the pattern
| _ -> throw InvalidCast()
}
```

## Why have `as` expressions?

Consider the following Abstract Datatype.

```
`base class Animal
class Dog() extends Animal
class Cat() extends Animal
...
class Zebra() extends Animal
```

Before the `as` expression, we would define helper methods to cast to each child class of the ADT.

```
fun asDog(data: Animal): Dog {
  data match {
  | tmp @ A _ -> tmp
  | _ -> throw InvalidCast()
  }
}

fun asCat(data: Animal): Cat {
  data match {
  | tmp @ B _ -> tmp
  | _ -> throw InvalidCast()
  }
}
```

Now, with `as`, we don't need any of these functions because `as` effectively desugars to these functions.

```
fun bar(): void {
  x: Foo = A();
  a = x as A _; // no need to define asA and do (a = asA(x))
}
```

## Why use patterns instead of types?

Patterns let us leverage existing machinery already in place `match` to perform “cast”-like behavior. Additionally, this let's us perform more restrictive casts than a basic typecast. Deep introspection would not be possible with a simple `instanceof` like typecast. For example:

```
base class B
class C1(int: Int) extends B
class C2(string: String) extends B

fun foo(): void {
  x: ?B = Some(C1(4));
  y: ?C1 = x as Some(C1 _) // no way to express nested patterns with a typecast
}
```

## `as` patterns become `@` patterns

While the two pieces of syntax exist in different namespaces, to avoid potential conflicts with the `as` pattern and have less confusing future syntax for irrefutable patterns/bindings, Todd Nowacki suggested removing the `as` pattern and using `@` instead. Now, code like

```
x match {
| Some _ as y -> y
...
}
```

instead looks like

```
x match {
| y @ Some _ -> y
...
}
```
