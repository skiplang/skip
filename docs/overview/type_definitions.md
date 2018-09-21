---
id: type_definitions
title: Type Definitions
---

Sometimes it can be useful to define a name to refer to a longer type (when that type is long or needs to be referred to often).

```
type MyTriplet = (Int, Int, Int);
fun makeTriplet(): MyTriplet { (0, 0, 0) }
```

Things become more interesting within a class. A class can define a type, and later redefine it in on of it's children.

```
base class MyUser{name: String, age: Int} {
  type ID;
}

class IntKeyedUser{} {
  type ID = Int;
  static fun load(key: this::ID): this {
    ...
  }
}
class StringKeyedUser{} {
  type ID = String;
}
```

In this example, we have defined an interface of Users with a method `load` that references the most specialized instance of the type `ID` defined by the hierarchy.
But why bother? After all, we could have defined the function load with the type `Int` directly and call it a day right? Well, that feature becomes particularly interesting when coupled with `deferred` methods.
