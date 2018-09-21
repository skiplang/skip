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

  function Int16(val) {
    // type of val is sk.Int
    this.__val = val;
  }

  return {
    ctor: Int16,
    instanceMembers: {
      'toInt': function() {
        return this.__val;
      }
    },
    staticMembers: {
      'truncate': function(value) {
        // Limit the number to significant bits to 16
        var lo = value.__lo & 0xFFFF;

        // Sign extend
        var hi = 0;
        if (0 !== (lo & 0x8000)) {
          lo = lo | 0xFFFF0000;
          hi = -1;
        }

        return new Int16(
          (lo === value.__lo && hi === value.__hi)
            ? value
            : new sk.Int(lo, hi));
      }
    }
  };
};
