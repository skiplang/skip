---
id: closures
title: Closures
---

There are 2 types of closures in skip:

1. The flat arrow ones `->`, they can capture any value (mutable or not) but they cannot be stored in an immutable object. Or at least, once they have been placed in an immutable object, they cannot be used anymore.
2. The curly ones `~>`, they can only capture immutable values, but you can do whatever you want with them (store them anywhere).

Note: *a curly closure can be used in place of a flat one. More specifically, `~>` is subtype of `->`.*

```
fun sumVector(v: Vector<Int>): Int {
  sum = 0;
  v.each(x -> !sum = sum + x);
  sum
}
```
