#!/usr/bin/env python3


import argparse
import importlib
import logging
import os
import pipes
import subprocess
import sys

import common
import skip_native_compile


logger = logging.getLogger(os.path.basename(__file__))

build_dir = common.build_dir

default_annotation = "@test"

default_delegate_function = "test"

description = """
Run the unit tests for a project. Annotate test functions with `{annotation}` -
these functions will be collected and passed to `{delegate_function}()`,
a user-defined function that should execute all the tests:

Example:

    fun {delegate_function}(tests: Array<(String, () -> void)): void
"""


def build_main_file(
    stack,
    program,
    backend,
    annotation=default_annotation,
    delegate_function=default_delegate_function,
    rebuild=True,
):
    if rebuild:
        common.buildNinjaTarget("skip_collect_annotations")

    mainFilePath = stack.enter_context(common.tmpfile(suffix=".sk")).name
    with open(mainFilePath, "w") as mainFile:
        cmd = (
            os.path.join(build_dir, "bin/skip_collect_annotations"),
            "--binding",
            "backend=" + ("native" if backend == "native" else "nonnative"),
            "--annotation",
            annotation,
            "--delegate",
            delegate_function,
            program,
        )
        returncode = subprocess.call(
            cmd, env=os.environ, stdout=mainFile, stderr=subprocess.PIPE
        )
        if returncode != 0:
            print("Failed to load annotations - check the project file for `%s`." % (program))
            exit(1)
        mainFile.flush()

    return mainFilePath


def main(stack):
    remainder = common.splitRemainder()
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=description.format(
            annotation=default_annotation, delegate_function=default_delegate_function
        ),
        parents=[
            common.commonArguments(needsBackend=False),
            skip_native_compile.arguments(),
        ],
    )
    parser.add_argument(
        "program",
        type=str,
        help="The program for which tests should be run (project:unit)",
    )
    parser.add_argument("--backend", default=os.environ.get("BACKEND", "native"))
    parser.add_argument(
        "--timeout", type=int, default=os.environ.get("SKIP_TEST_TIMEOUT", "300")
    )
    parser.add_argument("--watch", default=False, action="store_true")

    args = common.parse_args(parser)
    mainFilePath = build_main_file(stack, args.program, args.backend)

    args.srcs = [args.program, mainFilePath]

    if args.backend == "native":
        binFile = skip_native_compile.compile(stack, args)
        cmd = (binFile.name,)
        if args.watch:
            cmd += ("--watch",)
    else:
        print("Uknown backend %s" % (args.backend))
        exit(2)

    cmd += tuple(remainder)
    logger.debug("Running: " + ' '.join(map(pipes.quote, cmd)))
    with common.PerfTimer("skip_native_exec.test_runtime"):
        res = subprocess.call(
            ("ulimit -t %d ; " % (args.timeout,)) + ' '.join(map(pipes.quote, cmd)),
            shell=True,
            env=os.environ,
        )
    if res != 0:
        sys.exit(res)


if __name__ == "__main__":
    with common.ExitStack() as stack:
        rc = main(stack)
    sys.exit(rc)
