---
id: variance
title: Variance
---

The question of variance can be summarized like this: if I have a `Vector<B>` when is it correct to use it as a `Vector<A>` (assuming B is subtype of A).

The answer to that question is surprisingly complicated. In fact, I don't expect to be able to do that in this document. The good news is that you don't have to know exactly how things work, you just have to follow what the compiler says.

## Covariance

If you want your objects of type `X<B>` to convert to `X<A>` automatically, you need to declare it as covariant (with a little + sign in front of the relevant type-parameter):

```
class MyCollection<+T>(value: T) {
  fun get(): T { this.value }
}
```

Note that the compiler might refuse the annotation if `T` appears is so-called `contra-variant` position within the definition of the class. As stated earlier, I won't go in the details of what that means here, just rework the API when the compiler is complaining.

## Contra-variance

Conversely, if you want an object of `X<A>` to convert to `X<B>` (the other way around), you need to annotate the type-parameter with a `-`. Note that contra-variance is less frequently used ...

## Invariance and mutability

By default, when no `+` or `-` sign is present, type-parameters are all considered invariant: their type must match exactly. What is also important to note is that mutable objects are always considered invariant (their type parameters are), regardless of the annotation: they only become co/contra-variant after they have been frozen.
