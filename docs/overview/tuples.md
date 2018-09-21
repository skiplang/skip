---
id: tuples
title: Tuples
---

Tuples are syntactic sugar for value classes.

```
my_pair = (1, 2);
```

Is equivalent to:

```
my_pair = Tuple2(1, 2);
```

With Tuple2 defined as follow:

```
value class Tuple2<T1, T2>(T1, T2)
```
