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
const skip = require('./skip');
const ctx = skip.createReactiveContext();
const sk = initFromArgs();

function run(fn, args) {
  ctx.run(fn, args).then(value => {
    const intValue = sk.__.intToNumber(value);
    if (intValue !== 0) {
      process.exit(intValue);
    }
    if (ctx.hasSubscriptions()) {
      const unsubscribe = ctx.subscribe(error => {
        unsubscribe();
        if (error) {
          console.error(error);
          process.exit(1);
        }
        // Ensure run() is not synchronously executed to prevent a loop
        Promise.resolve().then(() => run(fn, args));
      });
    }
  }).catch(error => {
    console.error(error.stack || error);
  });
}

run(sk['__main__'], []);
