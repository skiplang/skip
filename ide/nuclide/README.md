# Nuclide Support For Skip

Nuclide support for Skip includes:
- syntax highlighting
- diagnostics
- outline view

Nuclide integration requires building the nuclide target and configuring your Nuclide settings for Skip.

## Building Nuclide Support

Nuclide support requires the following targets:
- `build/bin/skip_check`
- `build/bin/nuclide/skip_outline`
- `build/bin/outline.js`
- `build/bin/nuclide/skip_get_definition`
- `build/bin/skip_get_definition.js`

The nuclide targets are built by default with `ninja`. You can build the nuclide targets explicitly with:

```
skip$ cd build
build$ ninja nuclide
```

## Configuring Nuclide for Skip Support

To configure Nuclide for Skip:

- open the Nuclide settings with `option-cmd-,`
- Set `Language Settings`-`Skip`-`Directory of Skip Binaries` to `skip/build/bin`
  - Set the local dir when Nuclide is running on the same machine as your Skip dir (aka running on a laptop)
  - Set the remote dir when your Skip dir is remote
  - Set the `Filter` (at the top of the Nuclide settings pane) to `Skip` to quickly find all Skip related settings

After changing your Nuclide settings, restart Nuclide with `ctrl-option-cmd-L` to have them take effect.

## Describing Your Project to Nuclide

When Nuclide checks your Skip code for errors it needs to know all the Sk code to check.
This project description is described in a `.skipconfig` file.

### Finding the .skipconfig File

When checking a file for errors, Nuclide searches the directory containing the `.sk` file
being checked for a file named `.skipconfig`. If it doesn't find it in the same directory
as the source being checked, then it searches in the parent directory recursively.

If no `.skipconfig` file is found, then Nuclide will be unable to check your `.sk` file for
errors.

### Format of .skipconfig

A `.skipconfig` file is a text file which describes a set of Skip source files which are compiled together.

By default, an empty `.skipconfig` file includes all `.sk` files in that directory (and its children)
in the project.

### [include]

Files outside the directory containing the `.skipconfig` can be included in the project
by listing them in an `[include]` section. Lines in an `[include]` specify paths, either
files or directories, relative to the directory containing the `.skipconfig`.
Directories mentioned in an `[include]` section include all `.sk` files in the
specified directory recursively.

Typically you will need to include the prelude code. For example, your `.skipconfig` might look like:

```
[include]
../prelude
```

### [ignore]

To ignore files within the current directory, or within directories explicitly added in an
`[include]` section, add an `[ignore]` section to your `.skipconfig`.

Typically, you will want to exclude test code from your `.skipconfig`.
For example:

```
[include]
../prelude

[ignore]
../prelude/__tests__
../prelude/php/__tests__
tests
```

## Guidelines for Organizing Your Skip Projects

When organizing your skip projects into a directory hierarchy, organize your
code into libraries, and programs.

### Libraries

Libraries are sets of `.sk` files which can be checked together as a unit, and do not
include a `main` function. Libraries can reference other libraries. Libraries may not
reference programs. The graph of references between libraries forms an acyclic graph.

Each library must have a distinct directory where that library's `.skipconfig` file
is placed. All source code in a library must be in the library's directory, or a child
directory of the library's directory. Referenced libraries should be in peer directories
of the library's directory, not in a parent or child directory of the referencing library.

The `.skipconfig` for a library `[include]`'s the directories for all referenced libraries
and `[ignore]`'s all tests (and test programs) in the current library and all
referenced libraries.

### Programs

Programs are sets of `.sk` files which describe an executable program. A program
includes a single definition of the `main` function. Programs can reference libraries.
Programs may not be referenced by other libraries or programs.

Each program should have a directory distinct from all other programs and libraries.
Ideally, each program would be in a peer directory to other libraries and programs
rather than a parent or child of other libraries or programs.

You may nest a program directory within a library directory, though that is discouraged.
When nesting a program directory within a library directory, that program must be
`[ignore]`ed from the containing library and all libraries and programs which reference
the containing library.

The `.skipconfig` for a library `[include]`'s the directories for all referenced libraries
and `[ignore]`'s all tests (and test programs) in the current library and all
referenced libraries.

### Test Programs

The test programs and libraries for a library are typically put in a sub-directory named
`tests` or `__tests__`.

For simple `tests` which have a single `main`, the `tests` directory can be treated as a
project containing a program which `[include]`'s the library in its parent directory.

For more complex tests, the `tests` should be organized into sub-directores,
each containing a test library, or test program.

### Libraries With Many Small Programs

A small program is one where the program specific code is contained in a single
`.sk` file. For a library with many small programs, the code for the small programs
may be included in the library directory and added to the `[ignore]` section of
the library.

With this structure, editing library code will show diagnostics for the shared library
code only. When editing one of the program files, diagnostics for the program plus the
contained library will be shown. Effectively, opening a file in Nuclide will override
it being listed in the `[ignore]` section of the containing `.skipconfig`.

Note that libraries referencing a library containing program code will need to `[ignore]`
all of the program code in the referencing `.skipconfig`.
