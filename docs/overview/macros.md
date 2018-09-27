---
id: macros
title: Macros
---

Skip has a limited form of macros. Macros are methods defined in base classes or traits which are expanded in every leaf class which inherits the macro method. They are particularly handy for default implementations of common traits.

For example:

```
trait Hashable {
  overridable macro fun hash(): Int {
    h = #thisClassName.hash();
    #forEachField (#field) !h = combine(h, this.#field);
    h
  }
}

class Person{name: String, age: Int} uses Hashable {
  // NOTE: No hash() implementation required.
}
```

In the above example the expanded implementation of Person.hash() is:

```
  fun hash(): Int {
    h = "Person".hash();
    {
      !h = combine(h, this.name);
      !h = combine(h, this.age);
    };
    h
  }
```

Also note that the `"Person".hash()` expression is evaluated at compile time initializing `h` to an integer constant.

Within a `macro` method, the following macros are available:

- `#thisClassName`: Expands to a string literal containing the fully qualified name of the leaf class the method is being expanded into. It may be used as an expression.

- `#ThisClass`: Expands to the class type of the class being expanded into. It may be used as a type annotation as well as in pattern matching.

- `#forEachField(#fieldIdentifier [, #fieldNameLiteral]) body`: `#forEachField` expands to an expression sequence where the `body` expression is expanded once for each field in the containing class. The expansion includes all fields in the class including those inherited from base classes.

   Within `body` the `#fieldIdentifier` macro expands to an identifier equal to the name of the field being expanded. It may be used as an expression, or more commonly on the right hand side of the `.` member access operator.

   If present, the `#fieldNameLiteral` macro is also available in the `body` expression and expands to a string literal containing the name of the field being expanded.

## Current Uses of Macros

Macros currently provide default implementations for the following traits:

- Hashable
- Equality
- Orderable

You should rarely need to provide manual implementations of these traits.

## Equality Example

The `Equality` implementation shows the use of the `#ThisClass` macro used in a pattern match:

```
trait Equality {
  overridable macro readonly fun ==(other: inst): Bool {
    other match {
    | #ThisClass _ as otherTyped ->
      // silence unused variable warning in classes
      // with no fields.
      _ = otherTyped;

      #forEachField (#field) {
        if (this.#field != otherTyped.#field) {
          return false;
        }
      };
      true

    | _ -> false
    }
  }
}
```

## JSON Example

Here's an example using the field literal version of `#forEachField` to implement JSON serialization:

```
trait Jsonable {
  overridable macro fun toJson(): JSON.Value {
    fields = mutable Map<String, JSON.Value>[];
    #forEachField (#field, #fieldName) {
      this.#field.toJson() match {
      | JSON.Null() -> void
      | value -> fields.set(#fieldName, value)
      }
    };

    JSON.Object(freeze(fields))
  }
}
```
