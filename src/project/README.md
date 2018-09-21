# Skip Project Files

Skip project files define

## Clients:
- IDE(Nuclide) services
- build systems
- humans

## Goals:
- Parsability - Use JSON rather than the current .ini style format.
- easily map from source file to minimum set of files it can be skip_check-ed with
- defines program units: libraries, executables, tests
- acyclic digraph of program units
- a source file may exist in more than one program unit. However,
  it is easy to map from a source file to every program unit that it is directly
  contained in; and every source file has a default program unit.
- a source file may not exist in more than one Project.
- a program unit can contain at most 1 main function
- TODO: Can project files be used for the CMake based build?
- TODO: Can project files be the basis of a Ski package manager?


## Syntax Backends

Identify a Program via a source file directly included in that program:

```
skip_to_llvm src/tools/skip_to_parsetree.sk
```

Identify a Program via a Project+ProgramUnit:

```
skip_to_llvm src/tools:skip_to_parsetree
```

Identify Program via the Project in which it is the default Program Unit.

```
skip_to_llvm src/tools
```

Identify Program via the Project in the current directory and the Program Unit name.

```
skip_to_llvm skip_to_parsetree
```

## Syntax Dependency Tracker

The dependency analyzer will output deps info. It will be used primarily by
build systems (CMake, Buck, etc.).

```
skip_depends --binding "backend=native" src/tools:skip_to_parsetree
```
