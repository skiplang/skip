---
title: Understanding Mutability
author: Todd Nowacki
---

Unlike many other languages, Skip favors immutability by default, with the ability to make things mutable explicitly. And as such, we've added a good bit of syntax and terminology that you would not normally find in an Object Oriented language. That being said, I think our system is remarkably simple given the amount of expressive power, and hopefully, it will be easy to understand!

This post will cover the principles of the mutability model in Skip

## Overview

Skip is designed first and foremost as a language to support reactivity (memoization with computation-dependent invalidation). In order to do this soundly, objects need to be immutable. As such, objects are immutable by default in Skip. However, when objects are immutable, it is often difficult to build up collections or partial results without a lot of copying, which can be slow. Additionally, some designs or ideas just aren't expressible with immutability, e.g. references. To support this, we have the ability to explicitly declare instances of objects as mutable.

A quick overview of these rules:
* Object types have a mutability "mode"
* Objects types have the immutable mode by default
* There is no immutable modifier in the syntax
* Objects can be explicitly instantiated as mutable
* `readonly` is a mode that lets code work both for mutable and immutable objects
* There are no global mutable values. All mutability is locally scoped, or explicitly declared at function boundaries.
* Objects are considered `frozen` if they are immutable and all of their type parameters are `frozen`
* Arguments to inputs and outputs to memoized functions must be `frozen`

## Basics of Mutable Objects

* Classes that *can have* mutable instances must be declared as `mutable`.
* The type of mutable instances of a class is the class name, preceded by the `mutable` type modifier. The class name by itself denotes the type of immutable instances of the class.
* Mutable instances are created by prefacing an object construction expression with `mutable`. An object construction expression without `mutable` in front creates an immutable instance.
* Fields are made assignable by prefacing their declaration with `mutable`.
* Assignable fields on mutable objects are modified by using a `!` on the left-hand side of an assignment.
* Mutable instances can be made immutable with `freeze()`.
* While a class's type parameters can be declared to be covariant or contravariant instead of the invariant default, those variances only apply to immutable and `readonly` instances. The type of a mutable instance is considered to have invariant type parameters.

Examples:

```
// Ref can have mutable instances
mutable class Ref<+T>(mutable value: T)

// Pet has no mutable modifier, so no mutable instances
base class Pet { children = Dog() | Cat() }

...
// mutable keyword before type and construction expression
ref : mutable Ref<Int> = mutable Ref(0);
ref.!value = 1; // assigns 1 to the vector
assert(ref.value == 1);
frozen_ref : Ref<Int> = freeze(ref); // create a fully immutable copy
// Immutable instances cannot have fields assigned
// ERROR frozen_ref.!value = 1

// ! can be placed anywhere in an lvalue, denoting the in-place update
// of the field immediately to the right of the !
nested : mutable Ref<mutable Ref<Int>> = mutable Ref(mutable Ref(0))
nested.value.!value = 100;

// Immutable instances are covariant as declared
_ : Ref<Pet> = Ref(Dog())
// Mutable instances are made invariant
// ERROR _ : mutable Ref<Pet> = mutable Ref(Dog())
```

## The Three Modes

Every object type has a mode: `immutable`, `mutable`, or `readonly`

immutable is the "default" mode, meaning that there is no modifier in the language for this mode. For example:

```
_ : Vector<Int> = Vector[]; // immutable Vector of Ints
_ : mutable Vector<Int> = mutable Vector[1, 2, 3]; // mutable Vector of Ints
// No direct way to create a readonly object
_ : readonly Vector<Int> = Vector[1, 2, 3]; // unmodifiable Vector of Ints
_ : readonly Vector<Int> = mutable Vector[1, 2, 3]; // unmodifiable Vector of Ints
```

## Subtyping

As seen above, `readonly` is the "parent" mode.

This gives us the following subtyping relations. Let `X` and `Y` be two object types:
```
        X <: Y
----------------------
mutable X <: mutable Y

        X <: Y
-----------------------
mutable X <: readonly Y

X <: Y
---------------
X <: readonly Y

         X <: Y
------------------------
readonly X <: readonly Y
```

(these rules become more complicated with generics, see **Mode Preservation** for details)

## Mode Semantics

The mode of a type can be thought of as modifying its declaration, as follows:

* **Immutable:** None of the object's fields can be modified. Any `mutable` or `readonly` field types become immutable (before type instantiation), and methods marked `mutable` (meaning they require a mutable instance of `this` to operate) become unavailable. (More on method modifiers below.) This property is recursive for any type in the object's fields.
* **Mutable:** Any of the object's fields can be modified if they are marked `mutable`. All field types keep their declared mode. Methods marked immutable (unmarked) or `frozen` become unavailable.
* **Readonly:** None of the object's fields can be modified, but the actual instance might be `mutable` (there could be a mutable reference to this object). Any `mutable` types become `readonly` (before type instantiation). Methods marked `mutable`, immutable (unmarked), or `frozen` become unavailable.

"Before type instantiation" means that this rule only applies to types that are not type parameters/generics. For example `readonly Vector<mutable Ref<Int>>` is a readonly object that points to mutable data.

Reading all of this as formal rules would make your eyes bleed. Instead, here are a few concrete examples:
```
// updatable ref to a mutable vector of some type T
// (we don't know anything about the mutability of T, yet)
mutable class VectorRef<T>(mutable value: mutable Vector<T>)

// note that the field here is not updatable, and the Vector type is immutable
// ...but we still don't know about the mutability of T
mutable class NestVector<T>(value: Vector<T>)

...

// Immutable cases

v1 : VectorRef<Int> = VectorRef(Vector[1, 2, 3]);
// note the 'mutable' mode has been "frozen" off
_ : Vector<Int> = v1.value;
// And the argument MUST be immutable
// ERROR _ = VectorRef(mutable Vector[1, 2, 3]);
// And fields cannot be modified
// ERROR v1.!value = Vector[4, 5, 6]

// An immutable value that contains a mutable value!
v2: VectorRef<mutable Ref<Int>> = VectorRef(Vector[mutable Ref(42)]);
// The field's written mutability is gone, but the generic's mutability is still around
_ : Vector<mutable Ref<Int>> = v2.value;
_ : mutable Ref<Int> = v2.value[0];

// Mutable cases

mv1 : mutable VectorRef<Int> = mutable VectorRef(mutable Vector[1, 2, 3]);
// The field retains its defined mode
_ : mutable Vector<Int> = mv1.value;
// And the argument MUST be mutable  
// ERROR _ = mutable VectorRef(Vector[1, 2, 3]

// The field retains its defined mode
mv2 : mutable NestVector<Int> = mutable NestVector(Vector[1, 2, 3]);
_ : Vector<Int> = mv2.value;

// Readonly cases

rv1 : readonly VectorRef<Int> = mutable VectorRef(mutable Vector[1, 2, 3]);
// The field's declared 'mutable' mode becomes 'readonly'
_ : readonly Vector<Int> = rv1.value;
// And fields cannot be modified
// ERROR rv1.!value = mutable Vector[4, 5, 6]

// mutable inside readonly value
rv2: readonly VectorRef<mutable Ref<Int>> = VectorRef(Vector[mutable Ref(42)]);
// The field's written mutability is gone, and replaced with 'readonly', but the generic's mutability remains
_ : readonly Vector<mutable Ref<int>> = rv2.value;
_ : mutable Ref<Int> = rv2.value[0];

// Immutable types retain their defined mode
rv3 : readonly NestVector<Int> = mutable NestVector(Vector[1, 2, 3])
_ : Vector<Int> = rv3.value
```

## The `frozen` Constraint

In several spots, such as with memoization, we need to know if a an object is "fully" immutable. Meaning that the object has to be mutable itself AND cannot contain any mutable values. For example `Vector<mutable Ref<Int>>` is immutable but not `frozen`.

The rule is as follows, given a class `C`:

`C<t0, ..., tn> : frozen` iff `t0 : frozen .... tn : frozen`

The constraint comes into play in two key places. The first, any generic can be marked as `frozen`

```
mutable class C<T1: frozen, T2>(x : T, y : T2) {
  fun test<V: frozen>(): void { ... }
  fun test2[T2: frozen](): void { ... }
}
```

Additionally, for memoized functions, all inputs and outputs must be `frozen`.

# The `freeze()` operator

Skip provides a native operator `freeze(e)`. That given any expression, it will give a deeply frozen copy such that `freeze(e) : frozen`. Note that not all types can be frozen (namely `->` and `^` more on this later)

## Method Modifiers

There are 3 method modifiers related to mutability, all of them modify the mutability of `this` inside the method (note they cannot be used on static methods)

* **no modifier** `this` is immutable. The method can be called only on immutable instances.
* **`mutable`** `this` is mutable. The method can be called only on mutable instances.
* **`readonly`** `this` is readonly. The method can be called on any instance.
* **`frozen`** `this` is frozen. The method can be called only on instances that satisfy the `frozen` constraint. For example:

```
mutable class C<T1, T2> {
  frozen fun ex(): void { ... }
  // is equivalent to  
  fun equivalent_ex[T1: frozen, T2: frozen](): void { this.ex() }
}
```

### Relaxed Override Rules

As you might imagine based on the subtyping rules.
* **no modifier** Can be overridden in the child by immutable or `readonly`
* **`mutable`** Can be overridden in the child by `mutable` or `readonly`
* **`readonly`** Can be overridden in the child by `readonly`
* **`frozen`** Can be overridden in the child by `frozen`, immutable, or `readonly`

But there is a further relaxing that you can be achieved. If a class isn't marked as `mutable`, it means that that class, and any of it's children, cannot have mutable fields nor can they refer to any objects statically that have mutable fields. So from a typing perspective, all three modes are equivalent! Thus we get this new set of rules

If the child is NOT marked as `mutable`
* **no modifier**, **`mutable`** and **`readonly`** Can be overridden in the child by `mutable`, immutable, or `readonly`
* **`frozen`** Can be overridden in the child by `frozen`, immutable, or `readonly`

Note that as with the previous set of rules, a `frozen` child method *must* be `frozen` in the parent. Even though the three modes are equivalent, **immutable** is not the same as `frozen`.

## Two Types of Lambdas

While lambda types are not objects in our type system, similar to objects they have a mutable and immutable mode.
* `->` This type of lambda can capture any type of value.
  * If a field contains a `->` in it's type, it can only be accessed from a `mutable`. There is no immutable or `readonly` view of `->`
* `~>` This type of lambda is considered "pure".   
  * It cannot capture any mutable values, additionally it cannot capture any local variable that was modified (past it's initial declaration).   
  * `~>` is a subtype of `->`, much like immutable is a subtype of `readonly`   
  * A `~>` type is `frozen`, regardless of it's parameter or return types.

## The Awaitable Type

The awaitable type `^t` behaves very similarly to `->`
* For any `t`, `^t` is not `frozen`
* If a field contains the type `^t`, it can only be accessed from a `mutable`. There is no immutable or `readonly` view of `->`

## Restrictions on Mutable Values

As mentioned, to provide certain guarantees for reactivity, we cannot allow mutability values in all locations.

Here are the restrictions
* You cannot have mutable `const` declarations. All global (and class) constant must be `frozen`
* As previously mentioned, `~>` cannot capture mutable values
* the `if` clause of a pattern match cannot modify any `mutable` value.
* All parameters to a memoized function must be `frozen`
* The return type of a memoized function must be `^t` or `t` where `t: frozen`
* memoized methods must have a `this: frozen`. This is easily done by marking the method as `frozen`
* `async { }` blocks cannot capture mutable values, additionally it cannot capture any local variable that was modified (past it's initial declaration)
* All parameters to a async function (and `this` for an async method) must be `frozen`   
  * However, just for these async contexts, we consider `^t : frozen` if `t : frozen`. This is safe due to the awaitable type is "logically frozen", meaning you cannot modify any of it's state

## Inferring `frozen` on certain functions

With the rules memoized and async functions/methods, it would be annoying to have to write `: frozen` on all of your type parameters.

Anywhere a type parameter appears in a memo/async function/method, where it would need to be frozen, we will infer the `frozen` constraint.

For example:

```
memoized fun ex<T>(Vector<T>): Map<Int, T> { ... }
// equivalent to
memoized fun ex<T: frozen>(Vector<T>): Map<Int, T> { ... }

async fun ex2<T, U>(Vector<T>, T ~> U): ^Vector<U>
// equivalent to
async fun ex2<T: frozen, U>(Vector<T>, T ~> U): ^Vector<U>
// note no need for U to be frozen

class Tester<T, U>(...)
  memoized fun ex(Vector<T>): U { ... }
  // equivalent to   
  memoized fun ex[T: frozen, U: frozen](Vector<T>): U { ... }

  async fun ex2(Vector<T>): ^U { ... }
  // equivalent to   
  async fun ex2[T: frozen](Vector<T>): ^U { ... }
}
```

## Mode Preservation

Recall from above that when doing subtyping checks, there certain rules around the "subtyping" of modes.

If we make immutable mode (even though it is implicit in the concrete syntax), we can generalize with a submode judgement `<|`

```
 X <: Y    m1 <| m2
--------------------
   m1 X <: m2 Y

----------------------
immutable <| immutable      

------------------
mutable <| mutable  

--------------------
readonly <| readonly  

---------------------
immutable <| readonly  

-------------------
mutable <| readonly  
```

Recall also that

```
 t1 : frozen ... tn : frozen
-----------------------------
   C<t1, ..., tn> : frozen
```

The new immutable mode, and the combination of these rules gives rise to a few properties that we need for these sub-typing relations to be sound.

Namely we need to know that for any type `t1`, `t2` where `mode(t1) = mode(t2) = immutable`
* If `not(t2 : frozen)` and `t1 <: t2`, `not(t1: frozen)`
* If `t1 : frozen` and `t1 <: t2`, `t2 : frozen`

We refer to these properties as **Mutability Preservation** and **Frozen Preservation** respectively. These rules are *so* critical that without them, we would not be able to allow mutable instantiations of type parameters for immutable objects.

In other words, without **Mode Preservation**, we would have to require that immutable objects are instantiated only with `frozen` type arguments.

### Mutability Preservation

Since the `frozen` constraint is critical to the safety around mutability, we need to ensure that the rule cannot be bypassed by upcasting. For example:

```
base class Parent
class Boxy<T>(value: T) extends Parent
```

It would be very dangerous if `Boxy<mutable Vector<Int>> <: Parent` since `Parent: frozen` but `not(Boxy<mutable Vector<Int>>: frozen)`! So you could break *any* of our immutability guarantees just by upcasting; this would be very, very bad.

This is why we have the mutability preservation rule.

In the implementation, when we are performing the subtyping relation in the type checker, if the new parent type is frozen, find all of the type parameters in the subtype that are NOT visible in the super type. Then verify that those types are frozen. If it contains a type variable, we just add a constraint that the type variable must be frozen.

For example: To check `Boxy<mutable Vector<Int>> <: Parent`
* `Parent: frozen` so check the missing type parameter of the child
* `T` for `Boxy` isn't visible
* `T` is instantiated with `mutable Vector<Int>`
* `mutable Vector<Int>` isn't mutable so ERROR

In another example, where `a` is a type variable, to check `Boxy<Vector<a>> <: Parent`
* `Parent: frozen` so check the missing type parameter of the child
* `T` for `Boxy` isn't visible
* `T` is instantiated with `Vector<a>`
* `Vector<a>` is `frozen` iff `a` is frozen so add a constraint to the environment that `a : frozen`

### Frozen Preservation

In the reverse case, we need to be concerned with casting changing the mode of any type. This might seem counterintuitive that it would matter that you lose `frozen` when upcasting, since we do allow upcasting from immutable into `readonly`. BUT this is important when upcasting from one immutable type to another.

For example:

```
base class Parent<+T: mutable Ref<Int>>(v: T)
class Child extends Parent<mutable Ref<Int>>

fun no1(ref: Ref<Int>): void {
  c = Child(ref);
  p: Parent<mutable Ref<Int>> = c;
  p.v.set(42) // modifying an immutable object!
}

fun no2(p: Parent<mutable Ref<Int>>): Ref<Int> {
  p match {
  | Foo(ref) ->
    _: Ref<Int> = ref // mutable cast to frozen!
  }
}
```

This rule is very easy to implement in practice since it works exactly as described: When upcasting to a type, if it's immutable and not `frozen`, check the original type is not `frozen`

## Summary

A quick wrap-up
* Three modes for objects immutable, `mutable`, and `readonly`
* Objects are immutable by default and can be explicitly instantiated as `mutable`
* `readonly` acts as a "supertype" between `mutable` and immutable instances
* `frozen` is a constraint that guarantees deep, full immutability.
* In certain spots, only `frozen` objects can be used
* The mode of the object can change the mode of the types in the field
* Unless they are marked as `frozen`, type parameters can be instantiated with mutable types
* **Mutability and Frozen Preservation** allow for mutable type arguments to immutable objects
