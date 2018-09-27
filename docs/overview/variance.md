---
id: variance
title: Variance
---

The question of variance can be summarized like this: if we have a `Vector<B>`, when is it correct to use it as a `Vector<A>` (assuming `B` is subtype of `A`)?

The complete answer to that question is surprisingly complicated, beyond the scope of this document. The good news is that we don't have to know exactly how things work, we just have to follow what the compiler says.

## Covariance

If we want our objects of type `X<B>` to convert to `X<A>` automatically, we need to declare it as covariant (with a little `+` sign in front of the relevant type parameter):

```
class MyCollection<+T>(value: T) {
  fun get(): T { this.value }
}
```

Note that the compiler might refuse the annotation if `T` appears in so-called *contra-variant* position within the definition of the class. As stated earlier, we won't go in the details of what that means here; just rework the API when the compiler is complaining.

## Contra-variance

Conversely, if we want an object of `X<A>` to convert to `X<B>` (the other way around), we need to annotate the type-parameter with a `-`. Note that contra-variance is less frequently used ...

## Invariance and mutability

By default, when no `+` or `-` sign is present, type parameters are all considered invariant: their type must match exactly. What is also important to note is that mutable objects' type parameters are always considered invariant, regardless of the annotation: they only become co/contra-variant after they have been frozen.
