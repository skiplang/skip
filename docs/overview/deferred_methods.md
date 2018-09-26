---
id: deferred_methods
title: Deferred Methods
---

## Deferred methods

A deferred method is a method that is inherited by the children of the class, but that is not callable by other methods (unless they are themselves deferred). That can be very useful to define a default implementation, but when that implementation signatures is such that it cannot be made visible safely.

```
base class MyValue {value: this::TV} {
  type TV;

  deferred static fun make(value: TV): this {
    static { value }
  }
}
class Child extends MyValue {
  type TV = Int;
  // inherited
  // static fun make(value: Int): this
}
```

What's going on here? We have defined a static method `make` that is a generic constructor. That constructor is now a factory that knows how to build objects of type `MyValue`. The children of MyValue will inherit that method, and therefore won't have to redefine it themselves. So why is that method deferred? Because the signature of `make` is going to be incompatible with the ones defined in the subclasses.

```
_ = MyValue::make(0) // error: cannot call a deferred
_ = Child::make(0) // yay!
```


## Constraints on types

A very advance feature is to add a constraint on types defined in a class (in order to use that type within a deferred method).

```
base class MindBlown {
  type T: A;
  deferred static fun make(x: this::T): this {
    // x can be used as an "A"
  }
}
```
