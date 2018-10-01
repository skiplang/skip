# Functions

## General

In Skip, what is commonly known in other programming languages as a *subroutine* or *procedure*, is called a ***function***. When a function is called, information may be passed to it by the caller via an ***argument list***, which contains one or more values known as ***argument***s. Arguments correspond by position or name to the ***parameter***s in a ***parameter list*** in the called function's declaration. Specifically, a list of ***positional argument***s corresponds with a list of ***positional parameter***s, and a list of ***named argument***s corresponds with a list of ***named parameter***s. (See later below.)

A [*parameters-specifier*](Functions.md#function-declarations) having parentheses (`()`) delimiters is said to have ***positional form***, allowing arguments to be “passed by position”; the first argument corresponds to the first parameter, the second argument corresponds to the second parameter, and so on. A *parameters-specifier* having brace (`{}`) delimiters is said to have ***named form***, allowing arguments to be “passed by name”; each argument is passed with a name that identifies the corresponding parameter. The [function-call operator](Expressions.md#function-call-operator) has corresponding forms: `()` to call a positional-form function, and `{}` to call a named-form function.

Skip does not support ***variadic functions***; that is, functions callable with a variable number of arguments.

Used generally, the term *function* encompasses the following callable entities:

* A [global function](Functions.md#function-declarations)
* A function member of a class or trait, referred to throughout this specification as a [*method*](Classes.md#methods)
* The [constructor](Classes.md#constructors) for a class
* A [lambda function](Functions.md#lambdas)

For the definition of a native function, see [§§](Basic-Concepts.md#native-support).

## Function Calls

A function is called via the [function-call operator](Expressions.md#function-call-operator).

A call to a function allows zero or more arguments to be passed, and one value to be returned.

## Function Declarations

**Syntax**

<pre>
  <i>function-declaration:</i>
    <i>function-modifiers<sub>opt</sub></i>   <i>function-declaration-header</i>   <i>function-body</i>

  <i>function-modifiers:</i>
    <i>function-modifier</i>
    <i>function-modifiers</i>   <i>function-modifier</i>

  <i>function-modifier:</i>
    async
    memoized
    native
    private
    untracked

  <i>function-declaration-header:</i>
    fun   .<i><sub>opt</sub></i>   <i>function-name</i>   <i>generic-type-parameter-list<sub>opt</sub></i>
      <i>parameters-specifier</i>   :   <i>return-type</i>

  <i>function-name:</i>
    <i>nontype-identifier</i>

  <i>parameters-specifier:</i>
    (   <i>pos-parameter-list<sub>opt</sub></i>   )
    (   <i>pos-parameter-list</i>   ,   )
    {   <i>nam-parameter-list<sub>opt</sub></i>   }
    {   <i>nam-parameter-list</i>   ,   }

  <i>pos-parameter-list:</i>
    <i>pos-parameter</i>
    <i>pos-parameter-list</i>   ,   <i>pos-parameter</i>

  <i>pos-parameter:</i>
    <i>type-specifier</i>   <i>generic-type-parameter-list<sub>opt</sub></i>
    <i>nontype-identifier</i>   :   <i>type-specifier</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>default-value<sub>opt</sub></i>  

  <i>nam-parameter-list:</i>
    <i>nam-parameter</i>
    <i>nam-parameter-list</i>   ,   <i>nam-parameter</i>

  <i>nam-parameter:</i>
    <i>type-specifier</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>default-value<sub>opt</sub></i>
    <i>nontype-identifier</i>   :   <i>type-specifier</i>   <i>generic-type-parameter-list<sub>opt</sub></i>   <i>default-value<sub>opt</sub></i>  

  <i>default-value:</i>
    =   <i>expression</i>

  <i>return-type:</i>
    <i>type-specifier</i>

  <i>function-body:</i>
    ;
    <i>block</i>
</pre>

**Defined elsewhere**

* [*block*](Expressions.md#blocks)
* [*expression*](#sec-XXXX)
* [*generic-type-parameter-list*](Generic-Types-Methods-and-Functions.md#type-parameters)
* [*nontype-identifier*](Lexical-Structure.md#identifiers)
* [*type-specifier*](Types.md#general)

**Constraints**

A *function-declaration* having the modifier `native` must have a *function-body* of `;`.

A *function-declaration* in the global module must not have the modifier `private`.

A *function-declaration* whose *function-name* has a `.` prefix must be public and must be declared inside a named module.

If the modifier `async` is present, *return-type* must be a [reactive type](Types.md#reactive-types). Conversely, if *return-type* is a reactive type, the modifier `async` must be present.

Each *nontype-identifier* in a *parameters-specifier* must be distinct.

If the *type-specifier* in *return-type* is `void`, the *block* must have the value `void`. Otherwise, the *block* must have a value whose type is a subtype of *type-specifier*.

A generic function and a non-generic function in the same scope cannot have the same *function-name*.

Programmers are discouraged from spelling *nontype-identifier* in *name-parameter* with a leading `_`.

**Semantics**

A *function-definition* declares a global function called *function-name*.

A *function-declaration* whose *function-name* has a `.` prefix is promoted to the global module.

The modifier `async` declares the function to be [asynchronous](Expressions.md#async-operator). For an async function, control may be transferred back to the caller before the function terminates normally. In such a case, the awaitable object that will be returned to the caller later on acts like a placeholder that will eventually be filled with the return result.

The modifier `memoized` … **UNDER CONSTRUCTION**

The modifier `untracked` … **UNDER CONSTRUCTION**

The modifier `native` declares the function to be a [native](Basic-Concepts.md#native-support) function. A native function has no implementation.

The modifier `private` declares the function to have private [accessibility](Basic-Concepts.md#accessibility). In the absence of `private` the function has public accessibility.

A function can be declared with zero or more parameters, each of which is specified in its own *pos-parameter* or *nam-parameter*. Each parameter has an optional name (*nontype-identifier*), a type (*type-specifier*), and an optional *default-value*.

The parameters in a *parameters-specifier* are considered to be declared at the start of *block*.

A parameter named `_` causes the value of the corresponding argument to be ignored.

For a constructor, see [constructor parameters](Classes.md#constructors). Otherwise, when a function is called, the name (*nontype-identifier*) of each parameter for which there is a corresponding argument is bound to that argument’s value. If a parameter has no name, the corresponding argument value is bound to an unspecified name created by the implementation. For a parameter having no corresponding argument, the name of that parameter is bound to its *default-value*.

After all parameters have been bound, *function-body*'s *block* is executed, and the value of that block becomes the return value of the function, whose return type is *return-type*.

Each parameter is a [local variable](Constants-and-Variables.md#local-variables) of its parent function.

**Examples**

```
fun invariant(cond: Bool, msg: String): void {
  if(!cond) throw InvariantViolation(msg)
}
// ----------------------------------------------
fun min<T: Orderable>(x: T, y: T): T {
  if (x < y) x else y
}
// ----------------------------------------------
fun fst<T1, T2>(p: (T1, T2)): T1 {
  (x, _) = p; x
}
// ----------------------------------------------
private fun for_impl(n: Int, f: Int ~> void, current: Int): void {
  if (current < n) {
    f(current);
    for_impl(n, f, current + 1)
  }
}
```

## Lambdas

A ***lambda*** is an object that encapsulates an unnamed function. (Such a function is sometimes known as an ***anonymous function*** or a ***closure***.) A lambda must be defined in the context of an expression whose value is used immediately to call that function, or that is saved in a variable for later use. A lambda has a type as described in [§§](Types.md#lambda-types), and a lambda is created via the [lambda-creation operator](Expressions.md#lambda-creation).

## Asynchronous Functions

The term *asynchronous programming* generally refers to design patterns that allow for cooperative transfer of control between several distinct tasks on a given thread of execution (or possibly a pool of threads) in a manner that isolates tasks from each other and minimizes unnecessary dependencies and interactions between tasks. Asynchronous programming is often used in the context of I/O, and with tasks that depend on I/O. Using asynchronous programming can make it significantly easier for a program to batch work together when calling synchronous I/O APIs (APIs that cause the thread to block until the I/O operation is complete) and it can make it easier to use asynchronous APIs (APIs that allow a thread to continue executing while the I/O operation is in progress).

A function is declared to be asynchronous via the function modifier [`async`](#sec-Function-Definitions). The execution of an asynchronous function can be *suspended*; that is, control is returned to its caller, until a designated *await condition* is satisfied. This condition is specified via the operator [`await`](Expressions.md#await-operator).

When an async function is compiled, a transformation is performed, so that an object of type `^`*T* is returned to the caller (where *T* is the function's *return-type*). This object is used to keep track of the state of execution for the async function and to keep track of dependencies on other tasks. Each time a given async function is called, a newly allocated object is returned to the caller.
When an async function is invoked, it executes synchronously until it returns normally, it throws an error/exception, or it reaches an await operation.

The *await operation* provides a way for an async function to yield control in cases where further progress cannot be made until the result of a dependency is available.

An *await operation* takes a single operand called the *dependency*, which must be an object of [reactive type](Types.md#reactive-types). (Such an object is called an *awaitable*.) When an await operation executes, it checks the status of the dependency. If the result of the dependency is available, the await operation produces the result (without yielding control elsewhere) and the current async function continues executing. If the dependency has failed due to an exception, the await operation re-throws this exception. Otherwise, the await operation updates the current async function's awaitable to reflect its dependence on the dependency and then yields control. The async function is considered *blocked* and ineligible to resume execution until the result of the dependency is available.
It is possible to wait on multiple dependencies by calling the library functions [`join2`](RTL) and [`join3`](RTL).

When yielding control, the implementation of `await` may choose to yield control back to the current async function's caller. Alternatively, if the dependency is a task and its async function is not blocked (i.e., it is eligible to resume execution), the implementation may choose (for optimization purposes) to perform an invocation to resume the dependency's async function to make further progress and then check again if the result of the dependency is available.

If an await operation would result in a dependency cycle (i.e., a task waiting on itself, or a group of two or more tasks that wait on each other in a cycle), the await operation will fail and an exception will be thrown.

Typically, when an async function terminates normally, control is transferred back to its caller. However, the implementation may choose (for scheduling or optimization reasons) to perform invocations to resume other dependent async functions that are now eligible to execute before ultimately transferring control back to the caller.
