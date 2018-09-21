---
id: extension_classes
title: Extension Classes
---

A common problem with traits (and with base classes), is that they are sometimes defined `after the fact`. In other words, we want to add methods to an existing class to satisfy a constraint but we don't want to make those methods part of the original definition of the class. For example, let's say that I want to define a hashing function:

```
trait Hashable {
  fun hash(): Int;
}
```

The problem with that type is that it won't work with Ints, Strings or any other primitve type ... Sure, we could add the definition to Integers, but do we really want to do that? Or we could Box the integer ... But that's going to be a lot of overhead ... That's what extension classes are for:

```
extension class String uses Hashable {
  fun hash(): Int {
    ...
  }
}
```

There is a problem with extension classes, and because of that, they should be used very carefully: two different libraries could add a method with the same name to a class. When that happens, the code won't compile, because the compiler won't know which one to pick. Exactly like it's not a good idea for a library to add a bunch of names in the global namespace, it is not a good idea to add too many extension classes.

Of course, we could have made the type-system `pick` the right implementation depending on the context, but we found that solution difficult to follow for the reader of a program. This is a judgement call and could be debated either way.
