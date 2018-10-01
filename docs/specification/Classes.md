# Classes

## General

A ***class*** is a type that contains zero or more data and/or function *members*. An ***object*** (often called an ***instance***) of a class type is created (i.e., ***instantiated*** or ***constructed***) via the [function-call operator](Expressions.md#function-call-operator).

A ***trait*** is a class-like type used to support class types, and used with generics.

Objects are the key data-structure in Skip. By default, they are immutable.

Skip supports [***inheritance***](Classes.md#class-declarations), a means by which a *derived class* can *extend* (i.e., *inherit* from) and specialize one or more *base class*es. (Note: Unlike some languages, classes in Skip are **not** all derived from a common ancestor.) A class may *use* one or more [*traits*](Classes.md#class-declarations), each of which can contribute data and/or function members to that class.

An [***abstract*** class](Classes.md#class-declarations) is a class type intended for future derivation or implementation, but which cannot be instantiated directly. A class is made abstract by declaring one or more of its members to be abstract. A ***concrete*** class is a class that is not abstract. When a concrete class is derived from an abstract class, the concrete class must include an implementation for each of the abstract members it inherits. A trait can also be abstract or concrete.

A [***constructor***](Classes.md#constructors) is a special method that is used to initialize an object immediately after it has been created.

The non-native members of a base class can be ***overridden*** in a derived class by re-declaring them with the same signature as defined in the base class. However, with respect to its parameter list, an overriding constructor cannot have the same signature as those from its base classes; instead, it augments those by optionally providing new parameters. (See [constructors](Classes.md#constructors).) Unless declared `final`, a constructor is implicitly overridable. A non-constructor class member not having the modifier `overridable` is ***final***. Final members cannot be overridden. Trait members are implicitly overridable and cannot be made otherwise.

The layout of an object is unspecified.

It is useful to think about a class hierarchy as having [subtypes and supertypes](Types.md#class-types-and-subtypes).

By design, Skip does not support the OO idiom in which a subclass is created later on to support a use case that wasn't known when the class hierarchy was originally designed.

Note: Some languages (notably those supporting a single-inheritance model only) support the notion of an *interface*. They allow a derived class to both extend a single base class and to implement one or more interfaces. In this model, an interface is considered a type, and with one interface being able to extend another, we have the notion of a hierarchy with sub-interface and super-interface types. Skip supports interface-like behavior through multiple inheritance. It is best to think of a trait as having its contents embedded directly into a using class. Most importantly, **on its own, a trait is not a type**, so there can be no instances of traits, no values having some trait type, no trait type hierarchy, and no super- or sub-traits. A trait simply contributes to an ordinary class that can be instantiated.

## Class Declarations

**Syntax**

<pre>
  <i>class-declaration:</i>
    <i>class-modifiers<sub>opt</sub></i>   class   .<i><sub>opt</sub></i>   <i>class-name</i>
      <i>generic-type-parameter-list<sub>opt</sub></i>   <i>constructor-declaration<sub>opt</sub></i>
      <i>extends-and-or-uses-clause<sub>opt</sub></i>   <i>class-body<sub>opt</sub></i>

  <i>class-modifiers:</i>
    <i>class-modifier</i>
    <i>class-modifiers</i>   <i>class-modifier</i>

  <i>class-modifier:</i>
    base
    native
    private
    value

  <i>class-name:</i>
    <i>type-identifier</i>
    .

  <i>extends-and-or-uses-clause:</i>
    <i>extends-clause</i>
    <i>uses-clause</i>
    <i>extends-clause</i>   <i>uses-clause</i>
    <i>uses-clause</i>   <i>extends-clause</i>

  <i>extends-clause:</i>
    extends   <i>qualified-type-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>
    <i>extends-clause</i>   ,   <i>qualified-type-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>

  <i>uses-clause: </i>
    uses   <i>qualified-type-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>
      <i>uses-clause</i>   ,   
      <i>qualified-type-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>

  <i>class-body:</i>
    {   <i>class-member-declarations<sub>opt</sub></i>   }

  <i>trait-declaration:</i>
    <i>trait-modifiers<sub>opt</sub></i>   trait   <i>class-name</i>
      <i>generic-type-parameter-list<sub>opt</sub></i>   <i>constructor-declaration<sub>opt</sub></i>
      <i>extends-clause<sub>opt</sub></i>   <i>class-body<sub>opt</sub></i>

  <i>trait-modifiers:</i>
    <i>trait-modifier</i>
    <i>trait-modifiers</i>   <i>trait-modifier</i>

  <i>trait-modifier:</i>
    native
    private
</pre>

**Defined elsewhere**

* [*class-member-declarations*](Classes.md#class-members)
* [*constructor-declaration*](Classes.md#constructors)
* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*qualified-type-name*](Types.md#general)
* [*type-identifier*](Lexical-Structure.md#identifiers)

**Constraints**

A *class-modifiers* must not contain the following: More than one occurrence of the same modifier or both `base` and `value` modifiers.

A *trait-modifiers* must not contain more than one occurrence of the same modifier.

The *class-name*s in all *class-declaration*s and *trait-declaration*s for any given module must be distinct.

A generic class and a non-generic class declared in the same module must not have the same *class-name*. Likewise for generic and non-generic traits.

In a *class-declaration* having an *extends-clause*, only a base class or an ordinary, reference class can extend a base class. (A trait can always extend another trait.)

A class or trait must not designate itself in *qualified-type-name* directly (or indirectly).

Each *qualified-type-name* in a *uses-clause* must designate a trait.

If *class-body* declares any abstract members, the class being declared must not be an ordinary class.

A concrete ordinary class must provide a definition for each of the non-native members from all the base classes it extends and from all the traits it uses, using the exact same signature as defined in each base or trait.

A trait can be used by a class, and as a type constraint. It cannot appear in any other contexts.

A *class-declaration* whose *class-name* has a `.` prefix must be public and must be declared inside a named module.

The *class-declaration* for an ordinary class can only omit *constructor-declaration* if at least one of that class’s base classes or traits has a *constructor-declaration*.

**Semantics**

A *class-declaration* defines a class type by the name *class-name*. A *class-declaration* whose *class-name* has a `.` prefix is promoted to the global module. Likewise for traits.

The modifier `base` declares the class to be a ***base class***, which is an abstract class. A class that is not a base class is an ***ordinary class***.

The modifier `native` declares the class or trait to be a [native](Basic-Concepts.md#native-support) classor trait .

The modifier `private` declares the class or trait to have private [accessibility](Basic-Concepts.md#accessibility). In the absence of `private`, a *class-declaration* has public accessibility.

The modifier `value` declares the class type to be a ***value class*** type. A value class has value semantics; every time an instance of a value class is used, a copy is made. In the absence of `value`, the class type is a ***reference class*** type. A reference class has reference semantics.

*constructor-declaration* declares the fields for the classor trait, their default initial values, and whether the class or trait’s constructor argument list has positional or named form. If *constructor-declaration* is omitted, the class or trait has no fields and its (non-existent) constructor argument list is considered to have positional form. See [*constructor-declaration*](Classes.md#constructors) for the details.

In an *extends-clause* for a class declaration, if an accessible base class exists by the name *qualified-type-name*, *extends-clause* specifies the base classes from which the class being declared is derived.

In an *extends-clause* for a trait declaration, if an accessible trait exists by the name *qualified-type-name*, *extends-clause* specifies the traits to be used to extend the set of fields and methods for the trait being declared.

A derived class inherits all the members from all its base classes. The ordering of base classes in *extends-clause* is relevant for constructors having fields and using the positional form. If *extends-clause* is omitted, the class has no base classes. A class declared to extend a given base class multiple times directly or indirectly behaves as if it had been declared to extend that base only once. Likewise for traits.

In a *uses-clause*, if an accessible trait exists by the name *qualified-type-name*, *uses-clause* specifies the traits that are used by the class being defined.

The using class includes all the fields and methods from all of its traits. The ordering of traits in *uses-clause* is relevant for constructors having fields and using the positional form. If *uses-clause* is omitted, no traits are used. A class declared to use a given trait multiple times directly or indirectly behaves as if it were declared to use it only once.

If *class-body* is omitted, or it is present, but *class-member-declarations* is omitted, no new members are added to the class or trait being declared beyond any constructor and fields specified by *constructor-declaration*, and whatever members the class or trait being declared inherits or gets.

A base class cannot be instantiated. A base class can be derived from other base classes.

A trait cannot be instantiated; a trait simply contributes (ultimately) to an ordinary class that can be instantiated.

There is no way for a method `m` in a derived class to call a method m in any of its base classes.

A class is mutable if any of the following are true:
* At least one of its field set is declared `mutable`
* At least one of its field set has a mutable type
* At least one of its methods is declared `mutable`

Note: The library value classes `Bool`, `Char`, `Int`, and `Float` are recognized by the compiler as special. As such, although they have no named fields, each has space to store an instance value, which is accessible via `this`. Their special handling is based on their being `native`, not on their being `value` class types. Programmer-defined value classes can have named fields.

**Examples**

```
trait Equality { … }
trait Orderable extends Equality { … }
// ----------------------------------------------
native value class Int uses Equality, Orderable, Show { … }
// ----------------------------------------------
base class ArrayKey uses Orderable { … }
class IntKey(value: Int) extends ArrayKey { … }
// ----------------------------------------------
base class Nullable<+T: nonNullable> { … }
class Box<+T: nonNullable>(value: T) extends Nullable<T> { … }
// ----------------------------------------------
class Queue<+T> {front: List<T>, back: List<T>} { … }
// ----------------------------------------------
class Ref<+T>(mutable value: T)
```

## Class Members

A class or trait may have zero or more ***members***, which define the data and function members of that class or trait, and in the case of child classes or traits, super-classes or traits being extended.

**Syntax**

<pre>
  <i>class-member-declarations:</i>
    <i>class-member-declaration</i>
    <i>class-member-declarations   class-member-declaration</i>

  <i>class-member-declaration:</i>
    <i>child-class-declaration</i>
    <i>constant-declaration</i>
    <i>method-declaration</i>
    <i>type-constant-declaration</i>
</pre>

**Defined elsewhere**

* [*child-class-declaration*](Classes.md#child-classes)
* [*constant-declaration*](Classes.md#constants)
* [*method-declaration*](Classes.md#methods)
* [*type-constant-declaration*](Classes.md#type-constants)

**Semantics**

The members of a class or trait are those specified by its *class-member-declarations*, its constructor, the fields declared by its constructor, the members inherited from its base classes, and the members from the traits it uses.

A class or trait may contain the following members:

* [Child Classes](Classes.md#child-classes) – classes derived from this class.
* [Constants](Classes.md#constants) – the named constants associated with the class.
* [Fields](Classes.md#fields) – the instance variables of the class or trait, as declared in the class or trait’s constructor.
* [Methods](Classes.md#methods) – the computations and actions that can be performed by the class or by an instance of the class.
* [Constructor](Classes.md#constructors) – conceptually, the actions required to initialize the field set in an instance of the class or trait. (A constructor has no code.)
* [Type Constant](Classes.md#type-constants) – the named types associated with the class.

Methods can either be *static* or *instance* members. A static method is declared using `static`. An instance method is one that is not static. Constants and type constants are implicitly static. Fields are implicitly non-static.
Each instance of a class contains its own, unique set of the fields declared for that class.

The name of a static method, constant, or type constant can never be used on its own; it must always be used as the right-hand operand of the `::` [member selection operator](Expressions.md#member-selection-operators). The name of an instance method or field can never be used on its own; it must always be used as the right-hand operand of the `.` [member selection operator](Expressions.md#member-selection-operators). (There is one exception to this prohibition: A field name must be used on its own when named notation is used to set its value in a constructor call.)

## Child Classes

**Syntax**

<pre>
  <i>child-class-declaration:</i>
    children   =   <i>child-class-list</i>   ;

  <i>child-class-list:</i>
    |<i><sub>opt</sub></i>   <i>child-class</i>
    <i>child-class-list</i>   |   <i>child-class</i>

  <i>child-class:</i>
    .<i><sub>opt</sub></i>   <i>type-identifier</i>   <i>constructor-declaration<sub>opt</sub></i>
</pre>

**Defined elsewhere**

* [*constructor-declaration*](Classes.md#constructors)
* [*type-identifier*](Lexical-Structure.md#identifiers)

**Constraints**

The parent class of this member must be a base class.

A *child-class* having a `.` prefix must be public and must be declared inside a named module.

The *type-identifier*s in a *child-class-declaration* must be distinct.

A derived class declared in this manner cannot have any modifiers, type parameters, *extends-clause*s or *uses-clause*s, or a class body.

**Semantics**

A *child-class-declaration* provides a simple, shorthand way to declare a set of public, ordinary, derived class types, each called by their corresponding *type-identifier*, at the same time as declaring the parent base class.

The *type-identifier* in a *child-class* having a `.` prefix is promoted to the global module.

Each class in the set of derived classes has the same semantics as if that classes were declared separately, outside this class. For example, the following code:

```
base class Base {
  children = A | B
  fun get(): Int	// get really is an abstract method
  | A() -> 112
  | B() -> 42
}
class C extends Base {
  fun get(): Int {
    7
  }
}
```

is equivalent to:

```
base class Base {
  fun get(): Int;
}
class A extends Base { fun get(): Int { 112 }
class B extends Base { fun get(): Int { 42 }
class C extends Base { fun get(): Int { 7 }
```

A class may have multiple *child-class-declaration*s, and other classes declared outside this class can also derive from this class.

**Examples**

```
base class Parent<T>{value: Int} {
  fun m1(): void { … }
  children = Child1
  const con: Float = 1.2;
  children = Child2{private f1: String} | Child3{protected f2: Int = -99}
  type T = Int;
}
class Child<T> extends Parent<T> {
  fun m2(): void { … }
}
// -----------------------------------------
base class Order uses Equality, Show {
  children = LT | EQ | GT

  fun ==(x: Order): Bool
  | LT() -> x match { LT() -> true | _ -> false}
  | EQ() -> x match { EQ() -> true | _ -> false}
  | GT() -> x match { GT() -> true | _ -> false}

  fun toString(): String
  | LT() -> "less than"
  | EQ() -> "equal to"
  | GT() -> "greater than"
}
```

## Fields

The data representation of an instance of a class consists of all the ***fields*** in that class, its base classes, and its traits. A class need not have any fields.

Consider the following type hierarchy rooted in class `C1`:

```
trait Ta(tf1: Int, tf2: Int)
trait Tb(tf3: Bool) extends Ta
trait Tc(tf4: String)
trait Td(tf5: Char)

base class Ba(cf1: Int)
base class Bb(cf2: Float, cf3: Char) extends Ba uses Td
base class Bc(cf4: Bool)

class C1(cf5: String) extends Bb, Bc uses Tb, Tc
 ```

An instance of `C1` has the following fields:
* `cf5` from `C1`
* `cf2` and `cf3` from `Bb`
* `cf1` from `Ba`
* `tf5` from `Td`
* `cf4` from `Bc`
* `tf3` from `Tb`
* `tf1` and `tf2` from `Ta`
* `tf4` from `Tc`

Although there clearly is a type hierarchy here, it is useful to think of `C1` as being a class having 10 fields. This is especially so when writing a call to `C1`’s constructor, as this requires arguments for all 10 fields.

The ordering and alignment of fields in a class is unspecified.

That said, there is one situation in which ordering is relevant, and this involves a constructor call that uses positional notation. Given the type hierarchy above, in what order must the arguments be given to `C1`’s constructor? (This has nothing to do with the physical ordering of fields in storage, however.)

Here is a valid call to that constructor with the comment showing each argument’s corresponding field:

```
C1("abc", true, 11, 22, "end", 1.2, '$', '*', 10, false)
//  cf5   tf3   tf1 tf2  tf4   cf2  cf3  tf5  cf1 cf4
```

The rules governing the argument order in this situation are, as follows:
1. First, fields in the class being constructed, in left-to-right order of their declaration.
1. Next, for each trait in the (optional) `uses` clause, in left-to-right order of the trait name list, the fields in that trait, in left-to-right order of their declaration. Then, if any trait has an `extends` clause, the process is repeated recursively over the traits it extends.
1. Next, for each base class in the (optional) `extends` clause, in left-to-right order of the base-class name list, the fields in that base class, in left-to-right order of their declaration. Then, if any base class has a `uses` clause, the process is repeated recursively over the traits it extends. Then, if any base class has an `extends` clause, the process is repeated recursively over its base classes.

As such, the order of base class or trait names in an `extends` clause, or of trait names in a `uses` clause, is important. Compare class `C1` from above with new class `C2`, in which the ordering in both lists has been reversed:

```
class C1(cf5: String) extends Bb, Bc uses Tb, Tc
class C2(cf5: String) extends Bc, Bb uses Tc, Tb
```

Here is a valid call to the constructor for `C2`:

```
C2("abc", "end", true, 11, 22, false, 1.2, '$', '*', 10);
//  cf5   tf4    tf3   tf1 tf2 cf4    cf2  cf3  tf5  cf1
```

Note that although the `extends` clause can either precede or follow a `uses` clause, the following class declarations are equivalent:

```
class C1(cf5: String) extends Bb, Bc uses Tb, Tc
class C3(cf5: String) uses Tb, Tc extends Bb, Bc
```

As the ordering of the names in the `extends` and `uses` clauses are the same, the argument list order in constructor calls to each is the same.

As fields are defined in a [*constructor-declaration*](Classes.md#constructors), refer to that section for more information.

## Constructors

**Syntax**

<pre>
  <i>constructor-declaration:</i>
    <i>constructor-modifiers<sub>opt</sub></i>   (   <i>constructor-pos-parameter-list<sub>opt</sub></i>   )
    <i>constructor-modifiers<sub>opt</sub></i>   (   <i>constructor-pos-parameter-list</i>   ,   )
    <i>constructor-modifiers<sub>opt</sub></i>   {   <i>constructor-nam-parameter-list</i>   ,<sub>opt</sub></i>   }

  <i>constructor-modifiers:</i>
    <i>constructor-modifier</i>
    <i>constructor-modifiers</i>   <i>constructor-modifier</i>

  <i>constructor-modifier:</i>
    <i>accessibility</i>   
    final

  <i>constructor-pos-parameter-list:</i>
    <i>constructor-pos-parameter</i>
    <i>constructor-pos-parameter-list</i>   ,   <i>constructor-pos-parameter</i>

  <i>constructor-pos-parameter:</i>
    <i>field-modifiers<sub>opt</sub></i>   <i>nontype-identifier<sub>opt</sub></i>
     <i>generic-type-parameter-list<sub>opt</sub></i>   <i>type-specification</i>

  <i>field-modifiers:</i>
    <i>field-modifier</i>
    <i>field-modifiers</i>   <i>field-modifier</i>

  <i>field-modifier:</i>
    <i>accessibility</i>   
    mutable

  <i>constructor-nam-parameter-list:</i>
    <i>constructor-nam-parameter</i>
    <i>constructor-nam-parameter-list</i>   ,   <i>constructor-nam-parameter</i>

  <i>constructor-nam-parameter:</i>
    <i>field-modifiers<sub>opt</sub></i>   <i>nontype-identifier<sub>opt</sub></i>
     <i>generic-type-parameter-list<sub>opt</sub></i>   <i>type-specification</i>
     <i>default-value</i><sub>opt</sub>
</pre>

**Defined elsewhere**

* [*accessibility*](Classes.md#methods)
* [*default-value*](Functions.md#function-declarations)
* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*nontype-identifier*](Lexical-Structure.md#identifiers)
* [*type-specification*](Types.md#general)

**Constraints**

If they have fields, a base class *constructor-declaration* and the *constructor-declaration*s of any classes derived from that base must use the named form of notation.

Each *nontype-identifier* within the *constructor-declaration* for a class or trait and all the *constructor-declaration*s of its base classes and traits, must be distinct.

If *expression* is present in *default-value*, the type of *expression* must be a subtype of *type-specifier* in *type-specification*.

A *constructor‐modifiers* must not contain more than one occurrence of the same modifier.

A *field‐modifiers* must not contain more than one occurrence of the same modifier.

A *constructor-declaration* for an ordinary class or trait must not have the modifier `final`.

A *constructor-declaration* must not have the modifier `final` if that constructor is to be overridden in a derived class.

The accessibility of a derived class must be the same as that if its base classes.

**Semantics**

A class or trait can optionally have a *constructor-declaration*; for example, in the following declarations, `B`, `T`, and `C` each has a *constructor-declaration* of the form `(`…`)`:

```
base class B(bf1: String)
trait T(tf1: Float)
class C(cf1: Int) extends B uses T
```

The purpose of a *constructor-declaration* is to declare the fields for that type, referred to as the ***field set*** for that type. However, despite its name, a *constructor-declaration* does **not** actually declare a constructor method, as will be explained later. In this example, `C`’s field set is made up of the three fields, one per *constructor-declaration*: `bf1`, `tf1`, and `cf1`.
The only types that can be instantiated are ordinary classes, such as `C` above. An example of the creation of a `C` is, as follows:

```
  c = C(10, 1.23, "abc");
```

In this case, an instance of type `C` is constructed; that is, the implementation allocates memory for its field set, and those fields are initialized with their corresponding constructor arguments. Unlike other languages where a constructor is a class member that is a special method, **in Skip, a constructor is conceptual; no such method exists, so no user-written code can be invoked during construction!** That said it is convenient (even if it is a bit misleading) to say that an object is instantiated and initialized by a call to that object type’s *constructor*, which looks like a method whose parameters correspond to fields.

Note: If a field set of a class changes, so too does the signature of that class’s constructor.

If a *constructor-declaration* is omitted from a class or trait, that class or field has an empty field set; that is, it contributes no fields. Unlike some other languages, Skip has no concept of a *default constructor*.

In some languages, an object is built from the bottom up, by having the top-level type’s constructor call the next level’s constructor, and so on down the class hierarchy. No such chaining exists in Skip; the top-level object is created all at once based on the field set.

Note: As no user code is executed during construction, it is not possible to validate the arguments passed to the constructor. To get around this, a *constructor-declaration* could be made `private`, and a public static factory method be provided to validate the arguments passed to it, and it calls the private constructor.

If *constructor-modifiers* is omitted or has no *accessibility* modifier, the *constructor-declaration* is public.

If *constructor-pos-parameter-list* is present, each of its *nontype-identifier*s defines a field for that class or trait, and the mutability and accessibility of that field. For *constructor-nam-parameter-list*, each of its *nontype-identifier*s defines a field for that class or trait, the mutability and accessibility of that field, and the field’s default initial value, if any, if no corresponding argument is passed when the constructor is invoked. If *nontype-identifier* is omitted for a field, a field with unspecified name is used by the implementation. If *accessibility* is omitted from a field, public accessibility is assumed. If *constructor-pos-parameter-list* is omitted, the *constructor-declaration* contributes no fields..

An ordinary class is instantiated by an explicit invocation of the [function-call operator](Expressions.md#function-call-operator). That call contains arguments for at least all of the fields in that class’s field set that do not have an *initializer*. Each field is bound to the value of its corresponding argument, except that for omitted arguments, in which case, the field is bound to its *initializer* value.

The modifier `final` in a base class allows a *constructor-declaration* from being provided in a derived class. If `final` is omitted, a *constructor-declaration* is permitted (but not required) in a derived class. Likewise for traits.

A derived class’s overriding *constructor-declaration* augments its base class *constructor-declaration* by providing new fields. It does not replace that base class's *constructor-declaration*.

Note: By declaring fields using a parameter-list-like notation on a class or trait definition, this allows pattern-matching to be used on objects.

**Examples**

```
class InvalidIndex(i: Int) extends Exception { … }
class Range {private pos: Int, private len: Int} … { … }
class Box<+T: nonNullable>(value: T) extends Nullable<T> { … }
```

## Methods

**Syntax**

<pre>
  <i>method-declaration:</i>
    <i>method-modifiers<sub>opt</sub></i>   <i>method-declaration-header</i>   <i>method-body</i>

  <i>method-modifiers:</i>
    <i>method-modifier</i>
    <i>method-modifiers</i>   <i>method-modifier</i>

  <i>method-modifier:</i>
    <i>accessibility</i>
    async
    capture
    static
    deferred
    memoized
    mutable
    native
    overridable
    untracked

  <i>accessibility:</i>
    private
    protected

  <i>method-declaration-header:</i>
    fun   <i>method-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>when-params<sub>opt</sub></i>
      <i>parameters-specifier</i>   :   <i>method-return-type</i>   <i>from-clause<sub>opt</sub></i>
    fun   <i>method-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>when-params<sub>opt</sub></i>
      <i>from-clause<sub></i>

  <i>method-name: one of</i>
    <i>nontype-identifier</i>  ==  !=  <=  <  >=  >  +  -  *  /  %  !

  <i>when-params:</i>
    [   <i>generic-type-parameters</i>   ]

  <i>method-return-type:</i>
    <i>type-specifier</i>
    this

  <i>method-body:</i>
    ;
    <i>block</i>
    <i>pattern-branch-list</i>
</pre>

The *pattern-branch-list* in a *method-body* is referred to as an ***algebraic body***. In this case, the expression `this` (which designates the instance on which the method is being called) is the test expression. *pattern-branch-list* contains the possible target expression(s).

**Defined elsewhere**

* [*from-clause*](Classes.md#constants)
* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*generic-type-parameters*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*parameters-specifier*](Functions.md#function-declarations)
* [*pattern-branch-list*](Expressions.md#pattern-branch-lists)
* [*type-specifier*](Types.md#general)

**Constraints**

A *method-declaration* must not have more than one of the accessibility modifiers `protected` and `private`.

A *method-declaration* for an abstract method must not have the modifier `overridable`.

A *method-declaration* for a method in an ordinary class must not have the modifier `overridable`.

A *method-declaration* for a concrete method must have the modifier `overridable` if that method is to be overridden in a derived class.

A *method-declaration* for a method having the `private` modifier cannot also have the modifier `overridable`.

An overriding method must have the same signature and a compatible return type as the method being overridden.

The `deferred` modifier can only be present on a static method in a base class.

If the modifier `async` is present, *return-type* must be a [reactive type](Types.md#reactive-types). Conversely,
if *return-type* is a reactive type, the modifier `async` must be present.

The modifier `untracked` … **UNDER CONSTRUCTION**

For a method to be able to modify a field within its class or trait, that method must be declared mutable.

A *method-declaration* having a *from-clause* must have a *method-body* of `;`.

The *type-identifier* in a *from-clause* must designate a base class of the class whose overridable method is being declared.

An algebraic body is only permitted in an instance method of a base class.

The *pattern-branch-list* of an algebraic body must have a leading `|`.

*pattern-branch-list* must be exhaustive.

If the *type-specifier* in *method-return-type* is `void`, the *block* must have that the void value. Otherwise, the *block* must have a value whose type is a subtype of *type-specifier*.

A generic method and a non-generic method in the same scope cannot have the same *method-name*.

Each *type-identifier* in *when-params* must be a subtype of its corresponding *type-constraint*. However, unlike *generic-type-parameters* in *generic-type-parameter-list*, *generic-type-parameters* in *when-params* are not constrained to being traits.

**Semantics**

A method is a function that is defined inside a class.

A *method-definition* declares a method called *method-name*.

If *function-body* is `;` and no `native` modifier is present in *method-declaration*, the declared method is abstract; otherwise, it is concrete.

The modifier `async` declares the function to be [asynchronous](Expressions.md#async-operator). For an async function, control may be transferred back to the caller before the function terminates normally. In such a case, the awaitable object that will be returned to the caller later on acts like a placeholder that will eventually be filled with the return result.

The modifier `capture` is needed if the class is mutable and the method uses `this` in any context than `this.`*memberName*. In such contexts, `capture` means that the object is being treated as immutable. When a method is called on a mutable instance of a class, it is necessary to ensure that the method is not used in a way that could be unsound for a mutable object.

Ordinarily, the method being called by a [function-call](Expressions.md#function-call-operator) expression is named explicitly. However, inside a static method having the modifier `deferred`, the determination of the actual method can be delayed by using `static`, allowing the calling context to be that of a derived type. Consider the following:

```
base class Foo { x: String } {
  deferred static fun create(): this {
    static { x => "xxx" }
  }
  fun get(): String { this.x }
}
class Bar extends Foo
fun main(): void {
  print_raw(Bar::create().get())
}
```

A base class cannot be instantiated, so `create` cannot contain `Foo{…}`. However, by using `static{…}` instead, and having a return type of `this`, what is actually constructed and returned is an instance of a derived, concrete type, in this case, a `Bar`.

The modifier `memoized` … **UNDER CONSTRUCTION**

The modifier `mutable` indicates a [mutable](Basic-Concepts.md#mutability) method.

The modifier `native` declares the function to be a [native](Basic-Concepts.md#native-support) function. A native function has no implementation.

The modifier `overridable` allows (but does not require) a replacement implementation of that method to be provided in a derived class.
The modifiers `private` and `protected` declare the method to have private and protected [accessibility](Basic-Concepts.md#accessibility), respectively. In the absence of both, the method has public accessibility.

The modifier `react` … **UNDER CONSTRUCTION**

The modifier `static` declares a static method. A *method-declaration* not containing the modifier `static` declares an instance method. A static method only has access to static members of its parent class.

A *method-declaration* having a *from-clause* is used to disambiguate between multiple definitions of a method called *method-name* in the base classes of the current class.

A method has access to its own class’s members and to accessible members of its base classes, via `this`.

Parameters are declared and handled in the same manner as with [functions](Functions.md#function-declarations). After all parameters have been bound, *method-body* is executed.

A parameter named `_` causes the value of the corresponding argument to be ignored.

Each *type-identifier* in *when-params* is constrained by its corresponding *type-constraint*.

A *method-return-type* of *type-specifier* is handled just like a *type-specifier* in a [function](Functions.md#function-declarations), except that in a method, the return type is *method-return-type*.

Regarding algebraic functions, for simple cases, the algebraic notation is equivalent to having a brace-delimited function body containing a `this match {` *pattern-branch-list* `}`. However, the two approaches are not always equivalent. Consider the following example:

```
base class Parent {
  children = A()
  fun foo(): Int
  | A() -> 1
  | B() -> 2
}
class B() extends Parent
class C() extends Parent {
  fun foo(): Int { 3 }
}
```

The declaration of `foo` in `C` is necessary; without it, that would make `Parent::foo` abstract, which is not permitted.

Trying to write this using `match` directly, as follows:

```
base class Parent {
  children = A()
  fun foo(): Int {
    this match { A() -> 1 | B() -> 2 }
  }
}
```

results in an invalid override error for `C::foo`, as the method wasn't declared to be overridable. A second error is a non-exhaustive match unless you have an implementation for `C::foo`.

Here’s a second example:

```
base class Parent {
  children = A() | B()
  private fun foo(): Int      // Error
  | A() -> 1
  | B() -> 2
}
```

A private abstract declaration in `Parent` is not permitted. However,

```
base class Parent {
  children = A() | B()
  private fun foo(): Int {   // OK
    this match {
    | A() -> 1
    | B() -> 2
    }
  }
}
```

This is allowed; `foo` is simply not callable in `A` or `B`.

**Examples**

```
native value class Bool … {
  native fun ==(Bool): Bool;
  fun toString(): String {
    if (this) "true" else "false"
  }
  …
}
// -----------------------------------------
base class Exception {
  overridable fun getMessage(): String { "TODO" }
}
class InvalidIndex(i: Int) extends Exception {
  fun getMessage(): String {
    "Invalid index of " + this.i.toString()
  }
}
// -----------------------------------------
native value class Float … {
  native fun !=(Float): Bool;
  native fun +(Float): Float;
  fun negate(): Float { 0.0 - this }
  fun isNaN(): Bool { this != this }
  …
}
// ----------------------------------------------
base class B1 {
  fun m1(): void { … }
}
base class B2 {
  fun m1(): void { … }
}
class D1 extends B1, B2 {
  fun m1(): void from B1;
}
// -----------------------------------------
base class Foo<T>{x: T} {
  fun hello[T: IntKey](): String;
}
class Bar<T> extends Foo<T> {
  fun hello[T: ArrayKey](): String { this.x.toString() }
}
fun main(): void {
  print_raw(Bar{ x => StringKey("Pass")}.hello())
}
// -----------------------------------------
class Foo<T>(x : T) {
  fun add<U>[T: U](x : U): Foo<U> { Foo(x) }
  fun inv(x : T): Foo<T> { this.add(x) }
}
// -----------------------------------------
trait Show {
  capture fun toString(): String;
}
native class String uses Equality, Orderable, Show {
  fun toString(): String { … }
  …
}
// -----------------------------------------
base class Bar {
  deferred static fun get(): Int { static::getImpl() }
  protected static fun getImpl(): Int;
}
class Foo extends Bar {
  protected static fun getImpl(): Int { 42 }
}
fun main(): void {
  print_raw(Foo::get().toString())
}
```

## Constants

**Syntax**

<pre>
  <i>constant-declaration:</i>
    <i>constant-modifiers<sub>opt</sub></i>   const   <i>const-name</i>   :   <i>type-specifier</i>   <i>initializer<sub>opt</sub></i>   ;
    <i>constant-modifiers<sub>opt</sub></i>   const   <i>const-name</i>   <i>from-clause</i>   ;

  <i>constant-modifiers:</i>
    <i>constant-modifier</i>
    <i>constant-modifiers</i>   <i>constant-modifier</i>

  <i>constant-modifier:</i>
    <i>accessibility</i>   
    overridable

  <i>from-clause:</i>
    from   <i>type-identifier</i>   
</pre>

Note: The reason *const-name* can be *type-identifier* is to support [constant patterns](#sec-xxx).

**Defined elsewhere**

* [*accessibility*](Classes.md#methods)
* [*const-name*](Constants-and-Variables.md#global-constants)
* [*initializer*](Expressions.md#general)
* [*nontype-identifier*](Lexical-Structure.md#identifiers)
* [*type-identifier*](Lexical-Structure.md#identifiers)
* [*type-specifier*](Types.md#general)

**Constraints**

A *constant-declaration* containing a *from-clause* must use *type-identifier* to name a base class of the current class, that has an accessible, overridable constant called *nontype-identifier*.

A *constant-declaration* for an abstract constant must not have the modifier `overridable`.

A *constant-declaration* for a concrete constant must have the modifier `overridable` if that constant is to be overridden in a derived class.

A *constant-declaration* for a constant having the `private` modifier cannot also have the modifier `overridable`.

A *constant-declaration* for a constant in an ordinary class must not have the modifier `overridable`.

For a class constant member of an ordinary class, if there is no *from-clause*, *initializer* must be present.

**Semantics**

If a *constant-declaration* has an *initializer*, it declares a concrete constant; otherwise, it declares an abstract constant.

A class constant is bound to its initial value prior to the parent class’s first use; as such, *initializer* need not be a compile-time constant.

The modifier `overridable` allows (but does not require) a replacement implementation of that constant to be provided in a derived class.

A *from-clause* is used to disambiguate between multiple definitions of *nontype-identifier* in the base classes of the current class.

**Examples:**

```
class C1(parm: Int) {
  const c1a: Int = -12;
}
class C2 {
  const c2d: Int = 100;
  const c2e: Int = C1::c1a;

  const min: Int = 10;
  const max: Int = 50;
  const range: Int = C2::max - C2::min + 1;
}
// -----------------------------------------
base class B1 {
  const con1: Int = 10;
}
base class B2 {
  const con1: Int = 20;
}
class D1 extends B1, B2 {
  const con1: Int from B1;
}
```

## Type Constants

**Syntax**

<pre>
  <i>type-constant-declaration:</i>
   <i>accessibility<sub>opt</sub></i>   type   <i>type-identifier</i>
     <i>generic-type-parameter-list<sub>opt</sub></i>
     <i>type-specification<sub>opt</sub></i>   <i>default-clause<sub>opt</sub></i>   ;
   <i>accessibility<sub>opt</sub></i>   type   <i>type-identifier</i>
      <i>generic-type-parameter-list<sub>opt</sub></i>   <i>type-initializer</i>   ;

  <i>default-clause:</i>
    default   <i>type-specifier</i>
</pre>

**Defined elsewhere**

* [*accessibility*](Classes.md#methods)
* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*type-identifier*](Lexical-Structure.md#identifiers)
* [*type-initializer*](Types.md#type-constants)
* [*type-specification*](Types.md#general)
* [*type-specifier*](Types.md#general)

**Constraints**

For a class type constant member of an ordinary class, *type-initializer* must be present.

*type-specifier* must not be an abstract type constant.

If both *default-clause* and *type-specification* are present, *type-specifier* in *default-clause* must be a subtype of *type-specifier* in *type-specification*.

**Semantics**

When *type-initializer* or *from clause* are present, the class type constant is concrete; otherwise, it is abstract.

When an abstract type constant is implemented in a derived class, its type there is constrained by *type-specifier* in *type-specification*. If an abstract type constant is not implemented in a derived class and a *default-clause* is present, it is considered to be defined with a type specified by *type-specifier* in *default-clause*.

In its concrete form, *type-constant-declaration* creates an alias, *type-identifier*, for the type specified by *type-specifier*. Once such a type constant has been defined, that alias can be used in any context in which *type-specifier* is permitted. In its abstract form, *type-constant-declaration* creates an alias that can be associated with a *type-specifier* in a derived class.

**Examples**

```
base class Bar {
  type T;				// abstract
  fun hello(): this::T;
}
class Baz() extends Bar {
  type T = String;
  fun hello(): this::T { "Pass" }
}
// -----------------------------------------
base class Foo {
  type T: ArrayKey;
  static fun isValid(x: ArrayKey): Bool {
    x match {
    | this::T _ -> true
    | _ -> false
    }
  }
}
class Bar() extends Foo {
  type T = IntKey;
}
// -----------------------------------------
base class Foo {
  type T;
}
// -----------------------------------------
base class Foo {
  type T default Int;			// T is not constrained
}
class Bar() extends Foo {
  fun num(): this::T { 42 }		// uses default of Int
}
class Baz() extends Foo {
  type T = String;
  fun str(): this::T { "42" }
}
// -----------------------------------------
base class Arry {
  type T : ArrayKey default IntKey;	// T is constrained to ArrayKey
}
class Inty() extends Arry {
  fun key(): this::T { IntKey(42) }	// uses default of IntKey
}
class Stringy() extends Arry {
  type T = StringKey;
  fun key(): this::T { StringKey("") }
}
// -----------------------------------------
base class Foo {
  type T1;
  type T2 : this::T1 default this::T1;
}
```

## Indexable Collections

***Indexing*** involves using an index value to designate a corresponding element in a collection of elements.

An ***indexable collection*** is an instance of a (possibly generic) class type that supports the notion of indexing, by way of the `[`*index-expression*`]` and `[]` notations used with the [simple-mutation operator, `=`](Expressions.md#simple-mutation), and with the [indexing operator, `[]`](Expressions.md#indexing-operator).

There are two kinds of indexing operations: read and write. An indexable collection can provide either or both.

### Read Indexing Operations

A ***read indexing operation*** supports syntax of the form *collection*`[`*index*`]`, and is implemented via an accessible, immutable instance method called `get` that takes one positional parameter. The compiler converts this form of expression into the following call: *collection*`.get(`*index*`)`, where `get` is declared, as follows:

```
class MyIndexableCollection(…) {
  fun get(idx: T1): T2 { … }
}
```

The type of *index* must be a subtype of *T1*, and the result type of the operation is *T2*. The types *T1* and *T2* are *not* otherwise constrained.

The intent is that `get` return the value of the collection element corresponding to *index*.

### Write Indexing Operations

Write indexing operations are broken down further, into the following: write-set, write-mset, write-pushBack, and write-mpushBack. An indexable collection can provide any of these.

#### Write-Set Indexing Operations

A ***write-set indexing operation*** supports syntax of the form `!`*collection*`[`*index*`] = `*value*, and is implemented via an accessible, immutable instance method called `set` that has two positional parameters. The compiler converts this form of expression into the following call: *collection*`.set(`*index*`,` *value*`)`, where `set` is declared, as follows:

```
class MyIndexableCollection(…) {
  fun set(idx: T1, val: T2): T3 {
}
```

The type of *index* must be a subtype of *T1*, and the type of *value* must be a subtype of *T2*. The types *T1*, *T2*, and *T3* are *not* otherwise constrained.

The intent is that `set` make a copy of the indexable collection, assign the copy’s element designated by *index* to the value of *value*, and return the copy. The original indexable collection is unchanged.

#### Write-Mset Indexing Operations

A ***write-mset indexing operation*** supports syntax of the form *collection*`![`*index*`] = `*value*, and is implemented via an accessible, mutable instance method called `mset` that has two positional parameters. The compiler converts this form of expression into the following call: *collection*`.mset(`*index*`,` *value*`)`, where `mset` is declared, as follows:

```
class MyIndexableCollection(…) {
  mutable fun mset(idx: T1, val: T2): void { … }
}
```

The type of *index* must be a subtype of *T1*, and the type of *value* must be a subtype of *T2*. The types *T1* and *T2* are *not* otherwise constrained.

The intent is that `mset` assign the element designated by *index* to the value of *value*, changing the indexable collection in the process.

#### Write-PushBack Indexing Operations

A ***write-pushBack indexing operation*** supports syntax of the form `!`*collection*`[] = `*value*, and is implemented via an accessible, immutable instance method called `pushBack` that has one positional parameter. The compiler converts this form of expression into the following call: *collection*`.pushBack(`*value*`)`, where `pushBack` is declared, as follows:

```
class MyIndexableCollection(…) {
  fun pushBack(val: T1): T2 {
}
```

The type of *value* must be a subtype of *T1*. The types *T1* and *T2* are *not* otherwise constrained.

The intent is that `pushBack` make a copy of the indexable collection, append to that copy the value designated by *value*, and return the copy. The original indexable collection is unchanged.

#### Write-MpushBack Indexing Operations

A ***write-mpushBack indexing operation*** supports syntax of the form *collection*`![] = `*value*, and is implemented via an accessible, mutable instance method called `mpushBack` that has one positional parameter. The compiler converts this form of expression into the following call: *collection*`.mpushBack(`*value*`)`, where `mpushBack` is declared, as follows:

```
class MyIndexableCollection(…) {
  mutable fun mpushBack(val: T1): void { … }
}
```

The type of *value* must be a subtype of *T1*. The type *T1* is *not* otherwise constrained.

The intent is that `mpushBack` append the value designated by *value*, changing the indexable collection in the process.

## Predefined Classes

The [runtime library](RTL) contains numerous predefined base classes, ordinary classes, and traits.
