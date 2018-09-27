---
id: inst
title: The type inst
---

Sometime, `this` is a little bit *too* precise. The type `this` always refers to the most precise type. The problem with that is that `this` is often, but not always, what we want.
Let's imagine you are building an algorithm that mixes integers and floats, and you want to be able to compare them. You would create types that would look like:

```
base class Num uses MyComparable
class NumInt(value: Int) extends Num
class NumFloat(value: Float) extends Num
```

Now what does the signature of isEqual look like?

```
class NumInt(value: Int) {
  fun isEqual(value2: NumInt): Bool { ... }
}
```

But is that really the type we want? We want to be able to compare numeric values, so we want `isEqual` to work with objects of type `Num` and not `NumInt`. The type `this` in the trait is too precise!

That's what the type `inst` is for. `inst` should be read as: the type of the object that used the trait — in this case the type `Num`.

```
class MyComparable {
  fun isEqual(inst): Bool;
}
```

And that's the correct definition. Note that it is possible to reuse the same trait twice, so if an ancestor already was using a trait, it's possible to make that instantiation more precise. An extreme version of that would be to systematically reuse a trait in all the subclasses: that would be strictly equivalent to the type `this`.
