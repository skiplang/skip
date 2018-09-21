---
id: classes
title: Classes
---

```
class Point(x: Int, y: Int) {}
```

Fields are defined right next to the name of the class. They also define the one and only constructor of the class. Of course it is possible to define static methods acting as factories building points, but there is not notion of a constructor in the classic sense: a function that manipulates the object before the initialization. The reason for this is that we want pattern-matching to work as the dual operation of object creation for any object. Concretely if I write `Point(x, y)` in an expression, this needs to be the dual operation of writing `Point(x, y)` in a pattern: one builds it, the other one de-sugars it.

**Note**: the default, for any object construction is to be immutable. In this case, writing `Point(.., ..)` means that we are building an immutable version of the class Point. More detail on that later.

## First Method

```
class Point(x: Int, y: Int) {
  fun add1X(): Point {
    Point(this.x + 1, this.y)
  }
}
```

There should be nothing surprising here. Except maybe that add1X is an immutable method: it is not possible to modify `this` within the body of add1X.

## Named parameters

```
class Point { x: Int, y: Int } {
  fun add1X(): Point {
    Point { x => this.x + 1, y => this.y };
  }
}

fun new_point0(): Point {
  Point { x => 0, y => 0 }
}
```

It is possible to defined named parameters for both classes and functions. This style is encouraged when the number of parameters is becoming too long, or to make the code more explicit. Named parameters are always introduced with curly braces `{}`, while positional ones use parenthesis `()`.
The reason for that distinction is again pattern-matching, we will very often need to pattern-match on objects with named parameters, when that happens it if often handy to define a local variable with the same name as the field, which we achieve by writing `Point { x, y }`. Had we use a different syntax for named parameters (such as Point(x = ..., y= ...)), it would have made the pattern-matching less convenient.

## Children

Defining a tree can be very verbose, because all the children have to be defined as separate classes. Skip adds a little bit of syntactic sugar to make the process more pleasant. It is possible for a base class to define a list of `children` without having to repeat the generics and the fact that those classes extends that particular base class.

```
base class Parent<T> {
  children = A() | B(Int) | C(Int, Bool)
}
```

This is strictly equivalent to:

```
base class Parent<T>
class A<T>() extends Parent<T>
class B<T>(Int) extends Parent<T>
class C<T>(Int, Bool) extends Parent<T>
```
