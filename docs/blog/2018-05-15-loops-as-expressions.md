---
title: Loops as Expressions
author: Todd Nowacki
---

Recently, loops were added to Skip!

In the past, we avoided adding loops under the assumption that higher-order functions could be used to write any code you would want in a nice, declarative manner. And if you really needed a loop, you could use a standard library function that provided mostly what you needed. Overtime, we found that it is very difficult to write certain performance critical algorithms without `break`, `return`, or `yield` from inside the loop.

## Making Loops Feel at Home with Expressions

If we had taken loops and implemented them exactly how you would expect in a statement-based language, they would not feel very cohesive with the rest of the language. To address this, Mat and I looked for someway that let's you use the loop directly, as you might with some of our other statement-like expressions such as if-else or try-catch.

The solution we arrived at was as follows:

### Break and Else

Loops in Skip are different than you might be used to in two key ways:
- **Break takes an expression** Instead of just `break;` we write `break e;` And if the break is hit, the value `e` is the resulting value of the entire loop.
- **Every Loop Has an Else** If the loop condition is not met, the else branch is taken as the resulting value of the loop. In other words, if a `break` (or some other divergent expression) the loop will either loop forever, or the else branch is the value. Like `if` expressions, if there is no explicitly written `else`, it has an implicit `else void`.

For clarity, two small examples:

```
while (true) break 100 else 0; // == 100
while (false) void else 42; // == 42
```

What's the motivation behind this design? Why not make loops more expression friendly in a different way?

Most iteration can be written with higher-order functions such as `map`, `filter`, and `fold`. The biggest weakness in the language without native loops was that there was no easy way to break early from "looping". Commonly, especially in the standard library, this was when some value had been found and looping wanted we wanted. For example
```
v : Vector<X>;
p : X -> Bool;
size = v.size();
i = 0;
result = None();
while_loop(
  () -> i < size && result matches None(),
  () -> {
    x = v[i];
    if (p(x)) !result = Some(x);
    !i = i + 1;
  },
);
result
```
can now be rewritten with native loops as:
```
v : Vector<X>;
p : X -> Bool;
result = None();
foreach (x in v) {
  if (p(x)) {
    !result = Some(x);
    break void;
  }
};
result
```
and then with a value-based `break` and `else` as:
```
v : Vector<X>;
p : X -> Bool;
foreach (x in v) {
  if (p(x)) break Some(x)
} else
  None()
```
while it might not be a huge difference in lines of code, with the expression-based loop you maintain this puzzle-piece-like feeling with your code. That is, this last version of the `foreach` loop can be moved around without having to preserve the context for these local side effects (the assignment).

To summarize
- A lot of patterns will continue to use standard library higher-order functions
- If you want to write code using a more iterative, statement-like style, you still can (you might just have to write `break void;` instead of `break;`
- The pattern that higher-order functions are bad at is supported by this style of expression-based loops

### Why the `else` keyword?

`else` might seems like an odd choice of keywords for this behavior. Here is a quick overview of why I went with this keyword over some other, new keyword
- Python has a similar behavior to this, but with no expression/value in `break`. Having a successful programming language as an existing data point is super helpful.
- `else` is already a keyword in a similar pattern with `if`. In particular, it shares similar behavior with `if-else` with having an implicit `else void` when the `else` branch is missing in the user's code.
- `else` fits logically with `while` and `do while`, and if you stretch it a bit for `foreach`, with "`else` is taken when the loop condition is false". Then `break` bypasses both the loop and the `else` branch.

That being said, this keyword isn't set in stone. We are in a bit of uncharted territory with this style of loops (at least among popular programming languages). With that in mind, we will try out the keyword, and if it doesn't feel like it is working, we will change it.

## The Three Loops

We added the three loops you might expect:
- `while`
- `do while`
- `foreach`

Each of these loops behaves as you might expect
- The condition of `while` and `do while` is of type `Bool`
- The body of each loop is of type `void`
- `break` and `continue` can only be used inside the loop body. Much like `throw` and `return`, `break` and `continue` have any type.
- `foreach` is desugared into a `do-while`. Examples in the sections below
- As described above, `break` takes an expression which if taken is the value of the loop. `else` is taken when the loop finishes without breaking.

### foreach Syntax

The syntax for the `foreach` is as follows:
```
foreach (<foreach_value> in <expression>) <expression> else <expression>
```
we currently support three "patterns" for the value bindings of the `foreach`.
- *Identifier*: Just a simple variable binding, e.g. `x`
- *Tuple of Identifiers*: A "tuple" of variable bindings, e.g. `(x, y, z)`. Currently these tuples cannot be nested.
- *Key-value*: A key value binding is two variable bindings separated by a fat arrow, e.g. `k => v`

In the future, we will support a more-general, irrefutable-pattern structure for these bindings, which will help bring them inline with any other lvalue.

### foreach Semantics

`foreach` uses the standard library's `Iterator` framework.
- *Identifier* or *Tuple of Identifiers* loops call `values()` on the collection
- *Key-value* loops call `items()` on the collection. It assumes that each element in iteration is a pair of the key and the value
- `next()` is called, looping while `Some` and stopping when `None()`
- The value of `next` is then matched against inside a `do while`

Here are two examples of
```
foreach ((x, y, z) in collection) <body> else <else_expr>
```
becomes
```
flag = true;
values = collection.values();
do
  values.next() match {
  | None() ->
    !flag = false;
    // A continue is added to prevent type errors with the other branch
    continue
  | Some((x, y, z)) ->
    <body>
  }
while (flag) else <else_expr>
```
and
```
foreach (k => v in collection) <body>
```
becomes
```
flag = true;
items = collection.items();
do
  items.next() match {
  | None() ->
    !flag = false;
    continue
  | Some((x, y, z)) ->
    <body>
  }
while (flag) else <else_expr>
```

## Summary

- Skip now has loops! They are `while`, `do while`, and `foreach`
- To make them feel more like expressions, `break` takes an expression and is the resulting value of the loop. If no break is reached, it takes the `else` branch.
- Our approach to loops as expressions fits nicely into the weak spot of higher-order functions, while not interfering with traditional "statement" usages of loops.
- `foreach` desugars into a `do while`, calling either `values()` or `items()` on the given collection. `next()` is called until a `None()` is reached.

And thanks to Mat for coming up with the break/else design and helping work out the kinks of the final implementation!
