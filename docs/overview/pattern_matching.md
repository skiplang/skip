---
id: pattern_matching
title: Pattern Matching
---

One of the key characteristics of Skip is that any class can be pattern-matched over. That's why the fields of a class look like they are parameters, and why they need to be defined right after the name of the class. It make pattern-matching feel like the `dual` operation of object construction.

```
fun valueToInt(value: Parent): Int {
  value match {
  | A() -> 0
  | B() -> 1
  | C() -> 2
  }
}
```

Pattern matching works on even more complicated structures. If you have a specific pattern of nested classes, you can match them with a single line. Using `_` just means to match everything else.

```
class A (x: ?Int)

class B (item: A)

fun match(b: B): Int {
  b match {
  | B(A(Box(x))) -> x
  | _ -> 0
  }
}
```

Sometimes you’ll want to pull out both the object fields and the object from the pattern match. Instead of doing multiple nested pattern matches, you can use the `@` syntax instead.

```
fun match(b: B): Int {
  b match {
  | B(a@A(Box(val))) -> a.getSize() * val
  | _ -> 0
  }
}
````

If you’re trying to pattern match on an object that uses [named parameters](http://skiplang.com:8080/docs/classes.html#named-parameters), just use the same syntax as the named constructor.

```
class A {x: ?Int}

fun match(a: A): Int {
  a match {
  | A{x => Box(i)} -> i
  | _ -> 0
  }
}
```

If the object you’re trying to match as private or protected fields, you won’t be able to access them. To pattern match, you’ll need to use `_` in the place of the fields you cannot access.

```
class A (private x: ?Int, y: ?Int)

fun getNonNullY(a: A): Int {
  a match {
  | A(_, Box(y))-> y
  | _ -> 0
  }
}
```

If the type structure and values aren't enough to get the branch you need, you can also use `if` expressions with your pattern matching. The branch will only match when the `if` expression evaluates to true.

```
class A(x: ?Int, y: ?Int)

fun getNonNegative(a: A): Int {
  a match {
  | A(_, Box(y)) if (y > 0) -> y
  | _ -> 0
  }
}
```

If you find yourself writing code that simply checks the structure of a type, like `x match {A(_, Box(_)) -> true | _ -> false}`, you can instead use `matches`. A `matches` expression evaluates as a `Bool` in the same way as a pattern in pattern matching. However, with `matches` you cannot pull out the values of the inner elements like you can with pattern matching.

```
class A(x: ?Int, y: ?Int)

fun match(a: A): Int {
  if (a matches A(_, Box(_))) {
    10
  } else {
    0
  }
}
```

If you have multiple patterns that should execute the same branch, you do not need to repeat the same code for every branch. For example,
```
class A(x: ?Int, y: ?Int)

fun getAnyValAndAdd100(a: A): Int {
  a match {
  | A(_, Box(val))
  | A(Box(val), _) -> val + 100
  | _ -> 100
  }
}
```
When the pattern `A(_, Box(val))` is matched, the execution will fall though to the next `->`. In this case it will execute the expression `val + 100`.
