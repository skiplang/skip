---
id: modules
title: Modules
---

A module is a namespace regrouping top-level definitions such as types, classes, constants and functions.

```
module Canvas;
type Point = (Int, Int);
fun move(Point, Int, Int): Point { ... }
module end;
```

The definitions in a module can be accessed with a `.` operator.

```
fun render(): Point {
  point = (0, 0);
  Canvas.move(point, 1, 1)
}
```

## Absolute paths

If a module defines a function that is also defined either at top-level, it is possible to refer to the top-level definition with an absolute path. Absolute paths start with a `.`.

```
module Canvas;
fun render(): Point {
  point = (0, 0);
  /* refers to the function move defined at top-level */
  .move(point, 1, 1);
}
module end;
```

## Module aliases

Sometimes, when a module is used very frequently in a file, or when the name of a module is simply too long, it can be handy to define a module alias.

```
module alias L = List;
/* Passed this point in the file, L.foo is equivalent to List.foo */
```

The scope of an alias is the file only, aliases are not visible outside of the files where they are defined.
