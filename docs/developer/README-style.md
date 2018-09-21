# General style guidelines for the Skip project

Skip and C++ are auto-formatted; details handled by formatting tools
are not covered here.

The more visibly scoped a symbol is, the more important style adherence is.

Honor the surrounding style in any file you edit, or add. It demonstrates
politeness, situation awareness, respect for existing work, and attention
to detail.

Break these rules when you have a good reason, but be prepared to explain it.

## Filenames

- ``underscore_case`` for scripts, source files, and directories.
  - The reason for directory name style, is that Python modules
  can't be in directories with hyphens since the ``import dir1.dir2``
  syntax wouldn't parse.
- ``UpperCase`` is acceptable for a source file when its contents are a
  type or module, e.g. ``Foo.h`` which defines ``struct Foo``.

## Skip

- UpperCase for types and modules
- ``camelCase`` for methods, top level functions, members, and locals.

## C++

- Always use ``struct`` vs ``class``
- Always use ``using`` vs ``typedef``
- ``UpperCase`` for ``struct`` and ``enum`` types
- ``m_`` prefix for member fields of structs with methods
  - no prefix for small POD structs
- ``tl_`` for thread locals
- ``s_`` for static members and globals
- ``g_`` for exported globals
- ``kUpperCase`` for constants at any scope
- Prefer ``CAPS_CASE`` *only* for C-preprocessor symbols
- Only use C-preprocessor macros for things only they can accomplish, such as
  symbol-to-string conversion, or symbol concatenation. Otherwise,
  use a constant, ``inline`` function, ``template``, ``using``, or whatever.
- Prefer anonymous namespace over static.
  - sometimes, static is a nice way to clarify file scope anyway, since an anon-
    namespace block can be very large.
- Prefer explicit namespace:: qualifiers over ``use namespace`` for well-known libraries, such as
  std, folly, and boost.
- Prefer explicit namespace qualifiers on skip classes that are API compatible with
  well known types, e.g. ``skip::unordered_map``
- Declarations exported from a module belong in the ``include`` directory, visible outside
  the module. Declarations only used within module, but shared between files in the module,
  belong in the module directory. Declarations only used in a single file belong in that file.

