---
title: Faster Option
author: Mat Hostetter
---

TL;DR: Skip's Option type is now implemented as a value class, reducing speed and space overhead.

## Problem: "Option is too slow"

The Skip programming language (formerly “Reflex”) does not have conventional null pointers (avoiding Tony Hoare's ["billion dollar mistake"](http://lambda-the-ultimate.org/node/3186). Instead, as with several other languages, possibly-absent values are represented using a simple `Option<T>` class that has subclasses `None()` and `Some(value: T)`.

The problem with this implementation is that calling Some(myValue) allocates a heap instance to "box" myValue. This can be expensive, especially given that the Iterator API is defined to return an Option each time through a loop. We were considering changing the Iterator API to avoid this overhead, but that idea was pretty unsatisfying.

## Solution: Avoid heap allocation

Rather than compromise standard library APIs, over the weekend I fixed the performance problem. The back end now compiles Option to a value class, a pass-by-value object that never allocates heap memory.

The user-visible API is unchanged. The front end compiles Option normally, as if it were a reference type, but the back end changes the representation to a value class and converts internal operations like `TypeSwitch` (normally a vtable-examining operation allowed only on reference types) to analogous operations on the value class form.

The Skip compiler uses two strategies to compile `Option<T>` as a value class, depending on `T`: *Flag* and *Sentinel*.

### Flag

The most general strategy is Flag. This compiles `Option<T>` as (effectively) a pair of `(Bool, Unsafe.RawStorage<T>)`. If the Bool is true, the Option is really `Some()`, and the RawStorage holds the value. If the Bool is false, the Option is `None()` and the RawStorage is unused (but filled with all zero bits to make the GC and interner happy).

Flag is efficient, but it adds an extra Bool field that must be passed around and stored. This is almost always better than heap allocating, but it can make an object containing an Option field larger than it was previously, when that Option was just a pointer.

### Sentinel

We can represent `Option<T>` using the same number of bits as `T`, if there is some "illegal" bit pattern for `T` that we can reserve for `None()`. The most obvious use case for this is `Option<SomeReferenceType>`. Here we just use a null pointer for `None()`, and the `SomeReferenceType` object pointer for `Some`. This is better in every way than the old Option implementation.

Sentinel is the preferred strategy, but can't always be used. Some types, like Int, have no "reserved" value, so `Option<Int>` must use Flag.

We can use Sentinel in some surprising cases. The tuple `(Int, Int, String)` is a value class, so we can’t use the null pointer trick for `Option<(Int, Int, String)>` in the usual way, but even so we don’t need to fall back to Flag. Instead, we use Sentinel, where the tuple `(*, *, null)` indicates `None()`. More generally, any value class that has a Sentinel-compatible field can use the Sentinel strategy by using the nullness of that field to indicate `None()` for the entire Option. Of course, for something like `Option<(Int, Int, Int)>` we still need to use Flag.

### Nested Options

What about `Option<Option<Option<String>>>`? We'd like to use Sentinel, but we can't just use a null pointer, because that would leave ambiguous `None()` vs. `Some(None())` vs. `Some(Some(None()))`. We could just give up and use Sentinel for the "inner" Option and Flag for the outer ones, but where’s the fun in that?

Null isn't the only reserved pointer value -- the Skip garbage collector ignores any pointer <= 0. The compiler exploits this by detecting the nested Option case and using null (0) for the innermost Option (that's always the strategy for `Option<String>`), and then -1 as the sentinel pointer value for `Option<Option<String>>`, -2 as the sentinel for `Option<Option<Option<String>>>`, and so on.

## Examples

Let's look at the assembly code produced for some simple functions. First, the Sentinel case:

```
fun stringSome(s: String): Option<String> {
  Some(s)
}

fun stringNone(): Option<String> {
  None()
}
```

compiles to:

```
sk.stringSome:
        movq    %rdi, %rax
        retq

sk.stringNone:
        xorl    %eax, %eax
        retq
```

And the Flag case:

```
fun intSome(n: Int): Option<Int> {
  Some(n)
}

fun intNone(): Option<Int> {
  None()
}
```

also compiles to some simple register manipulation:

```
sk.intSome:
        movb    $1, %al
        movq    %rdi, %rdx
        retq

sk.intNone:
        xorl    %eax, %eax
        xorl    %edx, %edx
        retq
```

As you can see, no heap allocations remain!

Now let’s try doing a pattern match:

```
fun stringDefault(s: Option<String>): String {
  s match {
  | Some(x) -> x
  | None() -> "[none]"
  }
}
```

This yields:

```
sk.stringDefault:
	testq	%rdi, %rdi
	movabsq	$-1152818814380970405, %rax # imm = 0xF0005D656E6F6E5B
	cmovneq	%rdi, %rax
	retq
```

 Not only does this code have no memory loads, it doesn’t even have any conditional branches.

## Conclusion

Skip Options are now fast enough that we can feel free to use them routinely, even in performance-sensitive code. Next up, I’d like to see us add some syntactic shorthand that makes them even easier to use.
