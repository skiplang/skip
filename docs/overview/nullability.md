---
id: nullability
title: Nullability
---

## Option Type

The type option is defined as follow:

```
base class Option<T> {
  children = None()| Some(T)
}
```

That is useful when a value is sometimes present but not always, the way to de-sugar an Option value is by using pattern-matching. Also, because the type is used so often, there is a bit of syntax to avoid spelling out the word `Option` every time. `?Int` stands for `Option<Int>`. It is placed before (and not after) the type to make generics more readable: `MyGeneric<...>?` is less readable than `?MyGeneric<...>`, but of course that is debatable. 

```
fun zeroIfUndefined(value: Option<Int>): Int {
  value match {
  | None() -> 0
  | Some(x) -> x
  }
}
```
