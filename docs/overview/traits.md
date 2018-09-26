---
id: traits
title: Traits
---

```
trait Showable {
  fun toString(): String;
}
```


Skip defines a special kind of base classes called `traits`. They are different from a base class in the sense that they don't define a type. It is, for example, incorrect to define a field of type Showable.

So what are they used for? Traits can only be used to constrain a generic! Why that restriction? Because it allows us to define more expressive traits.

But first, let's review the problem with base classes. The classic problem arises when you need to define an interface that talks about itself.

```
base class MyComparable {
  overridable fun isEqual(this): Bool;
}
```

That base class is illegal in Skip, as in most object oriented languages. The reason is that `this` appears in so-called `contra-variant` position while its type is covariant. In less cryptic jargon, it means that you can override that function in a way that breaks the type system. Let's do it together!

```
class Child1(value: Int) extends MyComparable {
  fun isEqual(y: Child1): Bool { ... }
}
class Child2(value: Bool) extends MyComparable {
  fun isEqual(y: Child2): Bool { ... }
}
```

Now what's the problem with that? Problems are going to arise when two Comparables are used at the same time, but refer to different implementations. Concretely:

```
fun tough(x: MyComparable, y: MyComparable): Bool {
  x.isEqual(y)
}
```

You can see the problem here. If you were to call `tough(Child1(0), Child2(true))` the system would explode. You would be passing an object of type `Child2` to a method that expects a `Child1` and bad things are going to happen.

So what's the solution? The solution is to create a special type, that can only be used to constrain a generic: a trait! Let's revisit the code:

```
fun tough<T: MyComparable>(x: T, y: T): Bool {
  x.isEqual(y)
}
```

Now this works. Because a call to `tough` is guaranteed to have consistent arguments. And that's the essence of a trait! The key difference between a trait and base class is that all the instances of `this` in the interface are guaranteed to refer to the same type, which makes the definition of interfaces like `MyComparable` possible.
