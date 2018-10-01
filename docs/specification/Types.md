# Types

## General

The meaning of a value is decided by its *type*.

**Syntax**

<pre>
  <i>type-specifier:</i>
    <i>class-type-specifier</i>
    <i>tuple-type-specifier</i>
    <i>lambda-type-specifier</i>
    <i>nullable-type-specifier</i>
    <i>reactive-type-specifier</i>
    <i>mutable-type-specifier</i>
    <i>generic-type-parameter-name</i>
    <i>global-type-constant-declaration</i>
    <i>parenthesized-type</i>
    void
    this
    inst
    <i>underscore-type</i>

  <i>parenthesized-type:</i>
    (   <i>type-specifier</i>   )

  <i>class-type-specifier:</i>
    <i>qualified-type-name   generic-type-argument-list<sub>opt</sub></i>

  <i>type-specifier-list: </i>
    <i>type-specifier</i>
    <i>type-specifier-list</i>   ,   <i>type-specifier</i>

  <i>type-specification:</i>
    :   <i>type-specifier</i>

  <i>qualified-type-name:</i>
    <i>type-identifier</i>
    .   <i>type-identifier</i>
    <i>type-identifier</i>   .   <i>type-identifier</i>
    <i>qualified-type-name</i>   ::   <i>type-identifier</i>
</pre>

**Defined elsewhere**

* [*generic-type-argument-list*](Generic-Types-Methods-and-Functions.md#type-arguments)
* [*generic-type-parameter-name*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*global-type-constant-declaration*](Types.md#type-constants)
* [`inst`](Types.md#the-inst-type)
* [*lambda-type-specifier*](Types.md#lambda-types)
* [*mutable-type-specifier*](Types.md#mutable-types)
* [*nullable-type-specifier*](Types.md#nullable-types)
* [`this`](Types.md#the-this-type)
* [*reactive-type-specifier*](Types.md#reactive-types)
* [*tuple-type-specifier*](Types.md#tuple-types)
* [*type-identifier*](Lexical-Structure.md#identifiers)
* [*underscore-type*](Types.md#the-underscore-type)

**Constraints**

If *qualified-type-name* consists solely of *type-identifier*, and *type-identifier* designates a [module](Modules.md#general), that module must contain a [class called `.`](Classes.md#class-declarations).

For the *qualified-type-name* form `.`*type-identifier*, the [global module](Modules.md#general) must contain a declaration for *type-identifier*.

For the *qualified-type-name* form *type-identifier*`.`*type-identifier*, the left-hand *type-identifier* must name a [module](Modules.md#general) that contains a declaration for the right-hand *type-identifier*.

**Semantics**

The *qualified-type-name* form `.`*type-identifier* designates the *type-identifier* in the [global module](Modules.md#general). The *qualified-type-name* form *type-identifier*`.`*type-identifier* designates the right-hand *type-identifier* in the module named the left-hand *type-identifier*.

If *qualified-type-name* consists solely of *type-identifier*, and *type-identifier* designates a module, *qualified-type-name* designates the class called `.` in that module; otherwise, *type-identifier* designates a class having that name.

## The Boolean Type

The Boolean type is [`Bool`](RTL-type-Bool), a predefined value class type. This type is capable of storing either of two distinct values, which correspond to the Boolean values True and False, respectively. The representation of this type and its values is unspecified.

`Bool` values are ordered; specifically, False < True.

## The Character Type

The character type is [`Char`](RTL-type-Char), a predefined value class type. This type is capable of storing a Unicode code point.

`Char` values are ordered.

## The Integer Type

There is one integer type, [`Int`](RTL-type-Int), a predefined value class type. This 64-bit type is binary, signed, and uses twos-complement representation for negative values. All bits are used to represent a value, which is finite.

The predefined constants `Int::min` and `Int::max` indicate the minimum and maximum values, respectively.

Certain operations on `Int` values can produce a mathematical result that cannot be represented as an `Int`. Specifically

* Applying the unary minus to the smallest value results in overflow, with no exception being thrown
* Dividing `Int::min` by `-1` results in overflow, with no exception being thrown
* Multiplying, adding, or subtracting two values can result in overflow, with no exception being thrown

`Int` values are ordered.

## The Floating-Point Type

There is one floating-point type, [`Float`](RTL-type-Float), a predefined value class type. This 64-bit type uses IEEE 754 double-precision representation.

The predefined constants `Float::inf` and `Float::nan` indicate positive infinity and Not-a-Number, respectively.

`Float` values are not ordered (because of the possibility of NaN).

## The String Type

The string type is [`String`](RTL-type-String), a predefined class type. This type is a sequence of zero or more Unicode characters U+0000 through U+10FFFF, excluding the surrogate halves U+D800 through U+DFFF.

Conceptually, a String can be considered as an array of `Char`s—the ***elements***—whose keys are the `Int` values starting at zero. The type of each element is `String`.

A String whose length is zero is an ***empty string***.

As to how the `Char`s in a `String` translate into Unicode code points is unspecified.

`String` values are ordered.

## The Void Type

Conceptually, the type `void` indicates the absence of a value, and is used primarily as the return type of a function. However, the type has exactly one value, `void`. The representation of this type and its value is unspecified.

An important use of this type and its value is as a marker/placeholder when using generic functions and classes. See [example](Lexical-Structure.md#void-literal).

Note: Even though one can use `void` in the general type sense, such as defining a local variable of type `void` and passing and/or returning a value of that type to and/or from a function, the use of `void` as a general-purpose *type-specifier* is discouraged, as it can lead to obfuscated code.

## Reactive Types

**Syntax**

<pre>
  <i>reactive-type-specifier:</i>
    ^   <i>type-specifier</i>
</pre>

**Defined elsewhere**

* [*type-specifier*](Types.md#general)

**Constraints**

*type-specifier* must not be *lambda-type-specifier*.

**Semantics**

The *reactive-type-specifier* `^`*T* is shorthand for [`Awaitable<`*T*`>`](RTL).

An instance of a reactive type is created by the compiler when an [async function](Functions.md#asynchronous-functions) returns normally.

**Examples**

```
async fun genString(): ^String {
  "Hello"
}
```

When the function terminates normally, the String `"Hello"` is wrapped in an instance of type `^String`.


## Mutable Types

**Syntax**

<pre>
  <i>mutable-type-specifier:</i>
    mutable   <i>type-specifier</i>
</pre>

**Defined elsewhere**

* [*type-specifier*](Types.md#general)

**Constraints**

*type-specifier* must not already contain `mutable`.

**Semantics**

In the absence of a `mutable` prefix, a *type-specifier* represents an immutable type. A *type-specifier* with a `mutable` prefix represents a mutable type. A mutable version of a *type-specifier* is a subtype of the corresponding immutable version.

**Examples**

```
class Foo(mutable value: Int) {
  mutable fun addOne(): void {	// `this` is mutable
    this.!value = this.value + 1;
    void
  }
  fun get(): Int {
    this.value
  }
}
fun addOneToMutablefoo(foo: mutable Foo): void {
  foo.addOne();
  void
}
fun main(): void {
  foo: mutable Foo = mutable Foo(1);
  foo.addOne();
  addOneToMutablefoo(foo);
  …
}
```

## Class Types and Subtypes

Class types are described in [§§](Classes.md#general).

When class type *T2* is derived directly or indirectly from class type *T1*, *T2* is said to be a ***subtype*** of *T1*, and *T1* is said to be a ***supertype*** of *T2*. A supertype can have one or more subtypes, and a subtype can have one or more supertypes. A supertype can be a subtype of some other supertype, and a subtype can be a supertype of some other subtype.

The relationship between a supertype and any of its subtypes involves the notion of substitutability. Specifically, if *T2* is a subtype of *T1*, program elements designed to operate on *T1* can also operate on *T2*.

For each type *T*, the type [`Null`](RTL-type-xxx) is a subtype of all nullable types `?`*T*.

A type is a subtype of itself.

One type is *type compatible* with another if it a subtype of that other type.

Every base and ordinary class type is a subtype of type [`this`](#the-this-type).

## Tuple Types

**Syntax**

<pre>
<i>tuple-type-specifier:</i>
  (   <i>type-specifier</i>   ,   <i>type-specifier-list</i>   )
</pre>

**Defined elsewhere**

* [*type-specifier*](#general)
* [*type-specifier-list*](#general)

**Semantics**

A ***tuple*** is a sequence of two or more elements, the number, type, and value of which are fixed at the time of [tuple creation](Expressions.md#tuple-creation).

Each element can have any type, and each unique, lexically ordered combination of element types designates a distinct tuple type.
The elements in a tuple can be accessed using a [tuple pattern](#tuple-pattern).

**Examples**

```
fun getTup(): (Int, String) {
  t: (Int, String) = (10, "Hi");
  t
}
// -----------------------------------------
t3: (Int, Float, String) = (-5, 3.6, "red");
t4 = (-5, 3.6, "red");	// has same type as t3
// -----------------------------------------
class C(private t: (Int, String)) { … }
c1 = C((22, "text"));
```

## Lambda Types

**Syntax**

<pre>
  <i>lambda-type-specifier:</i>
    <i>lambda-decl-parameter-list</i>   <i>lambda-arrow</i>   <i>return-type</i>
    <i>lambda-type-specifier</i>   <i>lambda-arrow</i>   <i>return-type</i>

  <i>lambda-arrow:</i>
    ~>
    ->

  <i>lambda-decl-parameter-list:</i>
    <i>pos-parameter</i>
    (   <i>pos-parameter-list<sub>opt</sub></i>   )
    (   <i>pos-parameter-list</i>   ,   )
</pre>

**Defined elsewhere**
* [*pos-parameter*](Functions.md#function-declarations)
* [*pos-parameter-list*](Functions.md#function-declarations)
* [*return-type*](Functions.md#function-declarations)

**Constraints**

An immutable form lambda can only capture immutable values.

A *pos-parameter* for a lambda declaration must not contain a *default-value*.

A mutable form lambda’s value cannot be stored in a field, nor can it be returned by a function.

**Semantics**

A ***lambda*** is an object that encapsulates a function with a given signature and return type. The function can be called through that object by using the [function-call operator](Expressions.md#function-call-operator).

The `~>` form of *lambda-arrow* results in an ***immutable lambda form*** of *lambda-type-specifier*, while the `->` form results in a ***mutable lambda form***.

Given two *lambda-type-specifier*s that are identical except that they each have a different *lambda-arrow* form, the mutable form type is a subtype of the immutable form type.

A mutable form lambda can capture either an immutable or a mutable value.

**Examples**

```
fun fl(p: () ~> String): String { p() }
lambda = () ~> "M1";
print_string(fl(lambda));               // outputs "M1"
// -----------------------------------------
fun fl(p: (Int) ~> String, val: Int): String { p(val) }
lambda = x ~> x.toString() + "XX";      // outputs "-8XX"
print_string(fl(lambda, -8));
// -----------------------------------------
fun fl(p: (String ~> String ~> String), s1: String, s2: String): String {
  p(s1)(s2)
}
lambda = x ~> (y ~> x + y);
print_string(fl(lambda, "He", "llo"));  // outputs "Hello"
// -----------------------------------------
fun doit(iValue: Int, process: Int ~> Int): Int {
  process(iValue)
}
result = doit(5, p ~> p * 2);           // doubles 5
result = doit(5, p ~> p * p);           // squares 5
// -----------------------------------------
lam: (mutable r: Ref<String>, t: String) -> void = (r, t) -> { r.value <- t };
mutable ref: Ref<String> = Ref("");
lam(ref, "NewValue");
// -----------------------------------------
class Foo { f: ( void ) ~> void } {
  fun call(): void {
    this.f()
  }
}
Foo { f => (() ~> {}) }.call();
// -----------------------------------------
fun move<T>(f: (mutable Ref<T>) ~> void, t: T): T {
  ref: mutable Ref<T> = mutable Ref(t);
  f(ref);
  ref.value
}

fun f(): void {
  s = move(ref ~> { ref.!value = "NewValue" }, "OldValue");
  void
}
```

Compare the following---almost identical---parameter types:

```
fun f2(p: (String ~> (String, Int, Int) ~> String)): String { … }
fun f3(p: (String ~>  String, Int, Int  ~> String)): String { … }
```

The first is a lambda taking one `String` argument and returning a lambda taking three arguments (of type `String`, `Int`, and `Int`, respectively) and returning a `String`. The second is a tuple having three elements: a lambda taking and returning a `String`, an `Int`, and a lambda taking an `Int` and returning a `String`.

## The this Type

**Constraints**

This type can only be used in a [*method-return-type*](Classes.md#methods).

**Semantics**

The type name `this` refers to "the current class type". (This use of `this` as a type should not be confused with the use of [`this` as an expression](Expressions.md#primary-expressions).)

`this` can be thought of as an implicit type constant in any class hierarchy.

**Examples**

```
native class String … {
  static fun empty(): this { "" }
}
// ----------------------------------------------
class Dequeue<T> … {
  fun pushBack(x: T): this { … }
  fun popFront(): (this, T) { … }
  …
}
// ----------------------------------------------
class IntKey(value: Int) extends ArrayKey {
  …
  type TIntishCast = IntKey;
  fun asIntKey(): IntKey { this }
  fun intishCast(): this::TIntishCast { this.asIntKey() }
}
```

## The inst Type

**Constraints**

The type name `inst` can only appear inside a [trait declaration](Classes.md#class-declaration).

**Semantics**

`inst` is the type of the class that uses the current trait.

`inst` can be thought of as an implicit type parameter on any trait.

**Examples**

```
trait Number extends Hashable, Show, Orderable {
  fun +(inst): inst;
  fun -(inst): inst;
  fun fromInt(x: Int): inst;
  fun fromFloat(x: Float): inst;
  …
}
// When used by Float, inst takes on the type Float
native value class Float uses Number {
  native fun +(Float): Float;
  native fun -(Float): Float;
  fun fromInt(x: Int): Float { … }
  fun fromFloat(x: Float): Float { … }
  …
}
// When used by Int, inst takes on the type Int
native value class Int uses Number, … {
  native fun +(Int): Int;
  native fun -(Int): Int;
  fun fromInt(x: Int): Int { … }
  fun fromFloat(x: Float): Int { … }
  …
}
```

## Nullable Types

**Syntax**

<pre>
  <i>nullable-type-specifier:</i>
    ?   <i>type-specifier</i>
</pre>

**Defined elsewhere**

* [*type-specifier*](#general)

**Constraints**

*type-specifier* must not be *nullable-type-specifier* or *lambda-type-specifier*.

**Semantics**

A ***nullable*** type can represent all of the values of its ***underlying type***, plus an additional null value, written as `Null()`. A nullable type is written `?`*T*, where *T* is the underlying type. For example, a variable of type `?Bool` can contain one of the values True, False, and null.

***Boxing*** is the process of making a value of type `?`*T* from a value of type *T*. This is achieved via the constructor for class [`Box`](RTL-type-xxx). Specifically, `Box(`*value-of-type-T*`)` constructs the corresponding value of type `?`*T*.

***Unboxing*** is the process of extracting a value of type *T* from a value of type `?`*T*, provided the nullable value is not null. This is achieved via [pattern matching](Expressions.md#patterns-and-pattern-matching), as follows:

```
nullable_type_value match {
    | Box(n) -> …
    | Null() -> …
  }
```

A null type is a specialization of the generic type [`Nullable`](Types.md#nullable-types).

**Examples**

```
class C {private f1: ?Bool = Null(), private f2: ?Bool = Box(false)} {
  fun getF1(): ?Bool {
    this.f1
  }
  fun getF2(): ?Bool {
    this.f2
  }
}
// -----------------------------------------
fun f4(p1: ?Int, p2: ?Float, p3: ?String, p4: ?Char, p5: ?C): void { … }
// -----------------------------------------
v1: ?Int = Box(1234);
print_string("Int boxed inside v1 is " + v1.getx().toString());
```

## Generic Types

Skip contains a mechanism to define generic (that is, type-less) classes and functions, and to create type-specific instances of them via parameters. See [§§](Generic-Types-Methods-and-Functions.md).

## The Underscore Type

**Syntax**

<pre>
  <i>underscore-type:</i>
    _
</pre>

**Constraints**

This type can only be used as the [type of a function parameter or return value](Functions.md#function-declarations), or as a [*generic-type-parameter*](Generic-Types-Methods-and-Functions.md#type-parameters) in the [*type-specifier*](#general) of a function parameter or return value.

**Semantics**

This type is for notation convenience.

The presence of the type `_` in a *function-declaration-header* makes that function generic (if it isn’t already). Each type `_` in that *function-declaration-header* represents a placeholder for a distinct type parameter for that function. The set of placeholders becomes the set of type parameters for that function, in lexical order, after any explicit type parameters that function already has. 

**Examples**

```
fun f1(x: _, y: Int): _ { … }
```

is shorthand notation for the following:

```
fun f1<T1, T2>(x: T1, y: Int): T2 { … }
```

and

```
fun f2<T1, T2>(x: T1, y: _, z: T2): void { … }
```

is shorthand notation for the following:

```
fun f2<T1, T2, T3>(x: T1, y: T3, z: T2): void { … }
```

## Type Inferencing

Ordinarily, a program entity having a type must have that type specified explicitly. One exception is local variables (but not parameters). Consider the following:

```
fun f(): void {
  v1: Int = 10;   // #1
  v2 = 10;        // #2
  …
}
```

In Line #1, `v1` is explicitly typed as being `Int`, whereas, in Line #2, `v2`’s type is inferred from its initializer.
Types are also inferred when calling generic methods and functions. See [§§](Generic-Types-Methods-and-Functions.md#general).

## Type Constants

There are two kinds of type constants:

* global type constants, which are discussed below
* [class type constants](Classes.md#type-constants)

**Syntax**

<pre>
  <i>global-type-constant-declaration:</i>
    type   .<i><sub>opt</sub></i>   <i>type-identifier</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>type-initializer</i>   ;
  <i>type-initializer:</i>
    =   <i>type-specifier</i>
</pre>

**Defined elsewhere**

* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*type-identifier*](Lexical-Structure.md#identifiers)
* [*type-specifier*](#general)

**Constraints**

A *global-type-constant-declaration* whose *type-identifier* has a `.` prefix must be public and must be declared inside a named module.

*type-specifier* must not be an abstract class type constant.

**Semantics**

*global-type-constant-declaration* creates an alias, *type-identifier*, for the type specified by *type-specifier*. Once such a type constant has been defined, that alias can be used in any context in which *type-specifier* is permitted.

A *global-type-constant-declaration* whose *type-identifier* has a `.` prefix is promoted to the global module.

A *global-type-constant-declaration* has public accessibility.

Note: A global type constant cannot be native.

**Examples**

```
type TC1 = Int;
type IntDict<V> = Dict<Int, V>;
type StrDict<V> = Dict<String, V>;
type KeyDict<V> = Dict<ArrayKey, V>;
```
