---
title: Conditional Trait Usage
author: Todd Nowacki
---

A long requested feature of conditional usage has finally landed!

## Quick Overview of Traits

In Skip, traits cannot be used in normal types, but can be used as constraints on generics. This limitation let's code to be easily shared between value classes and normal, reference classes. Additionally, it let's us capture behavior that cannot normally be expressed by traditional interfaces in other OO languages, for example, we can definite the equality operator `==` in the `Equality` trait.

Concretely we have

```
// inst get's substituted for the class that uses the trait
trait Equality {
  // A user of the trait must define this method
  readonly fun ==(other: inst): Bool;
  // They get this method for free
  overridable readonly fun !=(other: inst): Bool {
    !(this == other)
  }
}

fun selfEq<T: Equality>(x: T): Bool {
  // inst is substituted with T
  x == x
}
```

## Conditional Methods

Often when defining methods like == or toString for collections, the collection needs some handle to the function for it's underlying values. In OO languages, this is often done by defining a static method, that either adds an interface constraint to a generic, or has a lambda that is used to perform the operation.

For example

```
mutable class Vector<+T>(...) {
  static fun toString<V: Show>(v: readonly Vector<V>): String { ... }
  static fun eqBy<V>(v: readonly Vector<V>, eq: (V, V) -> Bool): Bool { ... }
}
```

But, with conditional methods, we can do better!

This feature adds a set of constraints written as `[T1: C1, T2: C2 & C3]`, where the method can be called only when the constraints are met. (This is similar to where types in other languages, for example in Hack)

We can then modify these examples to normal `==`, `toString` instance methods.

For example

```
mutable class Vector<+T>(...) {
  readonly fun toString[T: Show](): String { ... }
  readonly fun ==[T: Equality](): Bool { ... }
}
```

Even though `Vector` now has `==` and `toString`, it cannot use `Equality` nor `Show`, since it has these additional constraints. This brings us too...

## Conditional Trait Usage

Combining traits and conditional usage, we get conditional trait usage. The syntax is as follows

```
class MyClass<T>() uses MyTrait<T>[T: X]
```

And can be read as "MyClass uses MyTrait of T, when T is a subtype of X"

We can then modify our example before

```
mutable class Vector<+T>(...) uses Show[T: Show], Equality[T: Equality] {
  readonly fun toString[T: Show](): String { ... }
  readonly fun ==[T: Equality](): Bool { ... }
}
```

## Inheriting Methods

When inheriting method implementations, any conditions on the trait usage are added to the method. For example:

```
class MyClass<T>(x: T) uses Equality[T: Equality] {
  // Explicit declaration
  fun == [T: Equality](other: MyClass<T>): Bool {
    this.x == other.x
  }

 /* Implicit, inherited declaration
  * fun != [T: Equality](other: MyClass<T>): Bool {
  *   not(this == other)
  * }
  */
}
```

# Inheriting Parent Traits

When inheriting trait usage from another trait, any conditions on the trait usage are added to the method. For example:

```
trait Orderable extends Equality {
  ...
}
```

```
class MyClass<T>(x: T)
  uses Orderable[T: Orderable] /* (implicit) Equality[T: Orderable] */ {
}
```

This can lead to some nasty "gotchas", as you might not realize that it is NOT the case that `MyClass<T> uses Equality[T: Equality]`

For example:

```
class HasOrd() uses Orderable { ... }
class HasEq(): uses Equality { ... }
...
selfEq(MyClass(HasOrd())); // Valid!
// ERROR MyClass does not have Equality since HasEq does not have Orderable
// selfEq(MyClass(HasEq()));
```

# Limitations
I'll try to explain this very quickly, but I will be glossing over a lot of detail.

The TL;DR

The implementation of conditional trait usage leverages on the fact that the Skip type checker only looks at class hierarchies lazily AND that trait constraints tend to be solved only once type inference is complete.

If the type system yells and says this object doesn't use a given trait, but you think the conditions are met, try annotating your type

---

In the Skip type checker, we implement subtyping by expanding any object type into it's full hierarchy. Then we can implement various subtyping relations by using a combination of set operations. This system is very powerful, and deserves it's own post, but the important thing to know is that to do these set operations, we have to fully expand a type into it's hierarchy.

In some cases, we can be lazy and delay the expansion of the hierarchy until we need to. It was much easier to implement conditional uses via this lazy expansion, since no new information had to be added to these sets of types.

But, there are some situations where this expansion is forced, most commonly is when combining the branches of if-expressions with two children of the same hierarchy.

In cases like that, the type system might fail to see a conditional trait usage is valid.

Example:

```
base class P<T>(value: T) uses Equality[T: Equality] {
  children = L | R
}
...
// f : () -> alpha
f = () -> 0;
// p : P<alpha>, the trait was dropped since alpha wasn't known
p = if (true) L(f()) else R(f());
// ERROR P<Int> does not use Equality
selfEq(p)
```

In this example, the type system expands up the hierarchy to find the type of p. But since we do not know the type of alpha yet, the type system is forced to drop the conditional trait. And thus we get this weird error.

Note though, that the code will work if you annotate `p : P<Int>`

## Future Work

There are two major areas that need to be improved.

**Error Messages**: Currently the error message sucks. In the example above, it will just say that `P<Int>` doesn't have `Equality`. Which is very confusing. The error message needs to indicate that the conditional check was likely failed to be inferred correctly due to some usage at the type. And the type should be annotated

**Conditions Don't Affect Constraints**: If you do something like `[Vector<T>: Show]` It will not infer `[T: Show]` even though `Vector<T>` uses `Show[T: Show]`. It will require some reworking of parts of typing to keep track of these constraints (we need to not throw out certain bits of information)

## Summary

- Conditional Traits are a great tool for Collection-like objects
- Conditional Trait usage adds the conditions to any inherited method
- Conditional Trait usage adds the conditions to any inherited traits
- If the type system freaks at you and thinks the trait isn't there, annotate the type
- The conditions do not yet propagate for generic's constraints.
