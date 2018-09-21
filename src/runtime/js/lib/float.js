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
  function Float(value) {
    this.__value = value;
  }

  function floatLessThan(a, b) {
    return a < b || ((a === b) && isNegativeZero(a) && !isNegativeZero(b));
  }

  function floatLessThanEqual(a, b) {
    return floatLessThan(a, b) || floatEquals(a, b);
  }

  function isNegativeZero(n) {
    // No I am not making this up.
    return Boolean((n === 0) && (1/n) < 0);
  }

  function floatEquals(a, b) {
    return Boolean((a === b) && (a !== 0 || (isNegativeZero(a) === isNegativeZero(b))));
  }

  // format JavaScript Float_ in %.17g format
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/fprintf.html
  function floatToString(n) {
    if (isNegativeZero(n)) {
      return '-0';
    } else if (n === parseInt(n)) {
      return n.toString();
    } else if (n === Infinity) {
      return 'inf';
    } else if (n === -Infinity) {
      return '-inf';
    } else if (n !== n) {
      return 'nan';
    }
    var expStr = n.toExponential(16);
    var found = expStr.match(/e([-+]?\d+)$/);
    var exponent = parseInt(found[1]);
    if (exponent < -4 || exponent >= 17) {
      // force two-digit exponent
      if (Math.abs(exponent) < 10) {
        expStr = expStr.replace(/\d$/, '') + "0" + Math.abs(exponent).toString();
      }
      // strip trailing zeros
      expStr = expStr.replace(/\.0+e/, 'e');
      return expStr;
    } else {
      var prec17 = n.toPrecision(17);
      if (!prec17.includes(".") || (prec17.includes("e") || prec17.includes("E"))) {
        return prec17;
      } else {
        // Trim trailing fractional 0's, but always leave a digit after the '.'
        var result = prec17;
        while (result.endsWith('0') && !result.endsWith('.0')) {
          result = result.slice(0, -1);
        }
        return result;
      }
    }
  }

  // convert JavaScript Number to Skip Float
  function floatToFloat(value) {
    return new Float(value);
  }

  sk.__.floatToFloat = floatToFloat;

  return {
    ctor: Float,
    instanceMembers:  {
      '__deepFreeze': function(cache) {
        return this;
      },
      '==': function(other) {
        return sk.__.boolToBool(floatEquals(this.__value, other.__value));
      },
      '!=': function(other) {
        return sk.__.boolToBool(!floatEquals(this.__value, other.__value));
      },
      '+': function(other) {
        return floatToFloat(this.__value + other.__value);
      },
      '-': function(other) {
        return floatToFloat(this.__value - other.__value);
      },
      '*': function(other) {
        return floatToFloat(this.__value * other.__value);
      },
      '/': function(other) {
        return floatToFloat(this.__value / other.__value);
      },
      'negate': function() {
        return floatToFloat(-this.__value);
      },
      '<': function(other) {
        return sk.__.boolToBool(floatLessThan(this.__value, other.__value));
      },
      '>': function(other) {
        return sk.__.boolToBool(floatLessThan(other.__value, this.__value));
      },
      '<=': function(other) {
        return sk.__.boolToBool(floatLessThanEqual(this.__value, other.__value));
      },
      '>=': function(other) {
        return sk.__.boolToBool(floatLessThanEqual(other.__value, this.__value));
      },
      'toInt': function() {
        return sk.__.intToInt(Math.floor(this.__value));
      },
      'toBits': function() {
        var buf = new ArrayBuffer(8);
        (new Float64Array(buf))[0] = this.__value;
        return sk.__.makeInt(
          (new Uint32Array(buf))[0],
          (new Uint32Array(buf))[1]);
      },
      'toString': function() {
        let s = floatToString(this.__value);
        let ascii_code_0 = 48;
        let ascii_code_9 = 57;
        for (let i = 0; i < s.length; i++) {
          let c = s.charAt(i);
          let code = s.charCodeAt(i);
          if ((code < ascii_code_0 || code > ascii_code_9) && c != "-") {
            return new sk.String(s);
          }
        }
        return new sk.String(s + ".0");
      },
      '__getSwitchValue': function() {
        return this.__value;
      },
    },
    staticMembers: {}
  };
};
