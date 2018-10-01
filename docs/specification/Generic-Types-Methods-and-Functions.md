# Generic Types, Methods, and Functions

## General

Classes, their methods, and functions can be ***parameterized***; that is, their declarations can have one or more placeholder names—called ***type parameters***—that are associated with types via ***type arguments*** when a class is instantiated, or a method or function is called. A type, method, or function having such placeholder names is called a *generic type*, a *generic method*, or a *generic function*, respectively.

Generics allow programmers to write a class, method, or function with the ability to be parameterized to any set of types, all while preserving type safety.

Consider the following generic class type from the standard library, and its use in creating two local variables:

```
class Ref<+T>(mutable value: T)

r1: mutable Ref<Int> = mutable Ref(0);
r2: mutable Ref<String> = mutable Ref("red");
```

The class has one type parameter, `T`, which is used once in the class declaration, as the type of the only field, `value`. For `r1`, a type argument of `Int` is used, and the constructor is given an `Int` value. Similarly, for `r2`, a type argument of `String` is used, and the constructor is given a `String` value.

Consider the generic function [`fst`](RTL-fun-fst) from the standard library, and its use:

```
fun fst<T1, T2>(p: (T1, T2)): T1 {
  (x, _) = p; x
}

t = (10, "green");
v = fst(t);
```

The function has two type parameters, `T1` and `T2`, which are both used as the function’s parameter types, while the first is also used as the return type. The function extracts the first element from a 2-element tuple. The type arguments of `Int` and `String` are inferred from the tuple argument `t` when it is passed to `fst`.

A call to a generic class’s constructor, or to a generic function or method can have explicit type arguments. For example, the calls to the constructors above can be written, as follows:

```
r1 = mutable Ref<Int>(0);
r2 = mutable Ref<String>("red");
```

The call to `fst` above, can be written, as follows:

```
v = fst<Int, String>(t);
```

Consider the following generic function from the standard library:

```
fun min<T: Orderable>(x: T, y: T): T {
  if (x < y) x else y
}
```

In the case of the type parameter `T`, its corresponding type argument is constrained to having a type that uses the trait `Orderable`. That is, `Orderable` is a ***type constraint***.

The *arity* of a generic type, method, or function is the number of type parameters declared for that type, method, or function. As such, class `Ref` and function `min` have arity 1, and function `fst` has arity 2.

Type parameters and type constraints are discussed further in [§§](Generic-Types-Methods-and-Functions.md#type-parameters), and type arguments are discussed further in [§§](Generic-Types-Methods-and-Functions.md#type-arguments).

## Type Parameters

**Syntax**

<pre>
  <i>generic-type-parameter-list:</i>
    &lt;   <i>generic-type-parameters</i>   &gt;

  <i>generic-type-parameters:</i>
    <i>generic-type-parameter</i>
    <i>generic-type-parameters</i>   ,   <i>generic-type-parameter</i>

  <i>generic-type-parameter:</i>
    _
    <i>generic-type-parameter-variance<sub>opt</sub></i>   <i>generic-type-parameter-name</i>   <i>type-constraint<sub>opt</sub></i>

  <i>generic-type-parameter-name:</i>
    <i>type-identifier</i>

  <i>generic-type-parameter-variance:</i>
    +
    -

  <i>type-constraint:</i>
    :   <i>type-constraint-conj</i>

  <i>type-constraint-conj:</i>
    :   <i>type-constraint-id</i>
    :   <i>type-constraint-id</i>   &   <i>type-constraint-conj</i>

  <i>type-constraint-id:</i>
    nonNullable
    <i>type-identifier</i>
</pre>

**Defined elsewhere**

* [*type-identifier*](Lexical-Structure.md#identifiers)

**Constraints**

The *generic-type-parameter-name*s within a *generic-type-parameter-list* must be distinct.

Each *type-identifier* in *type-constraint* must designate a [trait](Classes.md#class-declarations).

**Semantics**

A type parameter is a placeholder for a type that is supplied when the generic type is instantiated or the generic method or function is invoked.

A type parameter is a compile-time construct. At run-time, each type parameter is matched to a run-time type that was specified by a type argument. Therefore, a type declared with a type parameter will, at run-time, be a closed generic type ([§§](Generic-Types-Methods-and-Functions.md#open-and-closed-generic-types)), and execution involving a type parameter uses the actual type that was supplied as the type argument for that type parameter.

The *type-identifier* of a type parameter is visible from its point of definition through the end of the type, method, or function declaration on which it is defined.

*generic-type-parameter-variance* indicates the variance for that parameter: `+` for covariance, `-` for contravariance. If *generic-type-parameter-variance* is omitted, invariance is assumed.

A *type-constraint* requires any corresponding type argument to have the type, or be a subtype, of the trait designated by *type-identifier*.

The conditional keyword `nonNullable` designates a trait that is built into the compiler. Every non-nullable type behaves as if it uses `nonNullable`. Trait `nonNullable` cannot be extended.

Multiple constraints can be specified by combining them using `&`.

A *generic-type-parameter* of the form `_` is described in [§§](Types.md#the-underscore-type).

**Examples**

```
base class Nullable<+T: nonNullable> {
  fun getx(): T
  | Box(value) -> value
  | Null() -> invariant_violation("getx on Null")
}
class Box<+T: nonNullable>(value: T) extends Nullable<T> {
  fun get(): T {
    this.value
  }
}
// -----------------------------------------
class Array<Tk: ArrayKey, Tv>(private x: Dict<Tk, Tv>) {
  fun fromDict(x: Dict<Tk, Tv>): Array<ArrayKey, Tv> {
    Array(this.x.foldlWithKey(
      (k, v, accum) ~> accum.set(k.intishCast(), v),
      Dict::empty(),
    ))
  }
  fun toDict(): Dict<Tk, Tv> {
    this.x
  }
  fun toVec(): Vec<Tv> {
    this.x.toVec()
  }
}
// -----------------------------------------
fun print_eq<T: Equality & Show>(x: T, y: T): void { … }
```

## Type Arguments

**Syntax**

<pre>
<i>generic-type-argument-list:</i>
  &lt;   <i>generic-type-arguments</i>   &gt;

<i>generic-type-arguments:</i>
  <i>generic-type-argument</i>
  <i>generic-type-arguments</i>   ,   <i>generic-type-argument</i>

<i>generic-type-argument:</i>
  <i>type-specifier</i>
  <i>qualified-type-name</i>

</pre>

**Defined elsewhere**

* [*qualified-type-name*](Lexical-Structure.md#identifiers)
* [*type-specifier*](Types.md#general)

**Constraints**

Each *generic-type-argument* must satisfy any [constraint](Generic-Types-Methods-and-Functions.md#type-parameters) on the corresponding type parameter.

**Semantics**

At runtime, a *generic-type-argument* is used in place of the corresponding type parameter. A *generic-type-argument* can either be [open or closed](Generic-Types-Methods-and-Functions.md#open-and-closed-generic-types).

**Examples**

```
class Ref<+T>(mutable value: T)

r1: mutable Ref<Int> = mutable Ref(0);
r2: mutable Ref<String> = mutable Ref("red");
// -----------------------------------------
class Array<Tk: ArrayKey, Tv>(private x: Dict<Tk, Tv>) {
  fun fromDict(x: Dict<Tk, Tv>): Array<ArrayKey, Tv> {
    Array(this.x.foldlWithKey(
      (k, v, accum) ~> accum.set(k.intishCast(), v),
      Dict::empty(),
    ))
  }
  fun toDict(): Dict<Tk, Tv> {
    this.x
  }
  fun toVec(): Vec<Tv> {
    this.x.toVec()
  }
}
```

## Open and Closed Generic Types

A type parameter is introduced in the corresponding class, method, or function declaration. All other uses of that type parameter occur in [*type-specifier*s](Types.md#general) for the declaration of fields, function parameters, function returns, local variables, and so on. Each such use can be classified as follows: An *open generic type* is a generic type that contains one or more type parameters; a *closed generic type* is a generic type that is not an open generic type.

At run-time, all of the code within a generic class, method, or function declaration is executed in the context of the closed generic type that was created by applying type arguments to that generic declaration. Each type parameter within the generic class, method, or function is associated with a particular run-time type. The run-time processing of all expressions always occurs with closed generic types, and open generic types occur only during compile-time processing.

Two closed generic types are the same type if they are created from the same generic type declaration, and their corresponding type arguments have the same type.
