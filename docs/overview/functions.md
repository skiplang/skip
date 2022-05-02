---
id: functions
title: Functions
---

Use `fun` to declare a function:

```
fun add1(x: Int): Int {
  x + 1
}
```

Skip is a typed language; function declarations must include the type of all parameters, as well as the
type returned by the function. The body of a function is an expression which is evaluated
to produce the return value of the function.

## Variables

Variables are introduced with the '=' operator:

```
fun add1(x: Int): Int {
  y = 1;
  x + y
}
```

The variable `y` is introduced without specifying its type; the types of local variables are inferred and rarely
 need to be explicitly specified.

## Expression Sequences

The above example also introduces the `;` operator. Expressions separated by a `;` are evaluated in sequence.
The result of an expression sequence is the result of evaluating the last expression in the sequence.

## Modifying Local Variables

Skip encourages an immutable style of programming: a style where variables are
assigned an initial value which is not later modified. In Skip the `=` operator introduces
a new variable, but unlike other languages the `=` operator alone can not modify
an existing variable.

To modify a local variable, prefix the variable with a `!` on the left hand side of the `=`.

```
fun add1(x: Int): Int {
  y = 0;  // declare a local with '='
  !y = 1; // modify an existing local by prefixing it with !
  x + y
}
```

## Types

Skip is a typed language. Declarations like function parameters, return types, and class fields
all include type annotations. The compiler computes the type of all expressions
and reports errors whenever an unexpected type is encountered.

## Primitive Types

Skip includes the usual primitive types: `Int`, `Float`, `String`, `Char`, `Bool`, `void`.

The `Int` type is 64-bit signed; the `Float` type holds 64 bit IEEE floating point values.
Skip also includes 8/16/32 bit signed and unsigned integral values.

## Explicit type annotations

The Skip compiler automatically infers the type of local variables and expressions.
The type of local variables and expressions may be stated explicitly:

```
fun add1(x: Int): Int {
  y: Int = 1;
  (x + y: Int)
}
```

## The type void

Skip has a special value called `void` of type `void`. It is legal (although not very useful) to pass it around.

```
fun notVeryUseful(): void {
  x = void;
  x
}
```

## Unused Values

Expression sequences, defined with the `;` evaluate each expression in the sequence.
The result of an expression sequence is the value of the last expression in the sequence.
All of the values produced by the non-terminal expressions in the sequence are discarded.

Not consuming computed values is a frequent source of bugs in other languages.
Skip enforces that only `void` values may be silently discarded. On the (rare) occasion when an
expression computes a non-void value as well as produces some meaningful side effects
the expression may be assigned to an unnamed variable `_`.

```
fun discardUnusedValue(): Int {
  _ = doSomethingWhichHasSideEffectsAsWellAsProducesAValue();
  42
}
```

The result of variable declarations (`=`) and mutations (`! =`) is the `void` value.
This enables them to be used as the non-terminal expression in a sequence.
