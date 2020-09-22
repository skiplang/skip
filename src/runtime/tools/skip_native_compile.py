#!/usr/bin/env python3





import argparse
import logging
import os
import pipes
import shutil
import subprocess
import sys

import common

logger = logging.getLogger(os.path.basename(__file__))

binary_dir = common.binary_dir
build_dir = common.build_dir
source_dir = common.source_dir

def compile(stack, args):
    if not args.preamble:
        args.preamble = os.path.join(binary_dir, 'runtime/native/lib/preamble.ll')

    CC = common.cmakeCacheGet('CMAKE_CXX_COMPILER')
    CFLAGS = tuple(subprocess.check_output(
        ('pkg-config', '--cflags',
         os.path.join(binary_dir, 'runtime/native/native_cc.pc'))
    ).decode('utf8').strip().split(' '))
    LIBS = tuple(subprocess.check_output(
        ('pkg-config', '--libs',
         os.path.join(binary_dir, 'runtime/native/native_cc.pc'))
    ).decode('utf8').strip().split(' '))

    # Run skip_to_native to generate our .o file
    objFile = stack.enter_context(common.tmpfile('tmp.gen_object.', '.o'))

    SKFLAGS = tuple([x for x in [x for x in CFLAGS if x.startswith(('-m', '-f', '-W', '-g', '-O'))] if not x.startswith('-Wl,')])

    PROFILE_FLAGS = ('--profile', args.profile) if args.profile else ()

    EMBEDDED_FLAGS = ('--embedded64',) if args.embedded64 else ()

    PARALLEL_FLAGS = (('--parallel', str(args.parallel))
                      if args.parallel is not None else ())

    PRINT_SKIP_TO_LLVM = ('--print-skip-to-llvm',) if args.print_skip_to_llvm else ()

    cmd = (
        os.path.join(source_dir, 'runtime/tools/skip_to_native'),
        '--preamble', args.preamble,
        '--output', objFile.name,
        '--via-backend', args.via_backend,
        ) + PROFILE_FLAGS + EMBEDDED_FLAGS + PARALLEL_FLAGS + tuple(args.srcs) + SKFLAGS + PRINT_SKIP_TO_LLVM

    logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))
    common.callHelper(cmd)

    # do not continue compilation if we are just printing skip_to_llvm
    if args.print_skip_to_llvm:
        exit(0)

    # Compile the .o into the final binary
    binFile = stack.enter_context(common.tmpfile('tmp.gen_binary.', ''))
    binFile.close()

    # For each file that we're compiling look to see if it has an associated
    # .cpp file.  For a directory the associated .cpp is named testhelper.cpp.
    # For a file.sk the associated .cpp is named file_testhelper.cpp.
    def testCpp(x):
        if os.path.isdir(x):
            return os.path.join(x, 'testhelper.cpp')
        else:
            return os.path.splitext(x)[0] + '_testhelper.cpp'
    cppSrcs = tuple(
        [x for x in map(testCpp, args.srcs) if os.path.isfile(x)])
    sk_standalone = (
        args.sk_standalone or
        os.path.join(source_dir, 'runtime/native/src/sk_standalone.cpp'))
    cmd = (
        CC,
        '-o', binFile.name, '-g', sk_standalone,
        objFile.name) + cppSrcs + CFLAGS + LIBS
    logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))

    with common.PerfTimer('clang.runtime'):
        common.callHelper(cmd)

    common.logPerfData('binary_size', ['skip_compiler'], os.path.getsize(binFile.name))

    # if we want to create a named executable at the location of args.output
    if args.output:
        shutil.copy(os.path.join(source_dir, binFile.name), args.output)

    return binFile


def arguments():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--parallel', type=int,
                        help='How many LLVM processes to run in parallel')
    parser.add_argument('--preamble')
    parser.add_argument('--via-backend', default=os.environ.get('VIA_BACKEND', os.path.join(build_dir, 'bin')))
    parser.add_argument('-o', '--output')
    return parser


def main(stack):
    parser = argparse.ArgumentParser(
        description='Run the Skip compiler',
        parents=[common.commonArguments(needsBackend=False, backend='native'), arguments()])
    parser.add_argument('srcs', metavar='SOURCE', nargs='+')

    args = common.parse_args(parser)
    compile(stack, args)

if __name__ == '__main__':
    with common.ExitStack() as stack:
        rc = main(stack)
    sys.exit(rc)
