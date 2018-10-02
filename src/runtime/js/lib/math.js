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

    // {__value: 1.57079633}
    // so use val.__value

    function sin(val) {
      return sk.__.floatToFloat(Math.sin(val.__value));
    }
    function cos(val) {
      return sk.__.floatToFloat(Math.cos(val.__value));
    }
    function asin(val) {
      return sk.__.floatToFloat(Math.asin(val.__value));
    }
    function acos(val) {
      return sk.__.floatToFloat(Math.acos(val.__value));
    }
    function floor(val) {
      return sk.__.floatToFloat(Math.floor(val.__value));
    }
    function ceil(val) {
      return sk.__.floatToFloat(Math.ceil(val.__value));
    }
    function round(val) {
      // Unlike all the other programming languages I could find
      // (Ruby, PHP, Python, C#, C, Swift), JavaScript rounds up for
      // negative numbers that end with 0.5.
      // Implementing round in terms of ceil(x - 0.5) makes it behave
      // like the rest.
      var x = val.__value;
      if (x >= 0) {
        return sk.__.floatToFloat(Math.floor(x + 0.5));
      } else {
        var result = Math.ceil(x - 0.5);
        // Handle -0.0
        return sk.__.floatToFloat((result === 0) ? 0 : result);
      }
    }
    function sqrt(val) {
      return sk.__.floatToFloat(Math.sqrt(val.__value));
    }
    function pow(val, power) {
      return sk.__.floatToFloat(Math.pow(val.__value, power.__value));
    }
    function abs(val) {
      return sk.__.floatToFloat(Math.abs(val.__value));
    }
    
    // attach global functions directly to sk object
    sk.__.defineGlobalFunction('Math.sin', sin);
    sk.__.defineGlobalFunction('Math.cos', cos);
    sk.__.defineGlobalFunction('Math.asin', asin);
    sk.__.defineGlobalFunction('Math.acos', acos);
    sk.__.defineGlobalFunction('Math.floor', floor);
    sk.__.defineGlobalFunction('Math.ceil', ceil);
    sk.__.defineGlobalFunction('Math.round', round);
    sk.__.defineGlobalFunction('Math.sqrt', sqrt);
    sk.__.defineGlobalFunction('Math.pow', pow);
    sk.__.defineGlobalFunction('Math.abs', abs);
}
