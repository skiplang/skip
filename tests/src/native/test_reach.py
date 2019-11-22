#!/usr/bin/env python3
import argparse
import collections
import imp
import logging
import os
import pipes
import shutil
import subprocess
import sys
import tempfile


common = imp.load_source(
    'common',
    os.path.join(os.path.dirname(sys.argv[0]),
                 '../../runtime/tools/common.py'))

logger = logging.getLogger(os.path.basename(__file__))
args = None


def runTest(stack, nbeFlags, code):
    skcode = stack.enter_context(
        common.tmpfile(prefix='tmp.code.', suffix='.sk'))
    skcode.write(code)
    skcode.flush()

    res = collections.namedtuple('res', ['output', 'stdout', 'stderr'])(
        output=stack.enter_context(
            common.tmpfile(prefix='tmp.out.', suffix='.ll')),
        stdout=stack.enter_context(
            common.tmpfile(prefix='tmp.', suffix='.stdout')),
        stderr=stack.enter_context(
            common.tmpfile(prefix='tmp.', suffix='.stderr')))

    cmd = (
        os.path.join(common.runtime_dir, 'tools/skip_to_native'),
        '-o', res.output.name,
        '--emit-llvm',
        '--nbe-flags=' + ','.join(nbeFlags),
        '--via-backend', os.path.join(common.build_dir, "bin"),
        skcode.name,
        common.prelude(),
    )

    logger.debug('running: %r', ' '.join(map(pipes.quote, cmd)))
    try:
        subprocess.check_call(cmd, stdout=res.stdout, stderr=res.stderr)
    except subprocess.CalledProcessError:
        print >>sys.stderr, 'ERROR: Compile failed'
        res.stderr.seek(0)
        shutil.copyfileobj(res.stderr, sys.stderr)
        sys.exit(1)

    res.output.seek(0)
    res.stdout.seek(0)
    res.stderr.seek(0)

    return res


def checkReach(name, prefix, code, expect):
    print 'Running test', name
    reach = {}
    with common.ExitStack() as stack:
        res = runTest(stack, ['--verbose', '--noinline'], code)
        for line in (x for x in res.stderr.read().split('\n')
                     if x.startswith('reach[' + prefix)):
            name, body = line.split('=', 1)
            name = name.strip()
            if '::' in name:
                continue
            if name.startswith('reach[') and name.endswith(']'):
                name = name[6:-1] # strip 'reach[' ... ']'
            else:
                assert False, 'Invalid format for name:' + name

            body = body.strip()
            if body == 'none':
                body = None
            elif body.startswith('[') and body.endswith(']'):
                body = set(x.strip() for x in body[1:-1].split(','))
            else:
                assert False, 'Invalid format for body:' + body

            reach[name] = body

    if reach != expect:
        for k, v in sorted(reach.items()):
            if k not in expect:
                print >>sys.stderr, \
                    "Key %s generated (with value %r) but wasn't expected" % (
                        k, v)
            else:
                if expect[k] != v:
                    print >>sys.stderr, 'Key %s expected %r but got %r' % (
                        k,
                        sorted(expect[k]) if expect[k] else None,
                        sorted(v) if v else None)
                del expect[k]

        for k, v in sorted(expect.items()):
            print >>sys.stderr, \
                "Key %s was expected but wasn't generated" % (k,)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        parents=[common.commonArguments(needsBackend=False, backend='native')])
    global args
    args = common.parse_args(parser)

    checkReach('Simple',
               'Chk', '''

mutable class ChkA0 { mutable za: Int = 0 }
class ChkA1 { x: ChkA0 = ChkA0{} }

base class ChkB0 { }
class ChkB1 { } extends ChkB0

mutable base class ChkC0 { }
class ChkC1 { } extends ChkC0
class ChkC2 { a: ChkA0 = ChkA0{} } extends ChkC0
mutable class ChkC3 { b: mutable ChkA0 = mutable ChkA0{} } extends ChkC0

mutable base class ChkD0 { mutable za: Int = 0 }
mutable class ChkD1 { } extends ChkD0
mutable class ChkD2 { b: mutable ChkD1 = mutable ChkD1{} } extends ChkD0

mutable base class ChkE0 { mutable za: Int = 0 }
mutable class ChkE1 { } extends ChkE0
mutable class ChkE2 { b: mutable ChkE0 = mutable ChkE1{} } extends ChkE0

mutable base class ChkF0 { mutable za: Int = 0 }
mutable class ChkF1 { b: mutable ChkF2 = mutable ChkF3{} } extends ChkF0
mutable base class ChkF2 { mutable za: Int = 0 }
mutable class ChkF3 { b: mutable ChkF0 = mutable ChkF1{} } extends ChkF2

mutable base class ChkG0 { mutable za: Int = 0 }
mutable class ChkG1 { b: mutable ChkG2 = mutable ChkG3{} } extends ChkG0
mutable base class ChkG2 { mutable za: Int = 0 }
mutable class ChkG3 { mutable zb: mutable ChkG0 = mutable ChkG1{} } extends ChkG2

mutable class ChkH0 { mutable za: Int = 0 }
mutable class ChkH1 { a: mutable ChkH0 = mutable ChkH0{}, b: ChkH0 = ChkH0{} }
mutable class ChkH2 { a: mutable ChkH0 = mutable ChkH0{}, b: mutable ChkH0 = mutable ChkH0{} }

value class Hack17<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17>(
  T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12, T13, T14, T15, T16, T17
)

@no_inline
fun hack<T>(T): void { void }

fun main(): void {
  hack(Hack17(
    ChkA0{}, ChkA1{},
    ChkB1{},
    ChkC1{}, ChkC2{}, ChkC3{},
    ChkD1{}, ChkD2{},
    ChkE1{}, ChkE2{},
    ChkF1{}, ChkF3{},
    ChkG1{}, ChkG3{},
    ChkH0{}, ChkH1{}, ChkH2{},
   ));
  print_raw("main")
}
''',
               {'ChkA0': set(['ChkA0']),
                'ChkA1': set(['ChkA1']),

                'ChkB0': set(['ChkB0', 'ChkB1']),
                'ChkB1': set(['ChkB1']),

                'ChkC0': set(['ChkA0', 'ChkC0', 'ChkC1', 'ChkC2', 'ChkC3']),
                'ChkC1': set(['ChkC1']),
                'ChkC2': set(['ChkC2']),
                'ChkC3': set(['ChkA0', 'ChkC3']),

                'ChkD0': set(['ChkD0', 'ChkD1', 'ChkD2']),
                'ChkD1': set(['ChkD1']),
                'ChkD2': set(['ChkD1', 'ChkD2']),

                'ChkE0': set(['ChkE0', 'ChkE1', 'ChkE2']),
                'ChkE1': set(['ChkE1']),
                'ChkE2': set(['ChkE0', 'ChkE1', 'ChkE2']),

                'ChkF0': set(['ChkF0', 'ChkF1', 'ChkF2', 'ChkF3']),
                'ChkF1': set(['ChkF0', 'ChkF1', 'ChkF2', 'ChkF3']),
                'ChkF2': set(['ChkF0', 'ChkF1', 'ChkF2', 'ChkF3']),
                'ChkF3': set(['ChkF0', 'ChkF1', 'ChkF2', 'ChkF3']),

                'ChkG0': None,
                'ChkG1': None,
                'ChkG2': None,
                'ChkG3': None,

                'ChkH0': set(['ChkH0']),
                'ChkH1': set(['ChkH0', 'ChkH1']),
                'ChkH2': None,
               })

if __name__ == '__main__':
    rc = main()
    sys.exit(rc)
