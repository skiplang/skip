---
id: debugging
title: Debugging Skip Code
---

## Invariants

Invariants add checks to your code to verify that the program is in an expected
state.

```
invariant(x > 10, "Expected x > 10");
```

Unexpected control flow can be indicated by `invariant_violation`:

```
if (x > 10) {
  // do something
  ...
} else {
  invariant_violation("Expected x > 10");
}
```

When an `invariant_violation` call is encountered an `InvariantViolation` value
is thrown. Typically, thrown `InvariantViolation` exceptions will not be caught
and your program will terminate with an uncaught exception.

## Uncaught Exceptions

By default, when your program terminates with an uncaught exception a short
message is displayed and your program exits with a non-zero exit code.

```
fun main(): void {
  invariant_violation("Whoops!");
}
```

Yields:

```
Uncaught exception: Invariant violation: Whoops!

File "prelude/System.sk", line 97, characters 3-31:
File "t.sk", line 2, characters 3-32:
File "t.sk", line 1, characters 5-8:
```

The stack trace which is printed will vary greatly based on the
backend (JS, native).

### Stack Traces and Tests

Stack traces are disabled when running the compiler unit tests. For the JS
backend they are disabled with the `--no-unhandled-exception-stack` argument
to the JS execution scripts. Disabling it can be handy when debugging unit tests
failing in the JS backend.

## Print Style Debugging

The simplest form of debugging is to print to either stdout or stderr as your
program runs. Some functions to be aware of when debugging:

| Function | Description |
|----------|-------------|
| `print_raw` | Print a string to stdout |
| `print_string` | Print a string and a new line to stdout |
| `print_error` | Print a string to stderr |
| `debug` | Print any Skip value to stdout. NOTE: Will crash on cyclic object references. |
| `print_stack_trace` | Print the current stack trace to stdout and continue executing. |

## Command Line Debugging - Node Debugger

Command line debugging is available via the JS backend. To debug your skip
program with the node debugger:

- add calls to `debug_break()` at the locations in your Sk code where you would like
  to break.
- compile your `.sk` files to JS with `build/bin/skip_to_js`
- launch the node debugger with `tools/debug_js_file`
- the node debugger will break whenever a `debug_break()` statement is executed.
- Instructions for the node command line debugger are [here](https://nodejs.org/api/debugger.html)

This gives a simple command line interface. You will need to open up the converted
JS file to see the local variables available to view using the `repl` command.

Control flow is all based on the converted JS code.

## GUI Debugging - Chrome Dev Tools

I highly recommend the Chrome debugger.

- Make sure you are using a recent (v8.6.0 works great) version of node.
- add calls to `debug_break()` at the locations in your Sk code where you would like
  to break. Putting a `debug_break()` at the start of your `main()` function is
  a good start.
- compile your `.sk` files to JS with `build/bin/skip_to_js`
- launch your Sk program with: `tools/inspect_js_file sk.js`
- open the URL `chrome://inspect` in Chrome
- You should see an entry in the `Remote Target` section. Click on the `inspect`
  link.

Chrome Dev Tools supports GUI exploration of local variables which is nice.
Chrome Dev Tools now supports JS Source Maps. All control flow operations (Step In/Over/Out)
and call stacks are mapped back to the original .sk files.

Some Tips:

- Keeping the `chrome://inspect` tab open will save you some clicks when restarting
  the debuggee.
- Sk Files will show up in the `Sources` Tab under the `file://` tree node.
- Break points can be set by clicking the line number of an Sk file. Breakpoints
  are indicated by a blue arrow over the line number.
- Break points are remembered between sessions.
- Click on the call stack to inspect variables from other stack frames.
- Check out the `Profiler` tab for CPU and heap profiling.
- Hitting an `invariant_violation` will break into the debugger if one is
  attached before throwing.

## Memory Debugging

### (NOTE: This currently only works on the native backend)

From any point in the program you can call `debug_printMemoryStatistics()` and
it will print to stderr a report of the current memory usage.

You can also set the environment variable `SKIP_MEMSTATS` to a non-zero value
and a memory report will be printed at exit.
