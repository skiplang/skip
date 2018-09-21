# Modules

## General

Each [*source-unit*](#sec-Program-Structure) is made up of a set of (possibly empty) [*declaration-list*s](#sec-Module-Declarations)  from one or more ***module***s. This set always includes the unnamed ***global module***, which consists of those *declaration-list*s not part of a ***named module***.

Any given module may have one or more ***module sections*** each of which is made up of a (possibly empty) *declaration-list*. The module sections for a module may be declared in any order. A module with multiple sections is treated as if all those sections were concatenated in lexical order.

At the start of each *declaration-list*, there is a ***current module***. If that *declaration-list* is inside a *named-module-declaration*, the current module is that named by *type-identifier* from that declaration; otherwise, the current module is the global module.

The names of entities declared in a module can be made private to that module or public to all modules.

A named module can also be known by one or more aliases.

Each module has its own [scope](#sec-Scope).

## Module Declarations

**Syntax**

<pre>
  <i>module-declaration:</i>
    <i>named-module-declaration</i>
    <i>module-alias-declaration</i>

  <i>named-module-declaration:</i>
    module   <i>type-identifier</i>   ;   <i>declaration-list<sub>opt</sub></i>   <i>module-end<sub>opt</sub></i>

  <i>module-end:</i>
    module   end   ;

  <i>module-alias-declaration:</i>
    module   alias   <i>type-identifier</i>   =   <i>type-identifier</i>
</pre>

**Defined elsewhere**

* [*declaration-list*](#sec-Program-Structure)

**Constraints**

*declaration-list* must not contain a *named-module-declaration*; that is, module declarations do not nest.

*module-end* is only optional for the global module and for the final module section in a *source-unit*.

The *declaration-list* in a *named-module-declaration* must not contain a *module-alias-declaration*.

The right-hand *type-identifier* in a *module-alias-declaration* must not be a module alias. That is, a module alias cannot itself be aliased.

A *module-alias-declaration* must precede all non-module-related [*declaration*s](#sec-Program-Structure) in a [*source-unit*](#sec-Program-Structure).

**Semantics**

A *named-module-declaration* introduces a [module section](#sec-Modules.General) for the module named *type-identifier*. A *named-module-declaration* ends at the following *module-end*, if any, or at the end of the parent *source-unit*.

Inside a module, names declared within that module can be used directly. Names can always be used by prefixing them with their parent module name followed by a period (`.`). In the case of the global module, the parent module name is omitted. Top-level public names having a prefix `.`, declared in a named module are promoted to the global module.

A *module-alias-declaration* declares the left-hand *type-identifier* to be an alias for the module designated by the right-hand *type-identifier*.

**Examples**

```
module alias Alias1 = Mod2;

// start out in the global module
fun f(): void { … }

module Mod1;      // declare a section for module Mod1
…
module end        // revert to the global module

module Mod2;      // declare a section for module Mod2

fun f(): void {
  .f();           // call global f()
  …
}
fun main(): Int { 123 }	// Mod2.main is just another function
private fun f3(): void { … }   // not callable from outside parent module

module end;       // revert to the global module

fun main(): void {	// program’s entry point
  f();            // calls global f()
  .f();           // uses explcit global module prefix to call global f()
  Mod2.f();
  print_string("Mod2.main returned " + Mod2.main().toString());
  Mod2.f();
  Mod2.f2();

  print_raw("Bye")
}

module Mod2;     // declare another section for module Mod2

fun f2(): void { .f(); … }	// calls global f()
                 // module section ends at EOF
```
