---
id: base_classes
title: Base Classes
---

```
base class Parent
class Child1() extends Parent
class Child2() extends Parent
```

A base class is a class that can be extended, but that cannot be instantiated. The reason for that distinction is that we want to encourage a style with *flat* class hierarchies. Ideally, a class could have many parents, but would not define a  *deep* class hierarchy. A base class is the moral equivalent of an interface that can provide default implementations.

Of course, it is still possible to create deep class hierarchies, and it's still possible to define a concrete class for each base class (in this case we could have create `class ConcreteParent() extends Parent`), it's just that the language doesn't encourage that.


## Multiple inheritance

```
base class Parent1 {
  overridable fun message(): String {
    "Hello from Parent1"
  }
}

base class Parent2 {
  overridable fun message(): String {
    "Hello from Parent2"
  }
}

class Child() extends Parent1, Parent2 {
  fun message from Parent1;
}
```

There will be cases where a class can inherit the same method from 2 different paths. When that is the case, the conflict must be resolved explicitly with the construction `from`. Note that methods are final by default, so we have to explicitly define them as overridable in the parents.
