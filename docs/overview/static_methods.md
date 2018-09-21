---
id: static_methods
title: Static Methods
---

A class can define static methods:

```
class Math {
  static fun plus1(x: Int): Int {
    x + 1
  }
}
```

That can be called by using the `::` operator.

```
fun testMath(): Int {
  Math::plus1(0)
}
```

That's because `::` is an operator on values of type `Class` and that `Math` in this example refers to the object of type `Class<Math>` that contains all of its static methods.

## Late Static Binding

Skip allows to specialize static methods, in exactly the same way as with normal methods. When I write `this.foo()`, I am calling the most specialized method named `foo`. In other words, `this` refers to the type of the most precise object, the one that was used to instantiate the object.
The exact equivalent is made available for static methods with `static::foo()`, except that in this case `static` refers to the most precise class.
Concretely:

```
base class Parent {
  static fun callChild(): String {
    static::whoAmI()
  }
  static overridable fun whoAmI(): String {
    "Parent"
  }
}
class Child1() extends Parent {
  static fun whoAmI(): String {
    "Child1"
  }
}
class Child2() extends Parent {
  static fun whoAmI(): String {
    "Child2"
  }
}
```

A call to `Parent::callChild()` results in the string `Parent`. A call to `Child1::callChild()` results in the string `Child1` etc ...

## Late Static Constructor

Sometimes it can be useful to define a constructor method in a parent class. But when that is the case, we almost never want that method to return the parent class, what we want is to build an object of type `this`. `static`, which, remember, refers to the most specialized version of the class, comes in handy to do that.

```
base class Parent final { value: Int } {
  static fun make(value: Int): this {
    static { value }
  }
}
```

**Note**: *late static constructors can only be used when the constructor of the class has been made final (no fields can be added by the children).*
