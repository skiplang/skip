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

module.exports = function(sk) {

  function Int32(val) {
    // type of val is sk.Int
    this.__val = val;
  }

  return {
    ctor: Int32,
    instanceMembers: {
      'toInt': function() {
        return this.__val;
      }
    },
    staticMembers: {
      'truncate': function(value) {
        // all 32 bits are significant
        var lo = value.__lo;

        // Sign extend
        var hi = 0;
        if (0 !== (lo & 0x80000000)) {
          hi = -1;
        }

        return new Int32(
          (lo === value.__lo && hi === value.__hi)
            ? value
            : new sk.Int(lo, hi));
      }
    }
  };
};
