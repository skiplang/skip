# Expressions

## General

An ***expression*** is a combination of one or more terms and zero or more operators.

A ***full expression*** is an expression that is not part of another expression.

A ***side effect*** is an action that changes the state of the execution environment. (Examples of such actions are modifying a variable, writing to a device or file, or calling a function that performs such operations.)

When an expression is evaluated, it produces a result. It might also produce a side effect. Only a few operators produce side effects.

An ***lvalue*** is an an expression that designates a memory location having a type. A ***modifiable lvalue*** is an lvalue whose value can be changed. A ***non-modifiable lvalue*** is an lvalue whose value cannot be changed.

The occurrence of value computation and side effects is delimited by *sequence points*, places in a program's execution at which all the computations and side effects previously promised are complete, and no computations or side effects of future operations have yet begun. There is a sequence point at the end of each full expression. The [logical AND](#sec-Logical-AND-Operator), [logical OR](#sec-Logical-Inclusive-OR-Operator), and [function-call](#sec-Function-Call-Operator) operators each contain a sequence point. (For example, in the following series of expressions, `a = 10; b = f(a); b * 2`, there is sequence point at the end of each full expression, so the binding of `a` to 10 is completed before the value of `a` is passed to function `f`, and the binding of `b` to the value returned from `f` is completed before the value of `b` is multiplied by 2.)

When an expression contains multiple operators, the ***precedence*** of those operators controls the order in which those operators are applied. (For example, the expression `a - b / c` is evaluated as `a - (b / c)` because the / operator has higher precedence than the binary minus operator.) The precedence of an operator is defined by the position of its associated production in the syntactic grammar.
If an operand occurs between two operators having the same precedence, the order in which the operations are performed is defined by those operators' ***associativity***. With *left-associative* operators, operations are performed left-to-right. With *right-associative* operators, operations are performed right-to-left. Operators that are neither left- nor right-associative are *non-associative*.

Precedence and associativity can be controlled using *grouping parentheses*. (For example, in the expression `(a - b) / c`, the subtraction is done before the division. Without the grouping parentheses, the division would take place first.)

While precedence, associativity, and grouping parentheses control the order in which operators are applied, they do *not* control the order of evaluation of the terms themselves. The order in which the operands in an expression are evaluated relative to each other is left-to-right.

Unless stated otherwise, the result produced by an operator is not a modifiable lvalue.

**Syntax**

<pre>
  <i>qualified-nontype-name: </i>
    .   <i>nontype-identifier</i>
    <i>type-identifier</i>   .   <i>nontype-identifier</i>
    <i>qualified-type-name</i>   ::   <i>nontype-identifier</i>

  <i>initializer:</i>
    =  <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*nontype-identifier*](#sec-Identifiers)
* [*qualified-type-name*](#sec-Types.General)
* [*type-identifier*](#sec-Identifiers)

**Constraints**

For the form `.`*nontype-identifier*, the [global module](#sec-Modules.General) must contain directly a declaration for *nontype-identifier*.

For the form *type-identifier*`.`*nontype-identifier*, *type-identifier* must name a [module](#sec-Modules.General) that contains directly a declaration for the right-hand *nontype-identifier*.

**Semantics**

The form `.`*nontype-identifier* names the *nontype-identifier* in the [global module](#sec-Modules.General). The form *type-identifier*`.`*nontype-identifier* names the *nontype-identifier* in the module named *type-identifier*.

## Operator Definitions

Some operators (such as `||` and `&&`) are predefined in the language, and have fixed operand and result types. Others (such as binary `+` and `-`) can be defined on a per-class basis. For these, the semantics described in the sections following are based on these operators in the primitive value classes (such as `Bool`, `Int`, and `Float`). In these latter cases, operators are syntactic sugar for calls to methods to the underlying native value class types. For example, given `x = 3;`, `x + 1` calls method `+` in class `Int`, and that method returns the `Int` result.

Unless stated otherwise below, operators can be defined for user-defined types; however, their precedence, associativity, and number of operands cannot be changed.

## Patterns and Pattern Matching

### General

A ***pattern*** is a structural representation of a set of one or more values. Patterns are used to test the type and value of structured data and also to decompose structured data into its constituent parts. A pattern is matched to a ***test expression***. If a pattern ***matches*** the value of the test expression, then the pattern introduces a (possibly empty) set of bindings to the pattern’s ***target expression*** and then evaluates that expression. The test expression and target expression for a given pattern depend on the ***pattern context*** in which the pattern is used. A pattern is ***exhaustive*** if it matches all possible values of the type of the pattern’s test expression. Some pattern contexts require their patterns to be exhaustive.

There are several kinds of patterns: class, literal, constant, name, tuple, and wildcard. Each kind of pattern imposes restrictions on the type of the pattern’s test expression. Each kind of pattern specifies what values it matches, what bindings the pattern introduces into the target expression when a match is successful, and whether the pattern is exhaustive.

Patterns are used in [pattern branch lists](#sec-Pattern-Branch-Lists) and [nested patterns](#sec-Pattern-Nesting). Each pattern context defines the test expression, target expression, and exhaustiveness requirement for the contained patterns.

A ***pattern alias*** is a *nontype-identifier* associated with a pattern that can be used to refer to the matched pattern’s value in the pattern’s target expression. If *pattern* has the form *pattern-with-alias*, the alias’s *nontype-identifier* is bound to the matching test expression value.

**Syntax**

<pre>
  <i>pattern:</i>
    <i>class-pattern </i>
    <i>constant-pattern</i>
    <i>literal-pattern</i>
    <i>name-pattern</i>
    <i>tuple-pattern</i>
    <i>wildcard-pattern</i>
    <i>as-pattern</i>

  <i>as-pattern:</i>
    <i>pattern</i>   as   <i>nontype-identifier</i>

  <i>test-expression:</i>
    <i>expression</i>
</pre>

**Defined elsewhere**
* [*class-pattern*](#sec-Class-Patterns)
* [*constant-pattern*](#sec-Constant-Patterns)
* [*literal-pattern*](#sec-Literal-Patterns)
* [*name-pattern*](#sec-Name-Pattern)
* [*nontype-identifier*](#sec-Identifiers)
* [*tuple-pattern*](#sec-Tuple-Patterns)
* [*wildcard-pattern*](#sec-Wildcard-Pattern)

An *as-pattern* causes *nontype-identifier* to bind to the matching expression.

### Pattern Branch Lists

A ***pattern branch list*** is used to match a test expression against a series of patterns and to execute different code based on which pattern in the list was matched. Pattern branch lists are used in several contexts: [match expressions](#sec-The-match-Expression), [catch clauses](#sec-The-try-Expression), and [algebraic function bodies](#sec-Methods). Each pattern branch list context defines the test expression for that pattern branch list.

A pattern branch list is a sequence of ***pattern branches***. Each pattern branch contains a list of one or more patterns, an optional when-clause, and a target expression. The test expression for each branch’s pattern list is the test expression of the pattern branch list. When a pattern branch list has more than one pattern, those patterns are OR’d together.

A pattern branch list attempts to match the pattern branch list’s test expression against the patterns in each pattern branch list in sequence. When a pattern in a branch list is matched, the result of the pattern branch list is the value of the target expression of the matched pattern. (Note that the pattern may introduce bindings into the pattern’s target expression.)

A sequence of pattern branches is exhaustive if the patterns they contain are known to match all possible values in the pattern branch list’s test expression.

A pattern branch list is exhaustive if the last pattern in the list is exhaustive. Each kind of pattern specifies how to determine if a sequence of that kind of pattern is exhaustive. Otherwise, the sequence of patterns is not exhaustive.

A *pattern-branch* whose *pattern* has one or more *wildcard-pattern*s and no *name-pattern*s, is called a ***wildcard branch***.

**Syntax**

<pre>
  <i>pattern-branch-list:</i>
    |<sub>opt</sub>   <i>pattern-branch-bar-list</i>

  <i>pattern-branch-bar-list:</i>
    <i>pattern-branch</i>
    <i>pattern-branch-bar-list</i>   |   <i>pattern-branch</i>

  <i>pattern-branch:</i>
    <i>pattern-list</i>   <i>when-clause<sub>opt</sub></i>   ->   <i>target-expression</i>

  <i>pattern-list:</i>
    <i>pattern</i>
    <i>pattern-list</i>   |   <i>pattern</i>

  <i>when-clause:</i>
    (   <i>controlling-expression></i>   )

  <i>target-expression:</i>
    <i>expression</i>
</pre>

*target-expression* is the target expression for its corresponding *pattern*.

**Defined elsewhere**

* [*controlling-expression*](#sec-The-if-Expression)
* [*expression*](#sec-Mutation-Operators.General)
* [*pattern*](#sec-Patterns-and-Pattern-Matching.General)

**Constraints**

The test expression for a *pattern-branch-list* must be a valid test expression for every *pattern-branch* in the list.

Each *pattern* in a *pattern-branch-list* must match some values which are not matched by the preceding branches. It is an error for an exhaustive pattern to not be the last pattern in a *pattern-branch-list*. Consider the following:

```
base class Base {}
base class Intermediate{} extends Base {}
class Top extends Intermediate {}
…
(expr: Base) match {
  | Intermediate _ -> …
  | Top _ -> …
  // Error: This pattern cannot match any values not previously matched
}
```

The types of the *target-expression*s in a *pattern-branch-list* must have a common supertype.

When a pattern branch has more than one pattern, and those patterns have bindings across patterns (see `fun foo` example below), each pattern must bind the exact same set of variables as any of the other patterns being OR’s, and each variable binding in each pattern must have the same type.

**Semantics**

The *pattern-branch*es in a *pattern-branch-list* are matched in lexical order. As such, the ordering of *pattern-branch*es can be significant.

The type of a *pattern-branch-list* is the supertype of the types of the target expressions of all of the *pattern-branch*es.

If the final pattern in a *pattern-branch-list* is exhaustive, then the *pattern-branch-list* is exhaustive.

When a pattern branch has more than one pattern, those patterns can have bindings across patterns. (See `fun foo` example below.)

**Examples**

```
fun f(p: Bool): String {
  p match {	// each pattern branch has 1 pattern only
    | true -> "On"
    | false -> "Off"
  }
}

base class A {
  children = B(Int) | C(Bool, Int) | D(Int, Bool, Char)
}

fun foo(x : A): Int {
  x match {	// 1 pattern branch ORs together 3 patterns
  | B(z)
  | C(_, z)
  | D(z, _, _) -> z
  }
}
```

### Pattern Nesting

One pattern may be nested inside another. This results in nested test expressions.

Consider the following *tuple-pattern*:

```
(10, 20) match {
  | (m, _) -> …
}
```

The tuple expression `(10, 20)` is the test expression for the *tuple-pattern* `(m, _)`. The integer expression `10` is the test expression for the nested *name-pattern* `m`. The integer expression `20` is the test expression for the nested *wildcard-pattern* `_`.

A similar situation can occur with *class-pattern*:

```
class C(f1: Int, f2: Int)
…
C(a, b) match {
  | C(_, x) -> …
}
```

The class expression `C(a, b)` is the test expression for the *class-pattern* `C(_, x)`. The integer expression `a` is the test expression for the nested *wildcard-pattern* `_`. The integer expression `b` is the test expression for the nested *name-pattern* `x`.

Patterns can nest arbitrarily; for example, a tuple pattern might contain a class pattern that contains a tuple pattern, a named pattern, a wildcard pattern, and a different class pattern. (For some examples, see [tuple patterns](#sec-Tuple-Patterns).)

### Wildcard Pattern

**Syntax**

<pre>
  <i>wildcard-pattern:</i>
    _
</pre>

**Constraints**

There are no constraints on the type of the test expression.

**Semantics**

This pattern matches a value of any type.

A *wildcard-pattern* does not introduce any bindings into the pattern’s target-expression.

This pattern is exhaustive.

**Examples**

```
_ -> …           // the matching value is ignored
_ as n …       // the matching value is bound to n
```

Examples of *wildcard-pattern*s nested inside *tuple-pattern*s and *class-pattern*s can be seen in [§§](#sec-Tuple-Patterns) and [§§](#sec-Class-Patterns), respectively.

### Name Pattern

**Syntax**

<pre>
  <i>name-pattern:</i>
    <i>nontype-identifier</i>
</pre>

**Defined elsewhere**

* [*nontype-identifier*](#sec-Identifiers)

**Constraints**

There are no constraints on the type of the test expression.

**Semantics**

This pattern matches a value of any type.

The *name-pattern*’s *nontype-identifier* is bound to the matching test expression value.

This pattern is exhaustive.

**Examples**

```
n -> …          // the new local variable n binds to the matching value
```

Examples of *name-pattern*s nested inside *tuple-pattern*s and *class-pattern*s can be seen in [§§](#sec-Tuple-Patterns) and [§§](#sec-Class-Patterns), respectively.

### Literal Patterns

**Syntax**

<pre>
  <i>literal-pattern:</i>
    <i>boolean-literal</i>
    <i>character-literal</i>
    -<sub>opt</sub></i>   <i>integer-literal</i>
    -<sub>opt</sub></i>   <i>floating-literal</i>
    <i>string-literal</i>
    <i>void-literal</i>
</pre>

**Defined elsewhere**

* [*boolean-literal*](#sec-Boolean-Literals)
* [*character-literal*](#sec-Character-Literals)
* [*floating-literal*](#sec-Floating-Point-Literals)
* [*integer-literal*](#sec-Integer-Literals)
* [*string-literal*](#sec-String-Literals)
* [*void-literal*](#sec-Void-Literal)

**Constraints**

The test expression and *literal-pattern* must have the same type.

**Semantics**

This pattern matches if the value of the test expression is the same as that of *literal-pattern*.

By itself, this pattern is not exhaustive; it can match just one value of the test expression.

A *pattern-branch-list* containing only *literal-pattern*s is exhaustive if the list contains *literal-pattern*s for every value in the test-expression’s type. (Typically, this only occurs for `Bool` test expressions; however, it is also possible for `Char` expressions.)

**Examples**

```
fun isRGB(str: String): Bool {
  str match {
  | "red" -> true
  | "green" -> true
  | "\u0062lue" -> true		// \u0062 is b
  | _ as x -> debug(x); false	// x binds to the matched string
			// this pattern makes the branch list exhaustive
 }
}
// -----------------------------------------
fun getStateStr(p: Bool): String {
  p match {
    | true -> "On"
    | false -> "Off"	// this makes the branch list exhaustive
  }
}
```

### Constant Patterns

**Syntax**

<pre>
  <i>constant-pattern:</i>
    <i>qualified-type-name</i>
</pre>

**Defined elsewhere**

* [*qualified-type-name*](#sec-Types-General)

**Constraints**

*qualified-type-name* must name a constant whose type has an `==` operator method.

**Semantics**

This pattern matches if the value of the test expression is the same as that of *qualified-type-name*.

By itself, this pattern is not exhaustive; it can match just one value of the test expression.

A *pattern-branch-list* containing only *constant-pattern*s is exhaustive if the list contains *constant-pattern*s for every value in the test-expression’s type. (Typically, this only occurs for `Bool` test expressions; however, it is also possible for `Char` expressions.)

 **Examples**

```
const On: Bool = true;
const Off: Bool = false;
fun constantTest(p: Bool): Bool {
  p match {
    | .On -> …
    | Off -> …
  }
}
// -----------------------------------------
class Colors() {
  const Red: String = "red";
  const Green: String = "green";
  const Blue: String = "blue";
}
fun isRGB(str: String): Bool {
  str match {
    | Colors::Red -> …
    | Colors::Green -> …
    | Colors::Blue -> …
  }
}
```

### Tuple Patterns

**Syntax**

<pre>
  <i>tuple-pattern:</i>
    (   <i>pattern</i>   ,   <i>pattern-comma-list</i>   )

  <i>pattern-comma-list:</i>
    <i>pattern</i>
    <i>pattern</i>   ,   <i>pattern-comma-list</i>
</pre>

**Defined elsewhere**

* [*pattern*](#sec-Patterns-and-Pattern-Matching.General)

**Constraints**

The test expression must have a tuple type with the same number of elements as the number of elements in *tuple-pattern*.

**Semantics**

Each element in a *tuple-pattern* is matched against the value of the corresponding element in the test expression. That element may have any type. The set of names bound in the pattern expression is the union of the name bindings for each element.

A *tuple-pattern* may contain [nested patterns](#sec-Pattern-Nesting), each of which has its own test expression.

This pattern is exhaustive.

**Examples**

```
(z1, (z2, z3)) -> …                  // tuple pattern inside a tuple pattern
(d1, C1A{x => d2, y => d3}) -> …     // class pattern inside a tuple pattern
(n, n) -> …    // error, because 'n' is bound in more than one tuple element
```

### Class Patterns

**Syntax**

<pre>
  <i>class-pattern:</i>
    <i>class-type-specifier</i>   <i>arguments-pattern</i>

  <i>arguments-pattern:</i>
    <i>positional-arguments-pattern</i>
    <i>named-arguments-pattern</i>
    _

  <i>positional-arguments-pattern:</i>
    (   <i>pattern-comma-list<sub>opt</sub></i>   )

  <i>named-arguments-pattern:</i>
    {   <i>named-argument-pattern-comma-list<sub>opt</sub></i>   }

  <i>named-argument-pattern-comma-list:</i>
    <i>named-argument-pattern</i>
    <i>named-argument-pattern</i>   ,   <i>named-argument-pattern-comma-list</i>

  <i>named-argument-pattern:</i>
    <i>non-type-identifier</i>   <i>named-argument-value-pattern<sub>opt</sub></i>

  <i>named-argument-value-pattern:</i>
    =>   <i>pattern</i>
</pre>

**Defined elsewhere**

* [*class-type-specifier*](#sec-Types.General)
* [*nontype-identifier-list*](#sec-Identifiers)
* [*pattern*](#sec-Patterns-and-Pattern-Matching.General)
* [*pattern-comma-list*](#sec-Tuple-Patterns)

**Constraints**

The test expression must have a reference-class type.

*class-type-specifier* must be the name of a base class, an ordinary class, or a type constant that designates a base class or ordinary class.

*class-type-specifier* must not be a type parameter.

*class-type-specifier* must be a subtype of the value being matched.

In a *named-argument-pattern*, *nontype-identifier* must be the name of a named field in the class’s field set.

For a *positional-arguments-pattern* pattern, *class-type-specifier* must contain a constructor that allows the same number of positional parameters as elements in the *positional-arguments-pattern*.

For a *named-arguments-pattern* pattern, *class-type-specifier* must contain a field whose name matches *nontype-identifier* for each element in the *named-arguments-pattern*.

**Semantics**

A *nontype-identifier* in *positional-arguments-pattern* binds to the corresponding positional field in the class’s constructor. A *wildcard-pattern* in *positional-arguments-pattern* causes the corresponding positional field in the class’s constructor to be ignored.

For a *named-argument-pattern* consisting of just a *nontype-identifier*, that *nontype-identifier* binds to the named field of the same name in the class’s constructor.

For a *named-argument-pattern* including a *named-argument-value-pattern*, *named-argument-value-pattern* binds to the named field called *nontype-identifier* in the class’s constructor.

An *arguments-pattern* of the form `_` causes the matching expression to be ignored.

A *class-pattern* may contain [nested patterns](#sec-Pattern-Nesting), each of which has its own test expression.

This pattern is exhaustive only if the type of the test expression is the class being matched.

**Examples**

```
class C4(f1: Int, f2: Int)
C4(m, n) -> …   // bind the new local variable m to the first positional
                // field’s value, the new local variable n to the second
C4(_, n) -> …   // ignore the first positional field’s value, bind the new
                // local variable n to the second
C4(_, _) -> …   // ignore both fields’ values
// -----------------------------------------
class C5 {f1: Int = 0, f2: String = "xxx"}
C5{f1 => m, f2 => n} -> …   // bind the new local variable m to field f1’s
   // value; bind the new local variable n to field f2’s
C5{f1 => m, f2 => _} -> …   // bind the new local variable m to field f1’s
   // value; ignore field f2’s
C5{f1 => m, f2} -> …        // bind the new local variable m to field f1’s
   // value; bind the new local variable f2 to field f2’s
C5{f1, f2 => n} -> …        // bind the new local variable f1 to field f1’s
   // value; bind the new local variable n to field f2’s
C5{f1, f2} -> …             // bind the new local variables f1 and f2 to the
   // values of fields f1 and f2, respectively
C5{f1 => m} -> …            // bind the new local variable m to field f1’s
   // value; implicitly ignore field f2’s
C5{f1 => _} -> …            // explicitly ignore field f1’s; implicitly ignore
   // field f2’s
C5{f1} -> …                 // bind the new local variable f1 to field f1’s
   // value; implicitly ignore field f2’s
C5{} -> …                   // implicitly ignore all field values
C5 _ -> …                   // explicitly ignore all field values
C5 _ as x -> …         // bind the new local variable x to the matching value
```

## Primary Expressions

### General

**Syntax**

<pre>
  <i>primary-expression:</i>
    <i>nontype-identifier</i>
    <i>qualified-type-name</i>
    <i>block</i>
    <i>if-expression</i>
    <i>match-expression</i>
    <i>throw-expression</i>
    <i>try-expression</i>
    <i>with-expression</i>
    <i>qualified-nontype-name</i>
    <i>literal</i>
    <i>class-literal</i>
    <i>collection-literal</i>
    <i>tuple-creation-expression</i>
    <i>lambda-creation-expression</i>
    (   <i>expression</i>   )
    this
    static
    void
</pre>

**Defined elsewhere**

* [*block*](#sec-Blocks)
* [*class-literal*](#sec-Class-Literals)
* [*collection-literal*](#sec-Collection-Literals)
* [*expression*](#sec-Mutation-Operators.General)
* [*if-expression*](#sec-The-if-Expression)
* [*lambda-creation-expression*](#sec-Lambda-Creation)
* [*literal*](#sec-Literals)
* [*match-expression*](#sec-The-match-Expression)
* [*nontype-identifier*](#sec-Identifiers)
* [*qualified-nontype-name*](#sec-Expressions.General)
* [*qualified-type-name*](#sec-Types-General)
* [*throw-expression*](#sec-The-throw-Expression)
* [*try-expression*](#sec-The-try-Expression)
* [*tuple-creation-expression*](#sec-Tuple-Creation)
* [*with-expression*](#sec-The-with-Expression)

**Semantics**

The type and value of a parenthesized *expression* are identical to those of the un-parenthesized *expression*.
The variable `this` is predefined inside any instance method, and designates the current object. `this` is a non-modifiable lvalue.

The variable `static` is predefined inside any static method, and is a [class literal](#sec-Class-Literals) for the current class type. `static` is a non-modifiable lvalue.

The expression [`void`](#sec-Void-Literal) has type [`void`](#sec-The-Void-Type).

### Class Literals

**Syntax**

<pre>
  <i>class-literal:</i>
    <i>qualified-type-name</i>   <i>generic-type-argument-list<sub>opt</sub></i>
</pre>

**Defined elsewhere**

* [*generic-type-argument-list*](#sec-Type-Arguments)
* [*qualified-type-name*](#sec-Types-General)

**Constraints**

*qualified-type-name* must designate an accessible [base or ordinary class](#sec-Class-Declarations) type.

**Semantics**

A ***class literal*** is a *qualified-type-name* that can be used in the context of an expression.

A *class-literal* that designates a base class *B*, has type `Base<`*B*`>` (where ordinary class `Base` is a subtype of the generic library base-class type [`Class`](RTL-type-Class)).

A *class-literal* that designates an ordinary class *O*, has type `Concrete<`*O*`>` (where ordinary class `Concrete` is a subtype of the generic library base-class type [`Class`](RTL-type-Class)).

**Examples**

Given that `IntKey` is an ordinary class that is a subtype of base class `ArrayKey`, and the following:

```
fun f(p1: Concrete<IntKey>, p2: Class<IntKey>): void {
  v1 = p1(10);	// OK: Binds to a Concrete<IntKey>
  …
}
```

the function call `f(IntKey, IntKey)` causes `f` to be passed two class literals, both having type `Concrete<IntKey>`. As `p1` has a concrete class type, its constructor can be called; that is, `p1(10)` is equivalent to `IntKey(10)`.

Given that the public base class `MyBase` is declared in module `M1`, the following bindings are permitted:

```
v1: Base<M1.MyBase> = M1.MyBase;
v2: Class<M1.MyBase> = M1.MyBase;
```

### Collection Literals

**Syntax**

<pre>
  <i>collection-literal:</i>
    <i>qualified-type-name</i>   <i>generic-type-argument-list<sub>opt</sub></i>   [   <i>expression-list<sub>opt</sub></i>   ]
</pre>

**Defined elsewhere**

* [*expression-list*](#sec-Tuple-Creation)
* [*generic-type-argument-list*](#sec-Type-Arguments)
* [*qualified-type-name*](#sec-Types-General)

**Constraints**

The type of each *expression* in *expression-list* must be a subtype of *generic-type-argument-list*.

**Semantics**

The evaluation of a *collection-literal* results in the creation of a collection of type *qualified-type-name generic-type-argument-list*, containing the zero or more elements designated by *expression-list*.

A *collection-literal* is converted by the compiler into a call to the public, static method

*qualified-type-name*`::fromItems(`*values*`)`

where *values* is an object whose type uses `Iterable`, which contains the elements from *expression-list*, if any.

The type and value of the result are those from the corresponding method `fromItems`.

**Examples**

```
Dict<(String, Int)>[("red", 10), ("white", 20), ("blue", 30)]
```

Calls the following public, static function:

`Dict::fromItems<I: Iterable<(K, V)>>(items: I): Dict<K, V>`

which returns a collection of three elements of type `Dict<K, V>`.

```
Vector<Float>[]
```

Calls following public, static function:

`Vector::fromItems<U, C: Iterable<U>>(elements: C): Vector<U>`

which returns an empty collection of type `Vector<U`>.

### Blocks

**Syntax**

<pre>
  <i>block:</i>
    {   <i>separated-expression-list</i>   ;<i><sub>opt</sub></i>   }

  <i>separated-expression-list:</i>
    <i>expression</i>
    <i>separated-expression-list</i>   ;   <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)

**Constraints**

In a *separated-expression-list*, all but the right-most *expression* must have type `void`.

**Semantics**

A *block* allows a group of one or more *expression*s separated by semicolons to be treated syntactically as a single expression.
The type and value of *block* is the type and value of the right-most *expression*.

**Examples**

```
if (condition) {        // braces required to group two actions on true
  expression1;
  expression2
}
else {                  // braces optional, as only one action on false
  expression3
}
// -----------------------------------------
p match {
  | pattern1 -> {       // braces required to group three actions
    expression1;
    expression2;
    expression3
  }
  | …
}
// -----------------------------------------
fun f1(): Int {         // a function’s body is a block
    expression1;
    expression2
}
// -----------------------------------------
fun f2(): void { void }
```

### The if Expression

**Syntax**

<pre>
  <i>if-expression:</i>
    if   <i>if-control</i>   <i>true-branch-expression   else-clause<sub>opt</sub></i>

  <i>if-control:</i>
    (   <i>controlling-expression</i>   )

  <i>controlling-expression:</i>
    <i>expression</i>

  <i>true-branch-expression:</i>
    <i>expression</i>

  <i>else-clause:</i>
    else   <i>false-branch-expression</i>

  <i>false-branch-expression:</i>
    <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*qualified-nontype-name*](#sec-Expressions.General)

**Constraints**

*qualified-nontype-name* must designate a value of type `Bool`.

*controlling-expression* must designate a value of type `Bool`.

If *else-clause* is present, the types of *true-branch-expression* and *false-branch-expression* must be compatible.

**Semantics**

If *controlling-expression* tests True, *true-branch-expression* is evaluated, and the type and value of its result becomes the type and value of the *if-expression*. Otherwise, if *else-clause* is present, *false-branch-expression* is evaluated, and the type and value of its result becomes the type and value of the *if-expression*.

**Examples**

```
if (expression1) expression2
// -----------------------------------------
base class C
class D1 extends C
class D2 extends C
v = if (expression) D1() else D2()	// both types extend type C
// -----------------------------------------
fun f1(): Bool { … }
fun f2(flag: Bool): Bool {
  if (flag) { expression1; f1() } else expression2
}
v = f2(if (expression3) { expression4 } else { expression5 });
```

### The match Expression

**Syntax**

<pre>
  <i>match-expression:</i>
    <i>test-expression</i>   match   {   <i>pattern-branch-list</i>   }
    <i>test-expression</i>   match   <i>pattern-branch-list</i>
</pre>

*test-expression* is the test expression for all *pattern*s in *pattern-branch-list*. *pattern-branch-list* contains the possible target expression(s).

**Defined elsewhere**

* [*pattern-branch-list*](#sec-Pattern-Branch-Lists)
* [*test-expression*](#sec-Patterns-and-Pattern-Matching.General)

**Constraints**

If the type of *test-expression* is one of the primitive value-class types, the patterns to be matched must be any combination of *name-pattern*s and *wildcard-pattern*s.

If the type of *test-expression* is a tuple type, the patterns to be matched must be *tuple-pattern*s.

If the type of *test-expression* is a reference-class type, the patterns to be matched must be *class-pattern*s.

*pattern-branch-list* must be exhaustive.

**Semantics**

A *match-expression* compares the value of *test-expression* with one or more *pattern*s in *pattern-branch-list*, until a match is found. The type and value of the result are the type and value of the *target-expression* for the matching *pattern*.

**Examples**

```
base class Foo {
  type T: ArrayKey;
  static fun isValid(x: ArrayKey): Bool {
    x match {
    | this::T _ -> true
    | _ -> false
    }
  }
}
// -----------------------------------------
class Bar<+TID: ArrayKey>(x: TID)
fun test2(): ?Bar<StringKey> {
  invariant_violation("")
}
fun getIntKeyAsArrayKey(): ArrayKey {
  IntKey(42)
}
fun main(): void {
  classname = (getIntKeyAsArrayKey() match {
    | IntKey _ -> Box(Bar(IntKey(42)))
    | StringKey _ -> test2()
  });
  print_raw(classname match {
    | Box(_) -> "Pass"
    | Null() -> "Fail"
  })
}
```

### The throw Expression

**Syntax**

<pre>
  <i>throw-expression:</i>
    throw   <i>expression</i>
</pre>


**Defined elsewhere**
* [*expression*](#sec-Mutation-Operators.General)

**Constraints**

The type of *expression* must be a subtype of class [`Exception`](RTL-type-Exception).

**Semantics**

A *throw-expression* throws an exception immediately and unconditionally.

Control never returns to the [throw point](#sec-Exception-Handling.General). See [§§](#sec-Exception-Handling.General) and [§§](#sec-The-try-Expression) for more details of throwing and catching exceptions.

To avoid type-compatibility errors, the type of a *throw-expression* is exactly the type needed in the context in which it appears, as demonstrated by the following:

```
class Ex1 extends Exception { … }
fun f1(): Float {
  throw Ex1()		// throw-expression has type Float
}
fun f2(): Int {
  throw Ex1(); 20	// throw-expression has type void
}
fun f3(p: (Int, String)): void { … }
fun f4(): void {
  f3(throw Ex1())	// throw-expression has tuple type (Int, String)
}
```

The value of a *throw-expression* is unspecified; in any event, it is not available to the programmer.

**Examples**

```
class Ex2{f1: Int = 10} extends Exception { … }
throw Ex2{}
throw Ex2{f1 => 20}
```

### The try Expression

**Syntax**

<pre>
  <i>try-expression:</i>
    try   <i>block   catch-clause</i>
  <i>catch-clause:</i>
    catch   {   <i>pattern-branch-list</i>   }
</pre>

The *block* in a *try-expression* is known as a ***try block***.

Note: Unlike some languages, which require a separate catch clause for each catchable type, Skip allows only one catch clause, which uses pattern matching to determine the type caught.

Note: Although the body of a *catch-clause* resembles a *block*, it is not a *block*!

**Defined elsewhere**

* [*block*](#sec-Blocks)
* [*pattern-branch-list*](#sec-Pattern-Branch-Lists)

See [§§](#sec-Exception-Handling.General) and [`throw`](#sec-The-throw-Expression) for more details of throwing and catching exceptions.

**Constraints**

The type of a try block must be `void`.

**Semantics**

Once an exception is thrown, the value of that exception becomes the test expression, and the environment searches for the nearest *pattern-branch* *pattern* that matches that value. The process begins at the current function level with a search for a try block that lexically encloses the throw point. The *pattern-branch*es in the *catch-clause* associated with that try block are considered in lexical order. If no match is found, the function that called the current function is searched for a lexically enclosing try block that encloses the call to the current function. This process continues until a *pattern-branch* is found that can handle the current exception.

If a matching *pattern-branch* is located, the environment evaluates the *target-expression* (from *pattern-branch-list*) that corresponds to the matching *pattern*. If no matching *pattern-branch* is found, the program terminates [abnormally](#sec-Program-Termination), with that exception’s `getMessage` method being called with the resulting string being given to the execution environment. As such, *pattern-branch-list* need not be exhaustive.

The type and value of a *try-expression* are the type and value of the matching *pattern-branch*’s *target-expression*.

**Examples**

```
class Ex1{…} extends Exception { … }
class Ex2(…) extends Exception { … }

try {
  …
  if (…) throw Ex1{…};
  …
}
catch {
    | Ex1{…} -> …
    | Ex2(…) -> {
        try {
          …
          if (…) throw Ex3(99);
          …
        }
        catch {
          | _ -> …
        }
      }
}
```

### The with Expression

**Syntax**

<pre>
  <i>with-expression:</i>
    <i>expression</i>   with   {   <i>nam-argument-expression-list<sub>opt</sub></i>   }
</pre>

**Defined elsewhere**

* [*expression*](#sec-Binding-Operators.General)
* [*nam-argument-expression-list*](#sec-Function-Call-Operator)

**Constraints**

The type of *expression* must have a constructor whose parameter list is declared using the named form.

Each of the *nontype-identifier*s in *nam-argument-expression-list* must name a distinct, accessible field in the object designated by *expression*.

**Semantics**

A *with-expression* is a shorthand way of making a copy of an object, with the specified fields being bound to given values, in a single expression. For example, given

```
class C1(x: Int, y: String, z: Bool)
a = C1(10, "red", true);
```

the following:

```
b = a with { x => 20, y => "blue" };
```

is equivalent to:

```
b = a;		// b.x = 20, b.y = "red", b.z = true
!b.x = 20;
!b.y = "blue";
```

The result of the expression is a new, immutable object of the same type as *expression*.  Any field not bound by the *with-expression* is bound to the value corresponding to that field in *expression*.

This operator is left-associative.

**Examples**

```
class C2(x: Int, y: String)
c = C2(20, "up");
d = c with { x => 40 } with { y => "down" };	// d.x = 40, d.y = "down"
```

### Tuple Creation

**Syntax**

<pre>
  <i>tuple-creation-expression:</i>
    (   <i>expression</i>   ,   <i>expression-list</i>   )

  <i>expression-list:</i>
    <i>expression</i>
    <i>expression-list</i>   ,   <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)

**Semantics**

A *tuple-creation-expression* creates a tuple with elements having values as specified by *expression* and *expression-list-one-or-more*, inserted in that order.

The type of a *tuple-creation-expression* is “tuple of type 'element type list in lexical order'”.

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
// -----------------------------------------
tup = (ArrayKey, Float);
// tuple contains a Base<ArrayKey> and a Concrete<Float>
```

### Lambda Creation

**Syntax**

<pre>
  <i>lambda-creation-expression:</i>
    untracked<sub>opt</sub>   <i>lambda-parameter-list</i>   <i>lambda-arrow</i>   <i>expression</i>
    <i>lambda-creation-expression</i>   <i>lambda-arrow</i>   <i> expression </i>

  <i>lambda-parameter-list:</i>
    <i>nontype-identifier</i>
    (   <i>nontype-identifier-list</i><sub>opt</sub>   )
    {   <i>nontype-identifier-list</i><sub>opt</sub>   }
  </pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*lambda-arrow*](#sec-Lambda-Types)
* [*nontype-identifier*](#sec-Identifiers)
* [*nontype-identifier-list*](#sec-Identifiers)

**Semantics**

A *lambda-creation-expression* creates a lambda that encapsulates a function having a list of positional parameters as specified by *lambda-parameter-list*, and a return type as implied by the type of *expression*.

A *nontype-identifier* of  `_` causes the value of the corresponding argument to be ignored.

A lambda defined inside an instance method has access to the instance’s `this`.

As with [*lambda-type-specifier*s](#sec-Lambda-Types), *lambda-creation-expression*s come in both immutable and mutable forms based on the choice of *lambda-arrow*.

`untracked` … **UNDER CONSTRUCTION**

**Examples**

```
lambda1 = () ~> "M1";
lambda2 = x ~> x.toString() + "XX";
lambda3 = x ~> (y -> x + y);
// -----------------------------------------
fun doit(iValue: Int, process: Int -> Int): Int {
  process(iValue)
}
result = doit(5, p ~> p * 2);		// doubles 5
result = doit(5, p ~> p * p);		// squares 5
// -----------------------------------------
class Foo { f: ( void ) ~> void } {
  fun call(): void {
    this.f()
  }
}
Foo { f => (() ~> {}) }.call();
// -----------------------------------------
lam: (mutable r: Ref<String>, t: String) -> void = (r, t) -> { r.value <- t };
mutable ref: Ref<String> = Ref("");
lam(ref, "NewValue");
// -----------------------------------------
fun foo(Bool ~> Int): void { … }
foo(c ~> if (c) 100 else 0);
// -----------------------------------------
fun move<T>(f: (mutable Ref<T>) -> void, t: T): T {
  ref: mutable Ref<T> = mutable Ref(t);
  f(ref);
  ref.value
}

fun f(): void {
  s = move(ref -> { ref.!value = "NewValue" }, "OldValue");
  void
}
```

## Postfix Operators

### General

**Syntax**

<pre>
  <i>postfix-expression:</i>
    <i>primary-expression</i>
    <i>function-call-expression</i>
    <i>member-selection-expression</i>
    <i>indexing-expression</i>
</pre>

**Defined elsewhere**

* [*function-call-expression*](#sec-Function-Call-Operator)
* [*indexing-expression*](#sec-Indexing-Operator)
* [*member-selection-expression*](#sec-Member-Selection-Operators)
* [*primary-expression*](#sec-Primary-Expressions.General)

**Semantics**

These operators associate left-to-right.

### Function-Call Operator

**Syntax**

<pre>
  <i>function-call-expression:</i>
    <i>postfix-expression</i>   <i>generic-type-argument-list<sub>opt</sub></i>   (   <i>pos-argument-expression-list<sub>opt</sub></i>   )
    <i>postfix-expression</i>   <i>generic-type-argument-list<sub>opt</sub></i>   (   <i>pos-argument-expression-list</i>   ,   )
    <i>postfix-expression</i>   <i>generic-type-argument-list<sub>opt</sub></i>   {   <i>nam-argument-expression-list<sub>opt</sub></i>   }
    <i>postfix-expression</i>   <i>generic-type-argument-list<sub>opt</sub></i>   {   <i>nam-argument-expression-list</i>   ,   }

  <i>pos-argument-expression-list:</i>
    <i>expression</i>
    <i>pos-argument-expression-list</i>   ,   <i>expression</i>

  <i>nam-argument-expression-list:</i>
    <i>nam-expression</i>
    <i>nam-argument-expression-list</i>   ,   <i>nam-expression</i>

  <i>nam-expression:</i>
    <i>nontype-identifier</i>   =>   <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*nontype-identifier*](#sec-Identifiers)
* [*postfix-expression*](#sec-Postfix-Operators.General)

Positional- and named-parameter (and argument) forms are described in [§§](#sec-Functions.General).

**Constraints**

*postfix-expression* must designate an accessible function, an accessible method, or be a variable of [lambda type](#sec-Lambda-Types), or be a [class literal](#sec-Class-Literals) that designates an [ordinary class](#sec-Class-Declarations).

A constructor call of the form `this(…)`/`this{…}` must be made from within a method.

The positional-argument form must only be used to call a function, method, or constructor whose parameter list is declared using the positional form. The named-argument form must only be used to call a function, method, or constructor whose parameter list is declared using the named form.

A *function-call-expression* must contain an argument for each parameter in the called [function's declaration](#sec-Function-Declarations) not having a default value. Furthermore, if the call is to a constructor of a class having one or more base classes or using traits, the call must also contain an argument for each base-class or trait constructor parameter not having a default value.

Except for the case involving a base class mentioned above, a *function-call-expression* must not contain more arguments than there are corresponding parameters.

*nontype-identifier* must be the name of a parameter in the called function, method, or constructor, or in the case of a constructor call, the name of a parameter in one of that class’s base-class or trait constructors.

In a *nam-expression*, the type of *expression* must be compatible with that of *nontype-identifier*.

Any given *nontype-identifier* must not appear more than once in any given *nam-argument-expression-list*.

A native function must be called using positional notation.

**Semantics**

A *function-call-expression* whose *postfix-expression* is a class literal is a ***constructor call***, in which *postfix-expression* designates the ***called constructor***. A *function-call-expression* of the form `this(…)`/`this{…}` is also a constructor call.  Otherwise, a *function-call-expression* having a *postfix-expression* form is a ***function call*** in which *postfix-expression* designates the ***called function*** or ***called method***. In either case, *pos-argument-expression-list* and *nam-argument-expression-list* specify the arguments, if any, to be passed to the constructor, function, or method. Each argument corresponds to, and is bound to, a parameter in the called constructor, function, or method's declaration, as described in [§§](#sec-Function-Declarations).

In a call to a constructor having a *pos-argument-expression-list*, the order of the argument *expressions* is significant, as described [here](#sec-Fields).

The argument expressions in an argument list are evaluated left-to-right.

When used from within an ordinary class, it’s a call to the constructor for that class. When used from within a base class, its a [deferred](#sec-Methods) call.

The return type of a constructor call is *qualified-type-name*.

When *postfix-expression* designates an instance method, inside the body of the called method, `this` is bound to the object used in that designation.

The optional trailing commas have no meaning; specifically, they do *not* imply an extra, empty argument.

Direct and indirect recursive function calls are permitted.

This operator cannot be defined for a user-defined type.

**Examples**

```
fun f1(start: Int = 0, length: Int = 1): void {…}
f1()			// use both defaults
f1(2)			// use one default
f1(3, 5, )		// superfluous trailing comma
// ----------------------------------------------
fun f2{start: Int = 0, length: Int = 1}: void {…}
f1{}			// use both defaults
f2{start => 2) 		// use one default
f1{length => 5, start => 3}
// ----------------------------------------------
module M;
base class C1{private p1: Int} { … }
class C2{private p2: Int} extends C1 { … }
module end;

M.C2{p2 => 4, p1 => 2};	// must provide arguments for both fields
// ----------------------------------------------
ik1 = IntKey(100);	// Constructs an IntKey
// ----------------------------------------------
v = IntKey;		// Results in a Concrete<IntKey>
ik2 = v(100); 		// Constructs an IntKey using class literal v
// ----------------------------------------------
module M1;
class .(p: String) { … }
module end;
fun f(p: Class<M1>): void { … }

f(M1);	// M1 is a class literal bound to M1’s class .
// ----------------------------------------------
trait Ta(tf1: Int, tf2: Int)
trait Tb(tf3: Bool) extends Ta
trait Tc(tf4: String)
trait Td(tf5: Char)

base class Ba(cf1: Int)
base class Bb(cf2: Float, cf3: Char) extends Ba uses Td
base class Bc(cf4: Bool)

class C1(cf5: String) extends Bb, Bc uses Tb, Tc

c = C1("abc", true, 11, 22, "end", 1.2, '$', '*', 10, false)
//      cf5   tf3   tf1 tf2  tf4   cf2  cf3  tf5  cf1 cf4
```

### Member-Selection Operators

**Syntax**

<pre>
  <i>member-selection-expression:</i>
    <i>postfix-expression</i>   .   <i>nontype-identifier</i>
    <i>postfix-expression</i>   ::   <i>nontype-identifier</i>
</pre>

**Defined elsewhere**

* [*nontype-identifier*](#sec-Identifiers)
* [*postfix-expression*](#sec-Postfix-Operators.General)

A *member-selection-expression* using `.` has the ***dot form*** while one using `::` has the ***double-colon form***.

**Constraints**

In the dot form, *postfix-expression* must designate an instance of a class having an accessible member called *nontype-identifier*.

In the double-colon form, *postfix-expression* must be a [class literal](#sec-Class-Literals) for a class having an accessible static member called *nontype-identifier*.

These operators cannot be defined for a user-defined type.

**Semantics**

A *member-selection-expression* designates the member *nontype-identifier*, whose type and value are the type and value of the result. For the dot form, if *postfix‐expression* designates a field, the result is a modifiable lvalue if *postfix-expression* is a modifiable lvalue.

These operators are left-associative.

**Examples**

```
class Point{x: Int = 0, y: Int = 0} {
  fun getX(): Int {
    this.x               // select field x in the current object
  }
  …
}
// ----------------------------------------------
module M1;
class C1(f1: Int, f2: C2) {
  fun m1(): void { … }
  const con = 10;
  static fun sm1(): void { … }
  …
}
class C2(f1: Int) {
  const con4: String = "con4";
  fun m1(): void {
    …
    … class(this)::con4);	// access static member
    …
  }
  static fun sm2(): void { … }
}
module end;
v1 = M1.C1(5, C2(3))    // . is part of the type qualifier, not an operator
v1.f1                   // select field f1 in the C1 designated by v1
v1.f2.m1()              // select method m1 in the C2 designated by field f2
                        // in the C1 designated by v1
M1.C1::sm1()            // M1.C1 is a qualified typename, which is a class
                        // literal, so need :: to select static member
class(v1)::con3.m1();   // create a class literal for v1, so can use ::
class(v1)::sm1()
class(v1.f2)::sm2()
class(class(v1)::con3)::sm2()
// ----------------------------------------------
100.toString()	// select method toString from class Int
// of which 100 is an instance
```

### Indexing Operator

**TBD: The Syntax for Indexing Tuples Might Change**

**Syntax**

<pre>
  <i>indexing-expression:</i>
    <i>postfix-expression</i>    &#91;   <i>index</i>   &#93;
  <i>index:</i>
    <i>expression</i>
</pre>

**Defined elsewhere**
* [*postfix-expression*](#sec-Postfix-Operators.General)

**Constraints**

*postfix-expression* must designate a tuple or an instance of an [indexable-collection-class type](#sec-Indexable-Collections).

When *postfix-expression* designates a tuple, *index* must be an *integer-literal* whose value is in the range \[0,*n*-1\], where *n* is the number of elements in that tuple’s type.

When *postfix-expression* designates an instance of an indexable-collection-class type, that type must support [read indexing operations](#sec-Indexable-Collections).

**Semantics**

When *postfix-expression* designates a tuple, *indexing-expression* designates element *index* in that tuple. The type of the result is that of the designated element.

When *postfix-expression* designates an instance of an indexable-collection-class type, the semantics are unspecified. However, a [read indexing operation](#sec-Indexable-Collections) is performed, and the type of the result is that of the method `get`. (The indexable-collection-class type is encouraged to throw an exception of type `OutOfBounds` if *index* is invalid.)

**Examples**

```
text = "abcdefg";
try {
  print_string("text[2] is " + text[2]);   // "c"
  print_string("text[20] is " + text[20]); // Index is OutOfBounds
}
catch {
  | OutOfBounds() -> print_string("Index is OutOfBounds")
};
// -----------------------------------------
t1 = (10, true, (2.5, "red"));
t1[0]      // designates the Int value 10;
t1[2]      // designates the tuple value (2.5, red);
t1[2][1]   // designates the String value "red"
// -----------------------------------------
class C1() {
  fun get(p: Int): String { … }
}

c1 = C1(…);
c1[100]   // the element corresponding to index 100
c1[-5]    //        "            "              -5
c1[idx]   //        "            "              idx
// -----------------------------------------
class C4() {
  fun get(p: String): Int { … }
}

C4 = C4(…);
C4["left"]   // the element corresponding to index "left"
C4["right"]  //        "            "              "right"
```

## Unary Operators

### General

**Syntax**

<pre>
  <i>unary-expression:</i>
    <i>postfix-expression</i>
    <i>unary-op-expression</i>
    <i>async-expression</i>
    <i>await-expression</i>
    <i>mutable-expression</i>
    <i>class-literal-creation-expression
</i></pre>

**Defined elsewhere**

* [*async-expression*](#sec-Async-Operator)
* [*await-expression*](#sec-Await-Operator)
* [*class-literal-creation-expression*](#sec-Class-Literal-Creation-Operator)
* [*mutable-expression*](#sec-Mutable-Operator)
* [*postfix-expression*](#sec-Postfix-Operators.General)
* [*unary-op-expression*](#sec-Unary-Arithmetic-Operators)

**Semantics**

These operators are right-associative, except for *class-literal-creation-expression*, which is non-associative.

### Unary Arithmetic Operators

**Syntax**

<pre>
  <i>unary-op-expression:</i>
    !   <i>multiplicative-expression</i>
    -   <i>multiplicative-expression</i>
</pre>

**Defined elsewhere**

* [*multiplicative-expression*](#sec-Multiplicative-Operators)

**Semantics**

For the unary operator `-`, the value of the result is the negated value of the operand.

For the unary operator `-` with an operand of type

* `Int`, see method [`Int.negate`](RTL-type-Int).
* `Float`, see method [`Float.negate`](RTL-type-Float).

For the unary `!` operator, the type of the result is `Bool`. The value of the result is `true` if the value of the operand is True; otherwise, the value of the result is `false`.

For the unary operator `!` with an operand of type `Bool`, see class [`Bool`](RTL-type-Bool).

As Skip does not support overloading of functions, there is no way to provide both a unary `-` and a binary `-` operator function. However, when a unary `-` is used with an expression of a user-defined type, and that type has a method called `negate` taking no arguments, that method is called.

**Examples**

```
if (v1 > -5) …
if (!flag) …
```

### Class Literal Creation Operator

**Syntax**

<pre>
  <i>class-literal-creation-expression:</i>
    class   (   <i>expression</i>   )
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)

**Constraints**

*expression* must not have any of the following types: function, tuple, trait, or `void`.

**Semantics**

The result of this operator is a [class literal](#sec-Class-Literals) that corresponds to the type of *expression*.

**Examples**

```
class C {
  const con: Int = 100;
  static fun sf(): void { … }
}
class(C())::con      // designates C's (static) constant con
c = C();
class(c)::sf()       // designates c's static method sf
```

### Async Operator

**Syntax**

<pre>
  <i>async-expression:</i>
    async   <i>unary-expression</i>
</pre>

**Defined elsewhere**

* [*unary-expression*](#sec-Unary-Operators.General)

**Semantics**

The (possibly) asynchronous operations designated by *unary-expression* are executed.

Let the type of *unary-expression* be *T*. The value of the result of *async-expression* is the value of *unary-expression* after it has been wrapped into a [`^`*T*](#sec-Reactive-Types) object. The type of the result of *async-expression* is`^`*T*.

**Examples**

```
  result = async await taskA();
// ----------------------------------------------
  result = async {
    r1 = await task1();
    r2 = await task2();
    r1 + r2
  };
```

### Await Operator

**Syntax**

<pre>
  <i>await-expression:</i>
    await   <i>unary-expression</i>
</pre>

**Defined elsewhere**

* [*unary-expression*](#sec-Unary-Operators.General)

**Constraints**

An *await-expression* can only appear inside an asynchronous context (such as an [*async-expression*](#sec-Async-Operator) or an async function or method).

*unary-expression* must have a [reactive type](#sec-Reactive-Types).

**Semantics**

`await` suspends the execution of an async function until the result of the asynchronous operation represented by *unary-expression* is available. See [§§](#sec-Asynchronous-Functions) for more information.

The resulting value is the value of type *T* that was wrapped in the object of type `^`*T* returned from the async function. Consider the following:

```
async fun f(): ^Int {
  …
  result = …;
  result
}
```

The expression `f()` has type `^Int`, and the expression `await f()` has type `Int`.

**Examples**

```
  result = async await taskA();
// ----------------------------------------------
  result = async {
    r1 = await task1();
    r2 = await task2();
    r1 + r2
  };
```

### Mutable Operator

**Syntax**

<pre>
  <i>mutable-expression:</i>
    mutable   <i>function-call-expression</i>
</pre>

**Defined elsewhere**

* [*function-call-expression*](#sec-Function-Call-Operator)

**Constraints**

*function-call-expression* must be a call to a constructor for a mutable type.

**Semantics**

The result is a mutable instance of the object constructed.

**Examples**

```
class C(mutable value: Int) { … }
v = mutable C(1);
```

## Multiplicative Operators

**Syntax**

<pre>
  <i>multiplicative-expression:</i>
    <i>unary-op-expression</i>
    <i>multiplicative-expression</i>   *   <i>unary-op-expression</i>
    <i>multiplicative-expression</i>   /   <i>unary-op-expression</i>
    <i>multiplicative-expression</i>   %   <i>unary-op-expression</i>
</pre>

**Defined elsewhere**

* [*unary-op-expression*](#sec-Unary-Arithmetic-Operators)

**Semantics**

The binary `*` operator produces the product of its operands.

The binary `/` operator produces the quotient from dividing the left-hand operand by the right-hand one.

The binary `%` operator produces the remainder from dividing the left-hand operand by the right-hand one.

For the binary operators `*`, `/`, and `%` with operands of type `Int`, see class [`Int`](RTL-type-Int).

For the binary operators `*` and `/` with operands of type `Float`, see class [`Float`](RTL-type-Float).

These operators are left-associative.

Integer division by zero results in an exception of type [`DivisionByZeroException`](RTL-type-DivisionByZeroException).

**Examples**

```
-10 * 100		// Int result with value -1000
100.0 * -3.4e10		// Float result with value -3400000000000.0
100 / 100		// Int result with value 1
100.0 / 123.0		// Float result with value 0.8130081300813
123 % 100		// Int result with value 23
```

## Additive Operators

**Syntax**

<pre>
  <i>additive-expression:</i>
    <i>multiplicative-expression</i>
    <i>additive-expression</i>  +  <i>multiplicative-expression</i>
    <i>additive-expression</i>  -  <i>multiplicative-expression</i>
</pre>

**Defined elsewhere**

* [*multiplicative-expression*](#sec-Multiplicative-Operators)

**Semantics**

For operands of type `Int` and `Float`, the binary `+` operator produces the sum of its operands, while the binary `- `operator produces the difference of its operands when subtracting the right-hand operand from the left-hand one.

For the binary operators `+` and `-` with operands of type

* `Int`, see class [`Int`](RTL-type-Int).
* `Float`, see class [`Float`](RTL-type-Float).

For operands of type `String`, the binary `+` operator creates a String that is the concatenation of the left-hand operand and the right-hand operand, in that order. See class [`String`](RTL-type-String).

These operators are left-associative.

**Examples**

```
-10 + 100                  // Int result with value 90
100.0 + -3.4e10            // Float result with value -33999999900.0
100 – 123                  // Int result with value 23
"X" + 10.toString() + "Y"  // String result with value "X10Y"
```

## Bitwise Shift Operators

The `Int` class implements these as named methods. See [`Int.shl`](RTL-type-Int), [`Int.shr`](RTL-type-Int) and [`Int.ushr`](RTL-type-Int).

## Relational Operators

**Syntax**

<pre>
  <i>relational-expression:</i>
    <i>additive-expression</i>
    <i>relational-expression</i>  &lt;   <i>additive-expression</i>
    <i>relational-expression</i>  &gt;   <i>additive-expression</i>
    <i>relational-expression</i>  &lt;=  <i>additive-expression</i>
    <i>relational-expression</i>  &gt;=  <i>additive-expression</i>
</pre>

**Defined elsewhere**

* [*additive-expression*](#sec-Additive-Operators)

**Semantics**

Operator `<` represents *less-than*, operator `>` represents *greater-than*, operator `<=` represents *less-than-or-equal-to*, and operator `>=` represents *greater-than-or-equal-to*.

For the binary operators `<=`, `<`, `>=`, and `>` with operands of type

* `Bool`, see class [`Bool`](RTL-type-Bool)
* `Char`, see class [`Char`](RTL-type-Char)
* `Int`, see class [`Int`](RTL-type-Int)
* `Float`, see class [`Float`](RTL-type-Float)

## Equality Operators

**Syntax**

<pre>
  <i>equality-expression:</i>
    <i>relational-expression</i>
    <i>equality-expression</i>  ==  <i>relational-expression</i>
    <i>equality-expression</i>  !=  <i>relational-expression</i>
</pre>

**Defined elsewhere**

* [*relational-expression*](#sec-Relational-Operators)

**Semantics**

Operator `==` represents *value-equality*, while operator `!=` represents *value-inequality*.

For the binary operators `==` and `!=` with operands of type

* `Bool`, see class [`Bool`](RTL-type-Bool)
* `Char`, see class [`Char`](RTL-type-Char)
* `Int`, see class [`Int`](RTL-type-Int)
* `Float`, see class [`Float`](RTL-type-Float)
* [*class-literal*](#sec-Class-Literals), see class [`Class`](RTL-type-Class)

These operators are left-associative.

**Examples**

```
10 != 10		// Bool result with value false
if (flag == true) …
```

## Bitwise AND Operator

The `Int` class implements these as named methods. See [`Int.and`](RTL-type-Int).

## Bitwise Exclusive OR Operator

The `Int` class implements these as named methods. See [`Int.xor`](RTL-type-Int).

## Bitwise Inclusive OR Operator

The `Int` class implements these as named methods. See [`Int.or`](RTL-type-Int).

## Logical AND Operator

**Syntax**

<pre>
  <i>logical-AND-expression:</i>
    <i>equality-expression</i>
    <i>logical-AND-expression</i>   &amp;&amp;   <i>equality-expression</i>
</pre>

**Defined elsewhere**

* [*equality-expression*](#sec-Equality-Operators)

**Constraints**

Both operands must have `Bool` type.

This operator cannot be defined for a user-defined type.

**Semantics**

Given the expression `e1 && e2, e1` is evaluated first. If `e1` is `false`, `e2` is not evaluated, and the result has type `Bool`, value `false`. Otherwise, `e2` is evaluated. If `e2` is `false`, the result has type `Bool`, value `false`; otherwise, it has type `Bool`, value `true`. There is a sequence point after the evaluation of `e1`.

This operator is left-associative.

**Examples**

```
if (month > 1 && month <= 12) …
```

## Logical Inclusive OR Operator

**Syntax**

<pre>
  <i>logical-inc-OR-expression:</i>
    <i>logical-AND-expression</i>
    <i>logical-inc-OR-expression</i>   ||   <i>logical-AND-expression</i>
</pre>

**Defined elsewhere**

* [*logical-AND-expression*](#sec-Logical-AND-Operator)

**Constraints**

Both operands must have `Bool` type.

This operator cannot be defined for a user-defined type.

**Semantics**

Given the expression `e1 || e2`, `e1` is evaluated first. If `e1` is true, `e2` is not evaluated, and the result has type `Bool`, value `true`. Otherwise, `e2` is evaluated. If `e2` is `true`, the result has type `Bool`, value `true`; otherwise, it has type `Bool`, value `false`. There is a sequence point after the evaluation of `e1`.

This operator is left-associative.

**Examples**

```
if (month < 1 || month > 12) …
```

## Mutation Operators

### General

**Syntax**

<pre>
  <i>expression:</i>
    <i>logical-inc-OR-expression </i>
    <i>simple-mutation-expression</i>
    <i>compound-mutation-expression</i>
</pre>

**Defined elsewhere**

* [*compound-mutation-expression*](#sec-Compound-Mutation)
* [*logical-inc-OR-expression*](#sec-Logical-Inclusive-OR-Operator)
* [*simple-mutation-expression*](#sec-Simple-Mutation)

**Constraints**

These operators cannot be defined for a user-defined type.

**Semantics**

There are two kinds of mutation operators: simple and compound. Each is discussed in subsequent sections.

These operators are non-associative.

### Simple Mutation

**Syntax**

<pre>
  <i>simple-mutation-expression:</i>
    <i>mutation-target</i>   =   <i>expression</i>

  <i>mutation-target:</i>
    <i>bind-mutation-target</i>
    <i>tuple-mutation-target</i>
    <i>collection-set-target</i>
    <i>collection-append-target</i>
    <i>assign-target</i>

  <i>bind-mutation-target:</i>
    <i>nontype-identifier</i>   <i>type-specification<sub>opt</sub></i>

  <i>tuple-mutation-target: </i>
    (   <i>tuple-mutation-target-list</i>   )

  <i>tuple-mutation-target-list:</i>
    <i>tuple-mutation-target</i>   ,<i><sub>opt</sub></i>
    <i>tuple-mutation-target</i>   ,   <i>tuple-mutation-target-list</i>

  <i>collection-set-target: </i>
    <i>expression</i>   !   [   <i>expression</i>   ]

  <i>collection-append-target: </i>
    <i>expression</i>   !   [   ]

  <i>assign-target: </i>
    <i>local-variable-assign-target</i>
    <i>field-assign-target</i>
    <i>with-assign-target</i>
    <i>collection-member-assign-target</i>
    <i>collection-append-assign-target</i>

  <i>local-variable-assign-target</i>
    !   <i>nontype-identifier</i>

  <i>field-assign-target:</i>
    <i>expression</i>   .   !   <i>nontype-identifier</i>

  <i>with-assign-target</i>
    <i>assign-target</i>   .   <i>nontype-identifier</i>

  <i>collection-member-assign-target:</i>
    <i>assign-target</i>   [   <i>expression</i>   ]

  <i>collection-append-assign-target:</i>
    <i>assign-target</i>   [   ]
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*nontype-identifier*](#sec-Identifiers)
* [*type-specification*](types.md#sec-Types.General)

**Constraints**

Each mutation target being bound or assigned must designate a modifiable lvalue.

For *simple-mutation-expression*: When a mutation target is being assigned, the type of *expression* must be a subtype
of *mutation-target*’s type. In an assignment, the variable designated by *mutation-target* must already exist in the
current [scope](#sec-Scope). In a binding, that variable must not already exist in the current scope.

**TBD:** Under what circumstances is an explicit type needed on the target of a binding?

For *bind-mutation-target*: If *type-specification* is present, the type of *expression* in *simple-mutation-expression*
must be a subtype of *type-specifier*. A *nontype-identifier* that is a local variable that does not designate a parameter, and begins with `_` cannot be referenced again in the same scope as its binding.

> Note: A *nontype-identifier* that that designates a parameter is not subject to such spelling rules based on usage.

For *collection-set-target*: The mutable target must designate a mutable instance of an indexable-collection-class type
that supports [write-mset indexing operations](#sec-Indexable-Collections).

For *collection-append-target*: The mutable target must designate a mutable instance of an indexable-collection-class type that supports [write-mpushBack indexing operations](#sec-Indexable-Collections).

For *field-assign-target*: The mutable target must be a mutable instance of a class having an accessible field called
*nontype-identifier*.

For *with-assign-target*: The mutable target must be a mutable instance of a class having an accessible field called
*nontype-identifier*.

For *collection-member-assign-target*: The expression *assign-target* must designate an instance of an indexable-collection-class
type that supports [write-set indexing operations](#sec-Indexable-Collections).

For *collection-append-assign-target*: The expression *assign-target* must designate an instance of an indexable-collection-class
type that supports [write-pushBack indexing operations](#sec-Indexable-Collections).

**Semantics**

This operator computes one or more values (by evaluating *expression*) and then performs one or more of the following functions:

* Binds one or more ***mutation targets*** each to a corresponding computed value. (The meaning of “corresponding” is
given later.) For the purposes of this operator, the *nontype-identifier* `_` is *not* considered to be a mutation target;
see "Discards"  below.
* Assigns one or more mutation targets each to a corresponding computed value.
* Discards one or more of the computed values when the corresponding *nontype-identifier* is `_`.
* Modifies an indexable collection.

A *mutation-target* containing a `!` is an assignment; otherwise, it’s a binding.

The result has type `void`.

For *bind-mutation-target*: The mutation target designated by *nontype-Identifier* is bound to the value of *expression*
in *simple-mutation-expression*. During binding, a mutation target takes on a type. If *type-specification* is present,
that is the type of the mutation target. If *type-specification* is absent, the mutation target’s type is inferred from
*expression*. The type of a mutation target is fixed throughout that target’s life.

For *tuple-mutation-target*: Each mutation target in *tuple-mutation-target-list* is bound (if `!` is omitted) or assigned
(if `!` is present) to the lexically corresponding value in *expression*.

For *collection-set-target*: The semantics are unspecified. However, a [write-mset indexing operation](#sec-Indexable-Collections)
is performed, and *assign-target* is modified according to the semantics of `mset`.

For *collection-append-target*: The semantics are unspecified. However,
a [write-mpushBack indexing operation](#sec-Indexable-Collections) is performed, and *assign-target* is modified
according to the semantics of `mpushBack`.

For *local-variable-assign-target*: The mutation target designated by *nontype-Identifier* is assigned to the value of *expression*.
For *field-assign-target*: The field designated by *nontype-identifier*, in a mutable instance of a class designated
by *expression* is assigned to the value of *expression* in *simple-mutation-expression*.

For *with-assign-target*: The field designated by *nontype-identifier*, in a mutable instance of a class designated
by *expression* is assigned to the value of *expression* in *simple-mutation-expression*.

For *collection-member-assign-target*: The semantics are unspecified. However, a
[write-set indexing operation](#sec-Indexable-Collections) is performed, and *assign-target* is assigned to the result of the `set` method.

For *collection-append-assign-target*: The semantics are unspecified. However, a
[write-pushBack indexing operation](#sec-Indexable-Collections) is performed, and *assign-target* is assigned to the result of the `pushBack` method.

**Examples**

```
fun f(p: Int): void {
  x1 = 1;             // binding of x1 to I; type Int
  x1 = 10;            // Error: can't bind to an existing name
  x2: Int = 23;       // binding of x2 to Int 23
  x3: Bool = 2;       // Error: type mismatch, Bool vs. Int
  x4 = (10, true);    // binding of x4 to a tuple of type (Int, Bool)
  x5 = Null();        // type of x5 is Null<_>
  x6: ?Int = Null();  // type of x6 is ?Int
  p = 10;             // Error: name p is already bound

  _ = 1;              // discard the Int 1; no binding
  _: Int = 10         // discard the Int 10; no binding
  _: Bool = 2;        // Error: type mismatch
  _ = (10, true);     // discard the tuple; no binding
  …
  v1 = .IntKey;
  v2: Concrete<StringKey> = StringKey;
  …
}
// ----------------------------------------------
fun f(p: Int): void {
  !p = 100;           // assign existing name p
  x1 = 1;             // binding of x1 to I; type Int
  !x1 = 10;           // assign existing name x1
  !x1 = 12.34;        // Error: Can’t assign an Int variable to a Float value
  !x3 = 99;           // Error: can't assign to a non-existant variable

  !_ = 1;             // discard the Int 1; no asignment
  !_ = (10, true);    // discard the tuple; no asignment
  …
}
// ----------------------------------------------
tup1 = (10, "red", true);
(e1, _, e3) = tup1;
  // bind e1 to 10, and e3 to true; discard "red"

(!e1, _, !e3) = (50, "blue", false);
  // assign e1 to 50, and e3 to false; discard "blue"

((!e1, e4), (_, e5)) = ((-6, 99.5), (true, "abc"));
  // bind e4 to 99.5, and e5 to “abc”
  // assign e1 to -6, and discard true
// ----------------------------------------------
class MC(mutable x: String) { … }  // x is accessible in f
fun f(): void {
  mc = mutable MC("up");    // mc binds to a mutable object
  mc.!x = "down";           // field x is assigned to "down"
  mc.!x = 5;                // Error: types are incompatible
  mc.!_ = "right";          // Error: _ is not permitted in this context
  …
}
// ----------------------------------------------
class C(f: String)
class MC(mutable g: C)

v = mutable MC(C("Hello"));
v.!g.f = "World";		// field f is asigned
// ----------------------------------------------
class C(x: String) {
  fun get(idx: Int): String { … }
  fun set(idx: Int, val: String): C { … }
}

c = C("abc");
print_string("c[1] is " + c[1]);  // calls method get
!c[1] = "xx"; // calls method set, and assigns c to the new collection
// ----------------------------------------------
class C {
  fun pushBack(val: String): C { … }
}

c = C();
!c[] = "aaa";  // calls method pushBack, and assigns c to the new collection
// ----------------------------------------------
class C1(mutable x: String) { … }
  mutable fun mset(idx: Int, val: String): void { … }
}

v1 = mutable C1("red");     // binds v1 to a mutable collection
v1![0] = "green";           // calls method mset on that collection
// ----------------------------------------------
class C1(mutable x: String) {
  mutable fun mpushBack(val: String): void { … }
}

v1 = mutable C1("xxx");     // binds v1 to a mutable collection
v1![] = "yyy";              // calls method mpushBack on that collection
```

### Compound Mutation

**Syntax**

<pre>
  <i>compound-mutation-expression:</i>
    <i>unary-expression</i>   =.   <i>expression</i>
</pre>

**Defined elsewhere**

* [*expression*](#sec-Mutation-Operators.General)
* [*unary-expression*](#sec-Unary-Operators.General)

**Constraints**

*unary-expression* must designate a modifiable lvalue having a class type with a member as named by *expression*.

**Semantics**

An expression of the form `!`*e1* `=.` *e2* is equivalent `!`*e1* `=` *e1* `.` *e2*.

The result has type `void`.

**Examples**

```
// Generic class Ref has a public field called value
x = Ref(-42);                // x is bound to a Ref containing an Int

!x.value =. negate();         // short-hand version
!x.value = x.value.negate();  // long-hand version
```
