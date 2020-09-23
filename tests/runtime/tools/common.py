from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import atexit
import argparse
import contextlib
import errno
import getpass
import json
import logging
import os
import pipes
import re
import resource
import shutil
import subprocess
import sys
import tempfile
import threading
import time

from collections import defaultdict

logger = logging.getLogger(__name__)

source_dir = os.path.normpath(os.path.join(os.path.abspath(__file__), '../../..'))
root_dir = os.path.normpath(os.path.join(source_dir, '..'))
phase = os.path.basename(source_dir)
build_dir = os.path.normpath(os.path.join(source_dir, '../build'))
binary_dir = os.path.normpath(os.path.join(build_dir, phase))
runtime_dir = os.path.normpath(os.path.join(source_dir, 'runtime'))
profile_dir = os.path.join(build_dir, 'profile')

# This is filled in by parse_args
ARGS = None

# (max lines, max columns)
MAX_ERROR_OUTPUT = (200, 1600) if int(os.environ.get('SKIP_TEST_LIMIT_OUTPUT', 0)) else (1 << 62, 1 << 62)

# If __file__ doesn't exist then we're probably in a zipfile (i.e. packaged)
IN_PROD_TREE = not os.path.exists(__file__)

# Turn off core dumps, because some of our tests make very big sparse
# VM spaces and core dumps can take forever.
resource.setrlimit(resource.RLIMIT_CORE,
                   (0, resource.getrlimit(resource.RLIMIT_CORE)[1]))


try:
    RED = subprocess.check_output(('tput', 'setaf', '1')).decode()
    GREEN = subprocess.check_output(('tput', 'setaf', '2')).decode()
    NORMAL = subprocess.check_output(('tput', 'sgr0')).decode()
except subprocess.CalledProcessError:
    RED, GREEN, NORMAL = (str(''), str(''), str(''))


def splitRemainder():
    # Strip off '--' remainder to pass to the skip binary separately
    if '--' in sys.argv:
        idx = sys.argv.index('--')
        remainder = sys.argv[idx + 1:]
        sys.argv = sys.argv[:idx]
    else:
        remainder = []
    return remainder


def commonArguments(needsBackend=True, backend=None):
    parser = argparse.ArgumentParser(add_help=False)

    parser.add_argument('--verbose', action='store_true',
                        default=os.environ.get('VERBOSE', '') != '')
    parser.add_argument('--prelude')
    parser.add_argument('--sk-standalone', help='A prebuilt sk_standalone.cpp to use rather than compiling manually.')
    parser.add_argument('--relative', help='Output paths relative to this location')
    parser.add_argument('--profile', help='Turn on profiling metrics and dump' +
                        ' them to a file')
    parser.add_argument('--update-baseline', action='store_true')
    parser.add_argument('--embedded64', action='store_true')
    parser.add_argument('--keep-temp', action='store_true',
                        default=os.environ.get('KEEP_TEMP', '') != '')
    parser.add_argument('--print-skip-to-llvm',
        default=False, action='store_true',
        help='Print full skip_to_llvm command then exit')

    if needsBackend:
        parser.add_argument('backend_gen', help='is one of skip_native_exec')

    if backend:
        global _shortBackend
        _shortBackend = backend

    return parser


def updateTmpDir():
    newtmp = os.path.join(binary_dir, 'tmp')
    try:
        ensureDirPathExists(newtmp)
        os.environ['TMPDIR'] = newtmp
    except Exception:
        pass


def fullBackendGen(backend_gen):
    return os.path.abspath(
        os.path.join(source_dir, 'runtime/tools', backend_gen))


def parse_args(parser, args=None):
    global ARGS
    ARGS = parser.parse_args(args=args)

    logging.basicConfig(level=logging.DEBUG if ARGS.verbose else logging.INFO)

    updateTmpDir()

    if not hasattr(ARGS, 'backend_gen'):
        ARGS.backend_gen = None
    elif not os.path.isabs(ARGS.backend_gen):
        ARGS.backend_gen = fullBackendGen(ARGS.backend_gen)

    if not ARGS.prelude:
        ARGS.prelude = os.path.join(runtime_dir, 'prelude')
    ARGS.prelude = os.path.abspath(ARGS.prelude)

    if os.environ.get('UPDATE_BASELINE', '') != '':
        ARGS.update_baseline = True

    return ARGS


def buildNinjaTarget(target):
    """Build a ninja target, suppressing stdout/stderr"""
    ninja_cmd = ('ninja', '-C', build_dir, target)
    grep_cmd = ('egrep', '-v', '^ninja: (no work to do\.|Entering directory .*/build.)$')
    logger.debug('Building: ' + ' '.join(map(pipes.quote, ninja_cmd)))

    ninja_process = subprocess.Popen(
        ninja_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=None,
        cwd=None)
    grep_process = subprocess.Popen(
        grep_cmd,
        stdin=ninja_process.stdout,
        stdout=None,
        stderr=subprocess.STDOUT,
        env=None,
        cwd=None)
    ninja_process.stdout.close()
    grep_process.wait()


def callHelper(cmd, output=None, env=None, cwd=None):
    """Given a command which dumps its output to stdout, run it and if it fails
    dump the output to stderr instead.
    """
    if env:
        env = dict(os.environ.items() + env.items())
    else:
        env = os.environ

    try:
        logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))
        subprocess.check_call(cmd, env=env, stdout=output, cwd=cwd)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


def pathRelativeTo(base, *delta):
    return os.path.normpath(os.path.join(os.path.abspath(base), *delta))


def prefixRelPath(*delta):
    return pathRelativeTo(sys.argv[0], '../..', *delta)


def cmakeCacheGet(cmake_var):
    cache = os.path.join(build_dir, 'CMakeCache.txt')
    if not os.path.isfile(cache):
        logger.error('%r not found', cache)
        raise RuntimeError('CMakeCache.txt not found')

    with open(cache, 'r') as f:
        data = [x.strip() for x in f if x.startswith(cmake_var)]

    if not data:
        logger.error('Key %r not found in CMakeCache.txt', cmake_var)
        raise RuntimeError('Key not found')

    return data[0].split('=', 1)[1]


def getBuildInfo():
    assert ARGS
    cmd = (os.path.join(build_dir, 'bin/skip_to_llvm'), '--version',)
    if IN_PROD_TREE:
        cmd = (prefixRelPath('bin/skip_to_llvm'),)
    elif 'via_backend' in ARGS:
        cmd = (os.path.join(ARGS.via_backend, 'skip_to_llvm'),)
    cmd += ('--version',)
    return tuple(subprocess.check_output(cmd).split('\n')[:2])


_shortBackend = None
def getShortBackend():
    assert ARGS
    global _shortBackend
    if _shortBackend: return _shortBackend
    if not ARGS.backend_gen:
        _shortBackend = None
        return None
    _shortBackend = {
        'skip_native_exec': 'native',
    }.get(os.path.basename(ARGS.backend_gen))
    if not _shortBackend:
        print('%sError: unknown backend_gen type (%r)%s'
              % (RED, ARGS.backend_gen, NORMAL))
        sys.exit(1)
    return _shortBackend


def backendName(name, ext, backend):
    if backend:
        return '%s.%s.%s' % (name, backend, ext)
    else:
        return '%s.%s' % (name, ext)


def computeRelativePath(path):
    assert ARGS
    if ARGS.relative:
        path = os.path.relpath(
            os.path.realpath(path),
            os.path.realpath(ARGS.relative))
        if not path.startswith('../'):
            path = './' + path
    else:
        path = os.path.abspath(path)
    return path


def sourceRelativePath(path):
    return os.path.relpath(
        os.path.realpath(path),
        os.path.realpath(source_dir))


_prelude = None
def prelude(absolute=False):
    assert ARGS
    global _prelude
    if _prelude: return _prelude

    if absolute:
        _prelude = ARGS.prelude
    else:
        _prelude = computeRelativePath(ARGS.prelude)

    return _prelude


# return /path/to/project/diretory:programUnit relative to skip/ dir
def getProjectFilePath(target):
    try:
        dir, unit = target.split(':')
    except ValueError:
        dir = os.path.dirname(target)
        unit = os.path.basename(target)
    rel_dir = os.path.relpath(os.path.abspath(dir), root_dir)
    return rel_dir + ':' + unit


def isSkipFile(path):
    base, ext = os.path.splitext(path)
    return ext == '.sk'

# Returns (relative path, unit) if the target is a valid project file,
# otherwise None.
def loadProjectFile(target, backend):
    if isSkipFile(target):
        return None

    buildNinjaTarget('skip_depends')
    try:
        dir, unit_name = target.split(':')
    except ValueError:
        unit_name = ''
        dir = target

    dir = os.path.abspath(dir)
    unit_suffix = ':' + unit_name if unit_name != '' else ''

    cmd = (
        os.path.join(build_dir, 'bin/skip_depends'),
        '--binding', 'backend=' + ('native' if backend == 'native' else 'nonnative'),
        dir + unit_suffix )
    logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))
    returncode = subprocess.call(cmd, env=os.environ, stdout=subprocess.PIPE)
    if (returncode != 0):
        return None

    # return /path/to/project_dir, programUnit
    return (dir, unit_name)


def computeTargetName(test, backend=None):
    assert ARGS
    if not backend:
        backend = getShortBackend()

    rootReal = os.path.realpath(root_dir)
    buildReal = os.path.realpath(build_dir)
    testReal = os.path.splitext(os.path.realpath(test))[0]

    if os.path.commonprefix((buildReal, testReal)) == buildReal:
        relPath = os.path.relpath(testReal, buildReal)
    elif os.path.commonprefix((rootReal, testReal)) == rootReal:
        relPath = os.path.relpath(testReal, rootReal)
    elif ARGS.relative:
        relPath = os.path.splitext(os.path.relpath(os.path.join(testReal, test[2:]), root_dir))[0]
    else:
        relPath = os.path.relpath(os.path.dirname(sys.argv[0]), root_dir)

    testPrefix = ('test_' + backend) if backend else 'test'

    testTarget = testPrefix + '.' + relPath.replace('/', '.')
    return testTarget


logged_data = []
registered = False
def _registerLogAtExit():
    global registered
    if not registered:
        atexit.register(_writeToPerfLog)
        registered = True


def logPerfData(label, tables, value):
    if ARGS.profile:
        logged_data.append({'sample_name': label, 'value': value})
        _registerLogAtExit()

def _writeToPerfLog():
    ensureDirPathExists(os.path.dirname(ARGS.profile))
    obj = []
    if os.path.isfile(ARGS.profile):
        with open(ARGS.profile, 'r') as f:
            try:
                obj = json.load(f)
            except Exception:
                pass

    obj += logged_data
    tmp_output_name = ARGS.profile + '.' + str(os.getpid())
    with open(tmp_output_name, 'w') as f:
        json.dump(obj, f)
    os.rename(tmp_output_name, ARGS.profile)


class PerfTimer(object):
    def __init__(self, label, tables=['skip_compiler'][:]):
        self.label = label
        self.tables = tables
    def __enter__(self):
        self.start = time.time()
    def __exit__(self, *exc_details):
        end = time.time()
        logPerfData(self.label, self.tables, (end - self.start) * 1000)


class StreamTee(threading.Thread):
    def __init__(self, reader, writer1, writer2):
        super(StreamTee, self).__init__()
        self._reader = reader
        self._writer1 = writer1
        self._writer2 = writer2
        self.daemon = True
        self.start()

    def run(self):
        while True:
            try:
                read = self._reader.read(16384)
                if not read:
                    break
                self._writer1.write(read)
                self._writer1.flush()
                self._writer2.write(read)
                self._writer2.flush()
            except Exception:
                break


class RunCommand(object):
    # test - the path to the test
    # cmd - the command list to run
    # env - if non-None then overrides the command environment
    # testReportsOk - if True then echo stdout and expect the test to report
    #                 [OK] internally.
    # testHasExpect - if True then expect the test to have a .exp file
    def __init__(self, test, cmd, env=None, testReportsOk=False,
                 expectError=False, testHasExpect=True, backend=None,
                 expectedExtension='.exp', manualUpdateBaseline=False):
        self.test = test
        self._testReportsOk = testReportsOk
        self._expectError = expectError
        self._testHasExpect = testHasExpect and not testReportsOk
        self.manual_update_baseline = manualUpdateBaseline
        self.backend = backend or getShortBackend()
        self.returncode = 1
        self.expectedExtension = expectedExtension
        self.time_taken = None

        self._computeFilenames()

        if os.path.exists(self.exp_name + '_failure'):
            self._expectError = True

        if not cmd:
            return

        logger.debug('running: %r', cmd)

        dst_we = RunCommand._computeDstWithoutExt(test)
        ensureDirPathExists(os.path.dirname(dst_we))

        with open(self.stdout_name, 'wb') as stdout, \
             open(self.stderr_name, 'wb') as stderr, \
             open(self.res_name, 'w') as resout, \
             open(os.devnull, 'rb') as stdin:
            start = time.time()
            p = subprocess.Popen(
                cmd,
                env=env,
                stdout=subprocess.PIPE if self._testReportsOk else stdout,
                stderr=stderr,
                stdin=stdin)

            stdoutTee = None
            if self._testReportsOk:
                stdoutTee = StreamTee(p.stdout, sys.stdout, stdout)

            self.returncode = p.wait()
            end = time.time()
            self.time_taken = end - start

            if stdoutTee:
                stdoutTee.join()

            json.dump({
                'returncode': self.returncode,
                'time_taken': self.time_taken,
                'expectError': self._expectError,
                'testReportsOk': self._testReportsOk,
                'testHasExpect': self._testHasExpect,
            }, resout)

    def checkReturnCode(self):
        assert ARGS
        success = self.returncode == 0
        if self._expectError:
            success = not success

        if success:
            return True

        self.error()
        print('  Expected test to %s but it %s (returncode %d).' % (
            ('succeed', 'fail')[self._expectError],
            ('failed', 'succeeded')[self._expectError],
            self.returncode))
        self.write_stdout()
        self.write_stderr()
        return False

    @staticmethod
    def _computeDstWithoutExt(test):

        binReal = os.path.realpath(binary_dir)
        testReal = os.path.realpath(test)

        if os.path.commonprefix((binReal, testReal)) == binReal:
            # The test is already in the binary directory
            path = test
        else:
            srcRel = sourceRelativePath(test)
            path = os.path.join(binary_dir, srcRel)

        return os.path.splitext(path)[0]

    @staticmethod
    def fromArtifacts(test, backend):

        cmd = RunCommand(test, None, backend=backend)

        if os.path.isfile(cmd.res_name):
            with open(cmd.res_name, 'rb') as f:
                results = json.load(f)
            if 'testHasExpect' not in results:
                results['testHasExpect'] = not results['testReportsOk']

            cmd._testReportsOk = results['testReportsOk']
            cmd._expectError = results['expectError']
            cmd._testHasExpect = results['testHasExpect']
            cmd.returncode = results['returncode']
            cmd.time_taken = results.get('time_taken')
        else:
            cmd.returncode = 127

        return cmd

    @staticmethod
    def _computeResName(test, backend):
        dst_we = RunCommand._computeDstWithoutExt(test)
        return backendName(dst_we, 'res', backend)

    def _computeFilenames(self):
        dst_we = RunCommand._computeDstWithoutExt(self.test)

        self.res_name = backendName(dst_we, 'res', self.backend)
        self.stdout_name = backendName(dst_we, 'out', self.backend)
        self.stderr_name = backendName(dst_we, 'err', self.backend)
        self.testok_name = backendName(dst_we, 'testok', self.backend)
        self.failing_name = backendName(dst_we, 'testfail', self.backend)

        # Default to a .exp next to the source
        self.exp_name = os.path.splitext(self.test)[0] + self.expectedExtension

        for ext in (self.expectedExtension, '.expectregex'):
            for path in (dst_we, os.path.splitext(self.test)[0]):
                name = path + ext
                if os.path.exists(name):
                    self.exp_name = name
                    break

    @property
    def timeTaken(self):
        return self.time_taken

    @property
    def targetName(self):
        return computeTargetName(self.test, backend=self.backend)

    @property
    def stdout(self):
        if self._testReportsOk: return ''
        try:
            with open(self.stdout_name, 'r') as f:
                return f.read()
        except IOError:
            return ''

    @property
    def stderr(self):
        try:
            with open(self.stderr_name, 'r') as f:
                return f.read()
        except IOError:
            return ''

    def success(self):
        assert ARGS
        if not self._testReportsOk:
            self.print_msg(GREEN, ('[OK] - %.2fs' % self.time_taken))

        with open(self.testok_name, 'w') as f:
            f.write('ok\n')

    def error(self):
        self.print_msg(RED, '[FAILED]')
        with open(self.failing_name, 'w') as f:
            f.write('fail\n')

    def write_file(self, name, data, limit=None):
        assert not limit or (isinstance(limit, (tuple, list)) and len(limit) == 2)
        data = data.strip()
        if data:
            print('  %s%s:%s' % (RED, name, NORMAL))
            if limit:
                lines = data.encode().split('\n')
                if len(lines) > limit[0]:
                    lines = ['<output truncated>'] + lines[-limit[0]:]
                lines = map(lambda x: x if (len(x) < limit[1]) else (x[:limit[1] - 3] + '...'), lines)
                data = '\n'.join(lines)
            print('\n'.join(map(lambda x: '    ' + x, data.split('\n'))))

    def write_stdout(self, limit=MAX_ERROR_OUTPUT):
        self.write_file('stdout', self.stdout, limit=limit)

    def write_stderr(self, limit=MAX_ERROR_OUTPUT):
        self.write_file('stderr', self.stderr, limit=limit)

    def verbose_write_stderr(self):
        assert ARGS
        if ARGS.verbose:
            self.write_stderr()

    def isFileEmpty(self, name):
        with open(name, 'r') as f:
            return not f.read()

    # Check that the diff matches the .exp file and return True if different
    def report_diff(self):
        assert ARGS
        if self._testReportsOk:
            return False
        if not self._testHasExpect:
            return False

        diffname = (self.stdout_name, self.stderr_name)[self._expectError]

        if not os.path.exists(self.stdout_name):
            print('%sError: missing output file %r%s' % (RED, diffname, NORMAL))
            return True

        if ARGS.update_baseline and not self.manual_update_baseline and not self.exp_name.endswith('.expectregex'):
            shutil.copyfile(self.stdout_name, self.exp_name)
            if not self.isFileEmpty(self.stderr_name):
                shutil.copyfile(self.stderr_name, self.exp_name + "_err")
            return False

        if not os.path.exists(self.exp_name):
            print('%sError: missing expect file %r.%s' % (RED, self.exp_name, NORMAL))
            print('To update the baseline re-run with UPDATE_BASELINE=1.')
            return True

        if self.diff_file(self.stdout_name, self.exp_name):
            return True
        if os.path.exists(self.exp_name + '_err'):
            if self.diff_file(self.stderr_name, self.exp_name + '_err'):
                return True
        else:
            with open(self.stderr_name, 'r') as f:
                content = f.read()
            if content:
                print('Expected %r to be empty. Got:\n%s' %
                      (self.stderr_name, content))
                return True

        return False

    def diff_file(self, actual, expected):
        if os.path.splitext(expected)[1].startswith('.expectregex'):
            with open(expected, 'r') as f:
                expectRE = f.read()
            with open(actual, 'r') as f:
                text = f.read()
            if re.match(expectRE, text, flags=re.DOTALL + re.MULTILINE):
                return False

        try:
            subprocess.check_output(('diff', '-u', expected, actual))
        except subprocess.CalledProcessError as e:
            # Diff failed
            self.error()
            self.write_file('diff', e.output, limit=MAX_ERROR_OUTPUT)
            return True
        return False

    def print_msg(self, color, msg):
        print_msg(self.test, color, msg, backend=self.backend)


def print_msg(test, color, msg, backend=None):
    name = computeTargetName(test, backend=backend)
    size = 80 - len(name)
    print('%s%s%*s%s' % (name, color, size, msg, NORMAL))


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


# tmpfile() is like NamedTemporaryFile() but instead of deleting on close it
# only deletes when the context exits (so the file can be closed and used).
@contextlib.contextmanager
def tmpfile(prefix=None, suffix=None):
    assert ARGS
    kwargs = {
        'delete': False
    }
    if prefix:
        kwargs['prefix'] = prefix
    if suffix:
        kwargs['suffix'] = suffix
    f = tempfile.NamedTemporaryFile(**kwargs)
    if ARGS.keep_temp:
        logger.debug('(KEEPING TEMP FILE: %s)', f.name)
    yield f
    if not ARGS.keep_temp:
        os.unlink(f.name)


# tmpdir() - directory version of tmpfile that creates a temporary directory
# and deletes when the context exits (so the directory can be used).
@contextlib.contextmanager
def tmpdir(prefix=None, suffix=None):
    assert ARGS
    kwargs = {}
    if prefix:
        kwargs['prefix'] = prefix
    if suffix:
        kwargs['suffix'] = suffix
    dir = tempfile.mkdtemp(**kwargs)
    if ARGS.keep_temp:
        logger.debug('(KEEPING TEMP FILE: %s)', dir)
    yield dir
    if not ARGS.keep_temp:
        shutil.rmtree(dir)


def ensureDirPathExists(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno != errno.EEXIST:
            raise
