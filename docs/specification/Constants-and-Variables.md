# Constants and Variables

## General

There are two kinds of constants:

* [global constants](Constants-and-Variables.md#global-constants)
* [class constants](Classes.md#constants)

There are two kinds of variables:

* [local variables](Constants-and-Variables.md#local-variables)
* [class variables](Classes.md#fields)

## Global Constants

A ***global constant*** is a constant declared at [global scope](Basic-Concepts.md#scope).

**Syntax**

<pre>
  <i>global-constant-declaration:</i>
    <i>gc-modifiers<sub>opt</sub></i>   const   .<i><sub>opt</sub></i>   <i>const-name</i>   :   <i>type-specifier</i>  <i>initializer</i>   ;


  <i>gc-modifiers:</i>
    <i>gc-modifier</i>
    <i>gc-modifiers</i>   <i>gc-modifier</i>

  <i>gc-modifier:</i>
    native
    private

  <i>const-name:</i>
    <i>nontype-identifier</i>
    <i>type-identifier</i>
</pre>

Note: The reason *const-name* can be *type-identifier* is to support [constant patterns](Expressions.md#constant-patterns).

**Defined elsewhere**

* [*initializer*](Expressions.md#general)
* [*nontype-identifier*](Lexical-Structure.md#identifiers)
* [*type-identifier*](Lexical-Structure.md#identifiers)
* [*type-specifier*](Types.md#general)

**Constraints**

A *global-constant-declaration* having a modifier `native` must not have an *initializer*.

A *global-constant-declaration* in the global module must not have the modifier `private`.

A *global-constant-declaration* whose *nontype-identifier* has a `.` prefix must be public and must be declared inside a named module.

**Semantics**

A global constant is bound to its initial value at [program start-up](Basic-Concepts.md#program-start-up); as such, *initializer* need not be a compile-time constant.

A *global-constant-declaration* whose *nontype-identifier* has a `.` prefix is promoted to the global module.

The modifier `native` declares the constant to be a [native](Basic-Concepts.md#native-support) constant. As such, it and its initial value are defined by the environment.

The modifier `private` is described in [§§](Basic-Concepts.md#accessibility).

**Examples**

```
const maxValue: Int = 1000;
const myNaN: Float = 0.0/0.0;
const text: String = "xxx" + "123" + "yyy";
// ----------------------------------------------
native const natConst1: Bool;
// ----------------------------------------------
const val1: Int = C1::c1a;
class C1(parm: Int) {
  const c1a: Int = -12;
}
```

## Class Constants

A class or trait may contain constant members. See [class constant](Classes.md#constants)

## Local Variables

A ***local variable*** is a variable declared at [block scope](Basic-Concepts.md#scope).

**Constraints**

If the name of a local variable that is not a function parameter begins with a lowercase letter, that name must be used somehow later in that function.

**Semantics**

A local variable is declared using the [simple mutation operator](Expressions.md#simple-mutation)

A [function parameter](Functions.md#function-declarations) is a local variable.

Consider the case in which a local variable that is not a function parameter, has a name beginning with `_`. The value of that variable cannot be used later in that function. That variable name can be re-bound, however, using `!`.

**Examples**

```
fun f(p1: Int, _p2: Int): void {	// parameter names exempt from usage check
  v1 = 10;		// Error: Local variable 'v1' was not used
  _v2 = 0;		// OK: underscore indicates it won't be used
  _v3 = _v2;		// Error: You cannot use a variable that starts with _
  !_v2 = 0;		// OK: Can rebind this name to a new value

  void
}
```

## Class Variables

A class may contain data members. See [fields](Classes.md#fields).
