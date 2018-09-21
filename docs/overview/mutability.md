---
id: mutability
title: Mutability
---

```
class Point(mutable x: Int, mutable y: Int) {
  mutable fun moveX(): void {
    this.!x += 1;
    void
  }
}
```

Mutability at function boundaries is not forbidden, just discouraged. However, there will be cases where it is necessary. For example, when defining a mutable data-structure. Mutable fields must be explicitly annotated with the keyword `mutable`. Note that this only means the field can be mutated in the mutable version of the object.

Concretely, if I define a new point by writing `Point(1, 2)`, that point is immutable, and therefore the mutable methods defined on Point are not accessible (nor can any field be mutated). To define a mutable point, one must create it explicitly by writing: `mutable Point(1, 2)`. The same is true for its type. A mutable point is of type `mutable Point` which different from a `Point` (and their types are incompatible).

## Readonly

So by now, the story is pretty simple. When an object is immutable, you can access its immutable methods, when the object is mutable, you can access both its mutable and its immutable methods.

But is that really safe? It turns out it's not! Let's see with an example:

```
class Nasty(mutable x: Int) {
  fun capture_this(): Nasty { this }
}

fun nasty_test(): void {
  mnasty = mutable Nasty(0);
  immutable_nasty = mnasty.capture_this();
  mnasty.!x = 0; // changes immutable_nasty!!!
}
```

What's happening here is that we first create a mutable object, and then call an immutable function returning `this`. If in turn that function captures a pointer to `this` in its immutable form, we have a problem! We managed to build an immutable object when a mutable reference that could change the object under our feet is still out there (as shown in the function `nasty_test`). Of course this is wrong given that the golden rule of the language is that immutable objects are proven to never change ...

Luckily that code doesn't compile: because the type of `this` in the method `capture_this` is not `Nasty` but `readonly Nasty`.
`readonly X` should be read as: I don't know if `X` is mutable or immutable, but I only intend to read things out of it. In other words, readonly should be used when you want code to work with both mutable and immutable objects (like an immutable methods in a mutable class).

If you try to compile the code, you will see that the method `capture_this` does not compile, because the type of `this` is `readonly Nasty`.

## Frozen Methods

It is possible to define an `immutable method` within a mutable object by using the keyword `frozen`. It's a way to tell the compiler that you do not want that function to be callable when the object is mutable, you only want to have that method in the immutable case. This is useful when you need the object to actually refer to the immutable version of `this`. In the example above, you could make the method capture_this annotated with `frozen`. The compiler would then no longer complain on the return type of the method, but in the code that is attempting to call that method on a mutable version of the object.

## Freezing

Freezing is a builtin function that converts a mutable object into its immutable version.

```
fun use_mutable(): Point {
  obj = mutable Point(22);
  …
  obj.moveX();
  freeze(obj)
}
```

The way you are encouraged to use mutable objects:

1. Create a mutable object
2. Mutate locally (within the scope of a function)
3. Freeze the object to cross function boundaries (either return, or pass to another function)

Freezing will create an immutable clone of your mutable object, by recursively copying all its mutable fields. Note that there are several cases where that copy will be optimized away by the compiler, when it can prove that the mutable version does not escape. Once the object has been frozen, there is no way to go back to its mutable version.
