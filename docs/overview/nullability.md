---
id: nullability
title: Nullability
---

## Option Type

The type option is defined as follows:

```
base class Option<T> {
  children = None()| Some(T)
}
```

That is useful when a value is sometimes present but not always. The way to de-sugar an `Option` value is by using pattern matching. Also, because the type is used so often, there is a bit of syntax to avoid spelling out the word `Option` every time. `?Int` stands for `Option<Int>`. It is placed before (and not after) the type to keep the `?` readily visible even with generics : `MyGeneric<...>?` is arguably less readable than `?MyGeneric<...>`.

```
fun zeroIfUndefined(value: Option<Int>): Int {
  value match {
  | None() -> 0
  | Some(x) -> x
  }
}
```
