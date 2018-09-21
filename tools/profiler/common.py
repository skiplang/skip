#!/usr/bin/env python
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import argparse
import contextlib
import errno
import logging
import os
import pipes
import subprocess
import sys
import tempfile
import time
import threading


logger = logging.getLogger(__name__)

root_dir = os.path.normpath(os.path.join(os.path.abspath(__file__), '../../..'))
runtime_tools_dir = os.path.join(root_dir, 'src/runtime/tools')
build_dir = os.path.join(root_dir, 'build')
source_dir = os.path.join(root_dir, 'tools')
tmp_dir = os.path.join(source_dir, 'profiler/tmp')

sys.path.append(source_dir)
import generate_project_json

def callHelper(cmd, env=None, **args):
    """Given a command which dumps its output to stdout, run it and if it fails
    dump the output to stderr instead.
    """
    if env:
        env = dict(os.environ.items() + env.items())
    else:
        env = os.environ

    try:
        logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))
        subprocess.check_call(cmd, env=env, **args)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


@contextlib.contextmanager
def tmpfile(delete=(os.environ.get('KEEP_TEMP', '') == ''), name=None):
    if not os.path.exists(tmp_dir):
        try:
            os.makedirs(tmp_dir)
        except OSError as exc: # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise
    if name:
        f = open(os.path.join(tmp_dir, name), 'w+')
    else:
        f = tempfile.NamedTemporaryFile(dir=tmp_dir, delete=delete)
    yield f
    if delete:
        os.unlink(f.name)


def validate_sk_unit(unit_path):
    cmd = ( 'ninja', '-C', build_dir, 'skip_depends' )
    callHelper(cmd)

    isSkFile = False

    base, ext = os.path.splitext(unit_path)
    if ext == '.sk':
        isSkFile = True

    try:
        dir, unit_name = unit_path.split(':')
        # if they provided a :programUnit then interpret it as a unit name and
        # not a source file
        isSkFile = False
    except ValueError:
        unit_name = os.path.basename(base)
        dir = os.path.dirname(base)

    dir = os.path.abspath(dir)
    source_path = os.path.relpath(unit_path, dir)

    cmd = (
        os.path.join(build_dir, 'bin/skip_depends'),
        '--binding', 'backend=' + os.environ.get("BACKEND", 'native'),
        dir + ':' + unit_name )
    returncode = subprocess.call(cmd, env=os.environ, stdout=subprocess.PIPE)
    if (returncode != 0):
        if not isSkFile:
            exit('Please provide either a .sk file or a program unit included in your skip.project.json file.')
        if raw_input("Would you like to generate a program unit? [y/n] ").strip().lower() == 'y':
            print('Generating skip.project.json')
            generate_project_json.generate_project(dir, unit_name, [source_path])
        else:
            exit(0)

    # return /path/to/project_dir, programUnit
    return (dir, unit_name)


def commonArguments():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('program', type=validate_sk_unit,
                        help='Skip program unit (must be specified ' +
                        'in your skip.project.json)')
    parser.add_argument('--verbose', action='store_true',
                        default=os.environ.get('VERBOSE', '') != '')
    parser.add_argument('--compiler', '-c', default=False, action='store_true',
                        help='Print full skip_to_llvm command then exit')
    parser.add_argument('--exists', '-e', default=False, action='store_true',
                        help='The program unit is already compiled with the given name.')
    return parser


def separateBinaryArgs():
    # Strip off '--' remainder to pass to the skip binary separately
    remainder = []
    if '--' in sys.argv:
        idx = sys.argv.index('--')
        remainder = sys.argv[idx + 1:]
        sys.argv = sys.argv[:idx]
    return remainder


def parse_args(parser):
    args = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
    return args


# source: https://stackoverflow.com/a/39504463
class Spinner(object):
    busy = False
    delay = 0.1

    @staticmethod
    def spinning_cursor():
        while 1:
            for cursor in '|/-\\': yield cursor

    def __init__(self, delay=None):
        self.spinner_generator = self.spinning_cursor()
        if delay and float(delay): self.delay = delay

    def __enter__(self):
        return self

    def spinner_task(self):
        while self.busy:
            sys.stdout.write(next(self.spinner_generator))
            sys.stdout.flush()
            time.sleep(self.delay)
            sys.stdout.write('\b')
            sys.stdout.flush()

    def start(self):
        self.busy = True
        threading.Thread(target=self.spinner_task).start()

    def stop(self):
        self.busy = False
        time.sleep(self.delay)

    def __exit__(self, exc_type, exc_value, traceback):
        self.stop()


class ExitStack(object):
    def __init__(self):
        self._objs = []

    def enter_context(self, obj):
        self._objs.append(obj)
        return obj.__enter__()

    def __enter__(self):
        return self

    def __exit__(self, *exc_details):
        for obj in self._objs:
            obj.__exit__(*exc_details)
