#!/usr/bin/env node

# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

// Converts a skip_lsp log file to a script for a skip_lsp test
// skip_lsp log files live in /tmp/skip_lsp*
const fs = require('fs');

if (process.argv.length < 4) {
  console.error('Usage: log_to_script.sh log-name root-source-dir');
  process.exit(1);
}

// Read JSON script
const logName = process.argv[2];
const rootSourceDir = process.argv[3];

const logContents = fs.readFileSync(logName, 'utf8');

// Convert a JS string for use in regexp
function escapeRegExp(str) {
  return str.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, "\\$&");
}

const currentDirPlaceholder = '%CURRENT_DIR%';

// Log files are a sequence of newline separated JSON messages.
// Remove the 'message' message kinds, leaving the RPC sends/receives.
const result = logContents.
  replace(
    new RegExp(escapeRegExp(rootSourceDir), 'g'),
    currentDirPlaceholder,
  ).
  split('\n').
  filter(message => message.length > 0 && JSON.parse(message).kind !== 'message').
  join(',\n');

console.log('[\n');
console.log(result);
console.log(']\n');
