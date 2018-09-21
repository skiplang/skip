---
id: control_flow
title: Control Flow
---

Skip includes the usual control flow constructs including `if`, `for/in`, `while`,
`do` and `loop`. Unlike most languages, Control flow constructs in Skip are expressions
and they produce values just like other expressions. Control flow expressions may be
used in any context an expression is expected.

## If-else

`if/else` evaluate one of 2 possible expressions.

```
fun maybeAdd1(condition: Bool, x: Int): Int {
  y = (if (condition) 1 else 0);
  x + y
}
```

The `else` clause may be omitted, in which case the result of the `if` expression
must be an expression of `void` type.

```
fun maybeAdd1(condition: Bool, x: Int): Int {
  y = 0;
  if (condition) {
    !y = 1
  };
  x + y
}
```

## Loops

The `for/in` expression enables iteration over all of the elements of a collection
or sequence.

```
fun findMax(values: Sequence<Int>): Int {
  max = Int.min;
  for (value in values) {
    if (value > max) {
      !max = value
    }
  };
  max
}
```

Within the body of a `for/in` expression, a `continue` expression terminates the current
iteration and advances to the next iteration through the loop body. A `break`
expression within the body of a `for/in` expression terminates the iteration. The
`break` expression includes an argument expression. The result of a `for/in` expression
which terminates due to a `break` results in the value of the `break` argument; the
value of a `for/in` expression which terminates by completing the entire iteration
is either `void` or the result of the `else` clause on the `for/in` expression:

```
fun getAge(name: String, people: Sequence<Person>): Int {
  for (person in people) {
    if (person.name == name) {
      break person.age
    }
  } else -1 // Return -1 if the person is not found.
}
```

Similarly, `while` and `do` loops may include an `else` clause:

```
fun getAgeWhile(name: String, people: Sequence<Person>): Int {
  iter = people.values();
  current = iter.next();
  while (current.isSome()) {
    person = current.fromSome();
    if (person.name == name) {
      break person.age
    }
    !current = iter.next();
  } else -1 // Return -1 if the person is not found.
}
```

The `loop` expression loops forever:

```
fun getAgeLoop(name: String, people: Sequence<Person>): Int {
  iter = people.values();
  loop {
    current = iter.next();
    if (current.isNone()) {
      break -1
    };
    person = current.fromSome();
    if (person.name == name) {
      break person.age
    }
  }
}
```
