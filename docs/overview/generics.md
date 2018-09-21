---
id: generics
title: Generics
---

```
fun identity<T>(x: T): T { x }
class MyReference<T>(T)
```

It can sometime be useful to define functions or classes that just work with any type. That's what generics are for. One interesting thing about Skip generics is that they are inferred at the call site, you don't need to write the type arguments explicitly.

```
fun testingGenerics(): void {
  // All of the following is valid
  _ = identity(0);
  _ = identity<Int>(0);
  _ = MyReference(0);
  _ = MyReference<Int>(0);
}
```

## Constraints on generics

It is possible to add constraints on generics. The constraint `T: X` should be understood as: any type, as long as it implements X.

```
fun sum<T: IntConvertible>(v: Vector<T>): ...
```

A constraint can also be conditionally added to a method. When that's the case, the method will only be made available to the types that satisfy that constraint.

```
class MyReference<T>(value: T) {
  fun show[T: Showable](): String {
     this.value.show()
  }
}
```

In this case, the method `show` will only be made available when the type passed to MyReference implements `Show`. Calling the method `show` when the type of the generic doesn't satisfy the constraint will result in an error.
