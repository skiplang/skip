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

  function Int8(val) {
    // type of val is sk.Int
    this.__val = val;
  }

  function toUInt8(x /* JS number */) /* Skip UInt8 */ {
    return sk.UInt8.truncate(sk.__.intToInt(x));
  }

  sk.__.toUInt8 = toUInt8;

  return {
    ctor: Int8,
    instanceMembers: {
      'toInt': function() {
        return this.__val;
      }
    },
    staticMembers: {
      'truncate': function(value) {
        // Limit the number to significant bits to 8
        var lo = value.__lo & 0xFF;

        // Sign extend
        var hi = 0;
        if (0 !== (lo & 0x80)) {
          lo = lo | 0xFFFFFF00;
          hi = -1;
        }

        return new Int8(
          (lo === value.__lo && hi === value.__hi)
            ? value
            : new sk.Int(lo, hi));
      }
    }
  };
};
