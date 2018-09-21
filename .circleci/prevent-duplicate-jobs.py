#!/usr/bin/env python

import httplib
import json
import os
import subprocess
import sys
import time


def circleci_command(method, url, body=None):
  token = os.environ['CIRCLE_TOKEN']
  conn = httplib.HTTPSConnection('circleci.com')
  conn.request(
    method,
    '/api/v1.1/project/github/skiplang/skip' + url+'?circle-token=' + token,
    body,
    {'Accept': 'application/json'}
  )
  res = conn.getresponse()
  return json.loads(res.read())

branch = os.environ['CIRCLE_BRANCH']
# Only do this optimization on pull request jobs
if not branch.startswith('pull/'):
    sys.exit(0)

while 1:
    output = subprocess.check_output(
      ['git', 'ls-remote', 'git@github.com:skiplang/skip.git', 'refs/' + branch + '/head']
    )
    rev = output.split()[0]
    print(
      "Found rev (%s) vs running rev (%s)" % (rev, os.environ['CIRCLE_SHA1'])
    )
    if rev != os.environ['CIRCLE_SHA1']:
      print("Canceling myself (build: %s)" % os.environ['CIRCLE_BUILD_NUM'])
      circleci_command('POST', '/%s/cancel' % os.environ['CIRCLE_BUILD_NUM'])
    time.sleep(30)
