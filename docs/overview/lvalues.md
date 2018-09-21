---
id: lvalues
title: Lvalues
---

In Skip, an `!` designates which part of the lvalue is going to be modified. That notion is pretty unconventional, so let's just run through a few examples:

```
x.!field1 = 0
```
That last example works exactly like in most programming languages, the object that the variable `x` points to is going to be modified, the slot that corresponds to `field1` is going to be assigned the value `0`. Said differently, if `x` was pointing to `MyObject{ field1 => 42 }` it will point to the object `MyObject{ field1 => 0 }` after this expression is evaluated, and the modification will happen by physically updating the field `field1`. 

```
!x.field1 = 0
```

In this case, the object that the variable `x` points to is going to be left unmodified. Instead, the variable `x` is going to be updated to point to a new copy of the object pointed to by `x` where `field1` has the value `0`. Said differently, if `x` was pointing to `MyObject{ field1 => 42 }` it will still point to an object of type `MyObject{ field1 => 0 }` after the expression is evaluated (like in the previous example), but by physically updating the local `x` this time. This notation is a bit disturbing at first, but you will learn to love it when manipulating nested immutable values, which is very common in Skip.

Let's take a few more examples.

```
x.!field1.field2 = 0
```

That is a mix of the two behaviors, x.field1 is going to be physically modified to point to a copy of the object that was there, with the field `field2` set to the value `0`.

```
(!this, y) = this.f(...);
```

`=` is both used for assignments and variable introduction for a reason. It can be useful for an lvalue to both introduce new variables and modify existing ones. For example, when dealing with immutable objects, it will be often useful to update the variable `this` to point to a new version of the object.
