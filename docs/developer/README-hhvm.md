# General docs for HHVM interop.

HHVM interop is controlled through @hhvm_ annotations.  Functions can be
annotated to indicate HHVM functions callable from Skip or Skip functions
callable from HHVM.

Classes can be annotated to extend the method and type of data which can be
shared between the two runtimes.

## Loading Skip from within HHVM

TODO: Need to talk about building a Skip .so and the -vSkip.Binary setting.

## Portable Types

A "portable type" is one that can be transported to and from HHVM.

- Primitive types: `Bool`, `Int`, `Float`, `String`, `Object`.

- Proxy types: Classes annotated with `@hhvm_import` or `@hhvm_shape`.

- Copy types: Classes annotated with `@hhvm_copy` or `@hhvm_shape_copy`.

- Hack Arrays: The readonly types `Vector.HH_varray2`, `Map.HH_darray2`
  and `Set.HH_keyset2` (NOTE: names are temporary).

- HH.Mixed: HH.Mixed and all of its subtypes except `HH.Object` and
  `HH.Resource`

- Collections: Some Skip collections are portable.  They will be copied when
  transported.

  - Array<T> - Converts to `varray`.  `T` must be a portable type.

  - Vector<T> - Same rules as Array<T>

  - Map<Tk, Tv> - Converts to `darray`.  `Tk` must be either an `Int`, `String` or
    `HH.Arraykey`.  `Tv` must be a portable type.

  - UnorderedMap<Tk, Tv> - Same rules as Map<Tk, Tv>.

  - Set<Tk> - Converts to `keyset`.  `Tk` must be either an `Int`, `String` or
    `HH.Arraykey`.

  - UnorderedSet<Tk> - Same rules as Set<Tk>.

  - Tuple - Converts to `array`.  The types of each tuple element must be
    portable types.

- Options: An `Option<>` of any portable type other than Option<>.  `None()` in
  Skip will become `null` in HHVM and vice-versa.

## @hhvm_import functions

Annotating a function with @hhvm_import allows Skip code to call an HHVM
function.  Parameters and returns can be any portable type.

(T31307653: Right now Collection types are not supported for parameters or
returns).

```
@hhvm_import
native fun myPhpFunction(a: Int, b: String): Bool;
```

A parameter can be given to indicate the php function name being imported:

```
@hhvm_import("my_php_function")
native fun myPhpFunction(a: Int, b: String): Bool;
```

## @hhvm_export functions

Annotating a function with @hhvm_export makes it available to call from HHVM.
Parameters and returns can be any portable type.

(T31307653: Right now Collection types are not supported for parameters or
returns).

```
@hhvm_export
fun mySkipFunction(a: Int, b: String): Bool {
  ((b + a).length() > 17)
}
```

A parameter can be given to provide a name to export to HHVM:

```
@hhvm_export("my_php_function")
fun mySkipFunction(a: Int, b: String): Bool {
  ((b + a).length() > 17)
}
```

## @hhvm_import classes and @hhvm_shape classes.

A class can be annotated with @hhvm_import or @hhvm_shape to indicate that it is
proxy imported from HHVM.  The class should be nominal (defined with '{}').  All
fields should be portable types and any fields access on the class queries HHVM
to extract the value.

Classes annotated with @hhvm_import match up with normal PHP classes.  Classes
annotated with @hhvm_shape match up with PHP shapes.

Methods may also be defined on the class.  If the methods are annotated with
@hhvm_import then they will call methods on the HHVM object.  If the methods are
not annotated with @hhvm_import then they are only on the Skip definition.

A parameter can be given to provide a PHP classname:

```
@hhvm_import("MyPhpClassName")
class MyHhvmClass{
  fieldA: Int,
  fieldB: String
} {
  fun mySkipFunction(): A {
    this.fieldA + 7
  }

  @hhvm_import
  native fun myHhvmFunction(): Bool;
}
```

## @hhvm_copy and @hhvm_shape_copy

A class marked with @hhvm_copy or @hhvm_shape_copy will be copied as it is
passed to or from HHVM.  In Skip they're normal classes.

Like @hhvm_import, all fields must be portable types.

Classes annotated with @hhvm_copy match up with normal PHP classes.  Classes
annotated with @hhvm_shape_copy match up with PHP shapes.

## @hhvm_extern
### (mostly internal use)

@hhvm_extern works like a @cpp_extern with a few changes:

  1. The called name is name-mangled like a C++ template function.

  2. If the called function returns an @hhvm_import class then it is passed an
     HhvmImportFactory or HhvmArrayFactory as its first parameter.

  3. Any @hhvm_import classes passed to or returned from the function are passed
     as HhvmHandle or SkipRObj*.

## @hhvm_array
### (mostly internal use)

@hhvm_array works like @hhvm_import for HHVM arrays (copy-on-write vs reference
semantics).

  1. @hhvm_arrays must be concrete classes, although they can be parameterized.

  2. @hhvm_arrays may not have fields.
