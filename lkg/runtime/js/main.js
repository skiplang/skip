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

const {initFromArgs} = require('./init');
const sk = initFromArgs();

process.on('uncaughtException', (err) => {
  console.error('Uncaught exception: ' + err.getMessage().__value);
  process.exit(2);
});

if (typeof sk.main !== 'function') {
  console.error('Main not found');
  process.exit(2);
}

sk.main();
