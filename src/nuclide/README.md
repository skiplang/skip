# Nuclide Support

This directory contains binaries used to provide Skip language support for
the Nuclide IDE. Nuclide integration includes 2 options: separate binaries for each
operation, or LSP server support. The separate binary option is currently more
stable (on the skip side) but provides a poorer experience. The LSP option
provides a much more responsive experience, but currently crashes on occasion.

## Separate Binaries

The Separate binaries are:
- *skip_check_json*: produces diagnostics(both parse errors and type errors) for a
single source file.
- *skip_format*: formats a single source file.
- *skip_outline*: produces an outline(in JSON format) for a Skip source file.
- *skip_get_definition*: gets the location of the symbol at a given file/position

These binaries will all get decommissioned once *skip_lsp* is solid enough to become
the default editor support.

## Skip LSP Support

This is the way forwards. The *skip_lsp* binary implements language services in a
server process which conforms to Microsoft's Language Server Protocol(LSP) (https://github.com/Microsoft/language-server-protocol/blob/gh-pages/specification.md).

Nuclide includes some extensions to this protocol which are being folded into the
public standard. *skip_lsp* uses Nuclide specific extensions to the LSP protocol.
In general these extensions do not cause incompatibility with other LSP
clients (aka VS Code).

### Process Model

Each open `skip.project.json` (roughly corresponds to a directory of `.sk` files)
causes Nuclide to start a new *skip_lsp* server process. Once the last source file
in a project is closed then Nuclide will shutdown the *skip_lsp* process.

There are cases where Nuclide will fail to shutdown open *skip_lsp* processes.
You may need to do a `killall skip_lsp` every once in a while to clean up zombie
processes.

Nuclide will aggressively restart *skip_lsp* processes when they crash.

### Debugging Skip LSP Support

*skip_lsp* is a stateful server which makes debugging a pain. There are a couple
of ways to debug *skip_lsp* issues:
- logging: Each *skip_lsp* process opens a log file named `/tmp/skip_lsp__<project-path>.##`.
After a *skip_lsp* crash, look at the second to last one for a repro of the messages
which caused the crash. The most recent log for a given project will typically be
the log of the process that Nuclide restarted after the crash.
- Nuclide console: The *skip_lsp* process logs messsages to the Nuclide console.
The Nuclide console can be opened with Menu - View - Toggle Console (Cmd-Opt-J).
Top level status messages, including the stderr of the *skip_lsp* processes will
show up here. Note that Nuclide will often be connected to several *skip_lsp* servers
simultaneously, so the messages from the various process will be interleaved. Nuclide
console messages will be prefixed with the project path of the process. NOTE: The
Nuclide console should not be confused with the Atom developer console (Cmd-Opt-i).


## Other Binaries

- *skip_server*: A prototype version of Skip's incremental type checker.
- *skip_check*: A version of *skip_check_json* which type checks a single file and
writes the errors in human readable (not JSON) format on stdout. Useful for legacy
editors like vim or emacs.
