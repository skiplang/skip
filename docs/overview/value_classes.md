---
id: value_classes
title: Value Classes
---

A value class is a class that has a *flat* representation in memory. It is not possible to observe at run time that a class is a value class vs a normal one. But it can drastically impact the performance characteristics of a program.

```
value class Pair(x: Int, y: Int)
```

When creating a `Vector<Pair>`, both the fields of the object are going to be flattened. The upside is that it saves an allocation and that it makes accessing the field of an element in the `Vector` faster (it avoids an extra indirection). The downside is that moving a `Pair` out of the `Vector` requires a copy.

**Note:** *value classes cannot define mutable fields, which is why the difference with a normal class cannot be observed at runtime.*

**Note2:** *value classes cannot extend a a base class.*
