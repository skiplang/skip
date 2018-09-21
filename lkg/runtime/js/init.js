/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

'use strict';

const path = require('path');
const fs = require('fs');
const skip = require('./skip');

function init(mainFile, debug, args) {
  if (!fs.existsSync(mainFile)) {
    throw new Error(`${mainFile} does not exist`);
  }
  const sk = require(path.relative(__dirname, mainFile));

  skip.makeExceptionsPrintStacks(sk, debug);
  skip.setArguments(args);
  skip.setDebug(debug);
  return sk;
}

function usage() {
  console.log('Usage: node main.js [--no-unhandled-exception-stack] <source_file>');
  process.exit(1);
}

// Parse arguments:
//  node main.js [--no-unhandled-exception-stack] sk-to-js-file [args...]
function initFromArgs() {
  const args = process.argv;
  args.shift(); // node
  args.shift(); // main.js
  if (args.length < 1) {
    usage();
  }
  let debug = true;
  if (args[0] === '--no-unhandled-exception-stack') {
    debug = false;
    args.shift();
  }
  if (args.length < 1) {
    usage();
  }
  const sourceFile = args[0];
  args.shift();
  return init(sourceFile, debug, args);
}

module.exports = {
  init,
  initFromArgs,
};
