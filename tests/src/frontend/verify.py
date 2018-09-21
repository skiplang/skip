# @lint-avoid-pyflakes2
# Mostly copied from hphp/hack/test/verify.py

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import argparse
import os.path
import pipes
import subprocess
import sys
import re
import difflib
import glob
from collections import namedtuple
from concurrent.futures import ThreadPoolExecutor

max_workers = 48
verbose = False
dump_on_failure = False

Failure = namedtuple('Failure', ['fname', 'expected', 'output'])

def run_test_program(files, program, lib, expect_ext):
    """
    Run the program and return a list of Failures.
    """
    base_cmd = [program]
    base_cmd.extend(glob.glob(lib + '/*.sk'))
    def run(f):
        test_dir, test_name = os.path.split(f)
        cmd = base_cmd[:]
        cmd.append(test_name)
        if verbose:
            print('Executing', ' '.join(map(pipes.quote, cmd)))
        try:
            output = subprocess.check_output(
                cmd,
                stderr=subprocess.STDOUT,
                cwd=test_dir,
                universal_newlines=True,
            )
        except subprocess.CalledProcessError as e:
            # we don't care about nonzero exit codes... for instance, type
            # errors cause hh_single_type_check to produce them
            output = e.output
        return check_result(
            f,
            expect_ext,
            output.replace(os.path.abspath('skip/'), "<<root>>").rstrip()
        )

    executor = ThreadPoolExecutor(max_workers=max_workers)
    futures = [executor.submit(run, f) for f in files]

    results = [f.result() for f in futures]
    return [r for r in results if r is not None]

def check_result(fname, expect_exp, out):
    try:
        with open(re.sub('\.sk$', expect_exp, fname), 'rt') as fexp:
            exp = fexp.read().rstrip()
    except FileNotFoundError:
        exp = ''
    if exp != out:
        return Failure(fname=fname, expected=exp, output=out)

def get_diff(failure):
    expected = failure.expected.splitlines(1)
    actual = failure.output.splitlines(1)
    return difflib.ndiff(expected, actual)

def record_failures(failures, out_ext, diff_ext):
    for failure in failures:
        with open(re.sub('\.sk$', out_ext, failure.fname), 'wb') as f:
            f.write(bytes(failure.output, 'UTF-8'))
        with open(re.sub('\.sk$', diff_ext, failure.fname), 'wb') as f:
            f.write(bytes(''.join(get_diff(failure)), 'UTF-8'))


def dump_failures(failures):
    for f in failures:
        expected = f.expected
        actual = f.output
        diff = get_diff(f)
        print("Details for the failed test %s:" % f.fname)
        print("\n>>>>>  Expected output  >>>>>>\n")
        print(expected)
        print("\n=====   Actual output   ======\n")
        print(actual)
        print("\n<<<<< End Actual output <<<<<<<")
        print("\n>>>>>       Diff        >>>>>>>\n")
        print(''.join(diff))
        print("\n<<<<<     End Diff      <<<<<<<\n")

def files_with_ext(files, ext):
    """
    Returns the set of filenames in :files that end in :ext
    """
    result = set()
    for f in files:
        prefix, suffix = os.path.splitext(f)
        if suffix == ext:
            result.add(prefix)
    return result

def list_test_files(root, disabled_ext):
    if os.path.isfile(root):
        if root.endswith('.sk'):
            return [root]
        else:
            return []
    elif os.path.isdir(root):
        result = []
        children = os.listdir(root)
        disabled = files_with_ext(children, disabled_ext)
        for child in children:
            if child == 'disabled' or child == 'failing' or child == 'todo':
                continue
            if child in disabled:
                continue
            result.extend(
                list_test_files(os.path.join(root, child), disabled_ext),
            )
        return result
    elif os.path.islink(root):
        # Some editors create broken symlinks as part of their locking scheme,
        # so ignore those.
        return []
    else:
        raise Exception(
            'Could not find test file or directory at %s' %
            args.test_path
        )

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'test_path',
        help='A file or a directory. ',
    )
    parser.add_argument('--program', type=os.path.abspath)
    parser.add_argument(
        '--native-lib-dir',
        type=os.path.abspath,
        default=os.path.abspath('skip/prelude/'),
    )
    parser.add_argument('--out-extension', type=str, default='.out')
    parser.add_argument('--expect-extension', type=str, default='.exp')
    parser.add_argument('--diff-extension', type=str, default='.diff')
    parser.add_argument(
        '--disabled-extension',
        type=str,
        default='.no_typecheck'
    )
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('--max-workers', type=int, default='48')
    parser.add_argument(
        '--diff',
        action='store_true',
        help='On test failure, show the content of the files and a diff',
    )
    args = parser.parse_args()

    max_workers = args.max_workers
    verbose = args.verbose
    dump_on_failure = args.diff

    if not os.path.isfile(args.program):
        raise Exception('Could not find program at %s' % args.program)

    files = list_test_files(args.test_path, args.disabled_extension)
    lib = args.native_lib_dir
    failures = run_test_program(files, args.program, lib, args.expect_extension)
    total = len(files)
    if failures != []:
        record_failures(failures, args.out_extension, args.diff_extension)
        fnames = [failure.fname for failure in failures]
        if dump_on_failure:
            dump_failures(failures)
        for fname in fnames:
            print("Failed: %s" % fname)
        if len(failures) > 1:
            print("Failed %d out of %d tests." % (len(failures), total))
        sys.exit(1)

    if total == 0:
        print("No tests given")
    elif total == 1:
        print("Passed: %s" % files[0])
    else:
        print("All %d tests passed!" % total)
