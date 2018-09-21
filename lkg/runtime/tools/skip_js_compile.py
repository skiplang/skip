#!/usr/bin/env python
# This script is defined for compatibility with the Compiler Unit Tests.
# It should not be used for other purposes.
# Specifically it disables dumping stack traces on unhandled exceptions.
# Use build/bin/skip_to_js && tools/run_js directly instead.

# Arguments: sk-files [-- sk-args...]

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import logging
import os
import pipes

import common

VERBOSE = os.environ.get('VERBOSE', '0') == '1'

logger = logging.getLogger(os.path.basename(__file__))
logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.INFO)


def compile(stack, args):
    extraOpts = []
    skip_flags = os.environ.get('SKIP_FLAGS', '')
    if skip_flags:
        extraOpts += skip_flags.split(',')

    output = stack.enter_context(common.tmpdir(prefix='tmp.js_exec.'))
    cmd = [os.path.join(common.build_dir, 'bin/skip_to_js'),
           '--output', output + '/sk.js', '--no-source-map'] + extraOpts + args.srcs

    logger.debug('Running: ' + ' '.join(map(pipes.quote, cmd)))

    with common.PerfTimer('skip_to_js.runtime'):
        common.callHelper(cmd)

    return [common.root_dir + '/tools/run_js',
            '--no-unhandled-exception-stack',
            output]
