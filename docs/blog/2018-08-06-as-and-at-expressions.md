---
title: as expression and @ patterns
author: Aditya Durvasula
---

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

## How do as expressions work?

The expression

```
x as <pattern>
```

is desugared to

```
x match {
| tmp @ <pattern> ->
  // 'tmp @' is an "at pattern" that introduces the local 'tmp' in that branch,
  // bound to the value matched at that position in the pattern
  tmp
| _ -> throw InvalidCast()
}
```

## Why have as expressions?

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

## as patterns become @ patterns

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
