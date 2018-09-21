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

  const divide = sk.__.manglePropertyName('/');
  const times = sk.__.manglePropertyName('*');
  const plus = sk.__.manglePropertyName('+');
  const minus = sk.__.manglePropertyName('-');

  function toDebugString(value) {
    return value.__lo + ', ' + value.__hi;
  }

  function makeInt(lo, hi) {
    return new Int(lo, hi);
  }

  function toDebugString(value) {
    return value.__lo + ', ' + value.__hi;
  }

  function intEquals(left, right) {
    return left.__lo === right.__lo && left.__hi === right.__hi;
  }

  function isZero(value) {
    return value.__lo === 0 && value.__hi === 0;
  }

  function isOdd(value) {
    return (value.__lo & 1) === 1;
  }

  function not(value) {
    return makeInt(~value.__lo, ~value.__hi);
  }

  function signOfNumber(value) {
    return (value < 0) ? -1 : 1;
  }

  // 1 if this greater, -1 if other greater, 0 if same
  function compare(left, right) {
    const hiDiff = left.__hi - right.__hi;
    if (hiDiff !== 0) {
      return signOfNumber(hiDiff);
    }

    const loDiff = unsignedLoBits(left) - unsignedLoBits(right);
    if (loDiff === 0) {
      return loDiff;
    } else {
      return signOfNumber(loDiff);
    }
  }

  // returns -1 if negative, 1 otherwise
  function sign(value) {
    return signOfNumber(value.__hi);
  }

  function isNegative(value) {
    return value.__hi < 0;
  }

  const NEG_TWO_31 = 1 << 31;
  const TWO_32 = -NEG_TWO_31 * 2;
  const TWO_63 = -NEG_TWO_31 * TWO_32;
  const TWO_64 = TWO_32 * TWO_32;
  const MIN_VALUE = makeInt(0, 0x80000000);
  const MAX_VALUE = makeInt(0xFFFFFFFF, 0x7FFFFFFF);
  const MINUS_ONE = makeInt(-1, -1);
  const ONE = makeInt(1, 0);
  const ZERO = makeInt(0, 0);

  function unsignedBits(value) {
    return value >= 0 ? value : (value + TWO_32);
  }
  function unsignedLoBits(value) {
    return unsignedBits(value.__lo);
  }

  function unsignedHiBits(value) {
    return unsignedBits(value.__hi);
  }

  function intToNumber(value) {
    return value.__hi * TWO_32 + unsignedLoBits(value);
  }

  function fromNumber(value) {
    if (isNaN(value)) {
      return ZERO;
    // TODO: Should limit values to IEEE precision of 52 bits
    } else if (value <= -TWO_63) {
      return MIN_VALUE;
    } else if (value + 1 >= TWO_63) {
      return MAX_VALUE;
    } else if (value < 0) {
      return fromNumber(-value).negate();
    } else {
      var hiBits = (value / TWO_32) | 0;
      return makeInt(
          (value - hiBits * TWO_32) | 0,
          hiBits);
    }
  }

  // TODO: Consider caching common Int values: All values -1 -> 1<<16
  // __lo and __hi are both signed 32 bit integer values.
  // Note that __lo can be negative, even for positive values.
  // Generally you want to use unsignedLoBits() when computing with the __lo bits.
  function Int(lo, hi) {
    this.__lo = (lo | 0);
    this.__hi = (hi | 0);
  }

  // Convert JavaScript Number to Skip Int
  function intToInt(value) {
    if (value < 0) {
      return makeInt(-value).negate();
    } else {
      if (value !== (value | 0)) {
        throw new Error('Invalid integer value');
      }
      return makeInt(value, 0);
    }
  }

  sk.__.intToInt = intToInt;
  sk.__.intToNumber = intToNumber;
  sk.__.makeInt = makeInt;
  sk.__.fromNumber = fromNumber;

  return {
    ctor: Int,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return this;
      },

      'eqImpl': function(other) {
        return sk.__.boolToBool(intEquals(this, other));
      },

      'neImpl': function(other) {
        return sk.__.boolToBool(!intEquals(this, other));
      },

      'plusImpl': function(other) {
        const thisLo = this.__lo;
        let loSum = thisLo + other.__lo | 0;
        let hiSum = this.__hi + other.__hi | 0;
        hiSum = hiSum + ((loSum ^ NEG_TWO_31) < (thisLo ^ NEG_TWO_31)) | 0;
        return makeInt(loSum, hiSum);
      },

      'minusImpl': function(other) {
        const thisLo = this.__lo;
        let loSum = thisLo - other.__lo | 0;
        let hiSum = this.__hi - other.__hi | 0;
        hiSum = hiSum - ((loSum ^ NEG_TWO_31) > (thisLo ^ NEG_TWO_31)) | 0;
        return makeInt(loSum, hiSum);
      },

      'timesImpl': function(other) {
        if (isZero(this) || isZero(other)) {
          return ZERO;
        }

        if (intEquals(this, MIN_VALUE)) {
          return isOdd(other) ? MIN_VALUE : ZERO;
        }
        if (intEquals(other, MIN_VALUE)) {
          return isOdd(this) ? MIN_VALUE : ZERO;
        }

        // ensure all values are positive
        const thisSign = sign(this);
        const otherSign = sign(other);
        const thisValue = thisSign === -1 ? this.negate() : this;
        const otherValue = otherSign === -1 ? other.negate() : other;
        const negateResult = (thisSign !== otherSign);

        // Divide each long into 4 chunks of 16 bits, and then add up 4x4 products.
        // We can skip products that would overflow.
        const a48 = thisValue.__hi >>> 16;
        const a32 = thisValue.__hi & 0xFFFF;
        const a16 = thisValue.__lo >>> 16;
        const a00 = thisValue.__lo & 0xFFFF;

        const b48 = otherValue.__hi >>> 16;
        const b32 = otherValue.__hi & 0xFFFF;
        const b16 = otherValue.__lo >>> 16;
        const b00 = otherValue.__lo & 0xFFFF;

        let c48 = 0, c32 = 0, c16 = 0, c00 = 0;
        c00 += a00 * b00;
        c16 += c00 >>> 16;
        c00 &= 0xFFFF;
        c16 += a16 * b00;
        c32 += c16 >>> 16;
        c16 &= 0xFFFF;
        c16 += a00 * b16;
        c32 += c16 >>> 16;
        c16 &= 0xFFFF;
        c32 += a32 * b00;
        c48 += c32 >>> 16;
        c32 &= 0xFFFF;
        c32 += a16 * b16;
        c48 += c32 >>> 16;
        c32 &= 0xFFFF;
        c32 += a00 * b32;
        c48 += c32 >>> 16;
        c32 &= 0xFFFF;
        c48 += a48 * b00 + a32 * b16 + a16 * b32 + a00 * b48;
        c48 &= 0xFFFF;
        const result = makeInt((c16 << 16) | c00, (c48 << 16) | c32);
        return negateResult ? result.negate() : result;
      },

      'unsafe_divImpl': function(other) {
        if (isZero(this)) {
          return ZERO;
        }

        // Handle MIN_VALUE specially
        if (intEquals(this, MIN_VALUE)) {
          if (intEquals(other, ONE) || intEquals(other, MINUS_ONE)) {
            return MIN_VALUE;
          } else if (intEquals(other, MIN_VALUE)) {
            return ONE;
          } else {
            const halfThis = this.shr(ONE);

            const approx = halfThis[divide](other).shl(ONE);
            if (isZero(approx)) {
              return isNegative(other) ? ONE : MINUS_ONE;
            } else {
              const remainder = this[minus](other[times](approx));
              return approx[plus](remainder[divide](other));
            }
          }
        } else if (intEquals(other, MIN_VALUE)) {
          return ZERO;
        }

        // Ensure both values are positive
        const thisSign = sign(this);
        const otherSign = sign(other);
        const thisValue = thisSign === -1 ? this.negate() : this;
        const otherValue = otherSign === -1 ? other.negate() : other;
        const negateResult = (thisSign !== otherSign);

        // Repeat the following until the remainder is less than other:  find a
        // floating-point that approximates remainder / other *from below*, add this
        // into the result, and subtract it from the remainder.  It is critical that
        // the approximate value is less than or equal to the real value so that the
        // remainder never becomes negative.
        let result = ZERO;
        let remainder = thisValue;
        while (compare(remainder, otherValue) >= 0) {
          // Approximate the result of division. This may be a little greater or
          // smaller than the actual value.
          let approx = Math.max(1, Math.floor(intToNumber(remainder) / intToNumber(otherValue)));

          // We will tweak the approximate result by changing it in the 48-th digit or
          // the smallest non-fractional digit, whichever is larger.
          const log2 = Math.ceil(Math.log(approx) / Math.LN2);
          const delta = (log2 <= 48) ? 1 : Math.pow(2, log2 - 48);

          // Decrease the approximation until it is smaller than the remainder.  Note
          // that if it is too large, the product overflows and is negative.
          let approxRes = fromNumber(approx);
          let approxRem = approxRes[times](otherValue);
          while (isNegative(approxRem) || (compare(approxRem, remainder) > 0)) {
            approx -= delta;
            approxRes = fromNumber(approx);
            approxRem = approxRes[times](otherValue);
          }

          // We know the answer can't be zero... and actually, zero would cause
          // infinite recursion since we would make no progress.
          if (isZero(approxRes)) {
            approxRes = ONE;
          }

          result = result[plus](approxRes);
          remainder = remainder[minus](approxRem);
        }

        return negateResult ? result.negate() : result;
      },

      'unsafe_remImpl': function(other) {
        // this - (this/other*other)
        return this[minus](this[divide](other)[times](other));
      },

      'negate': function() {
        // negate === ~value + 1
        const lo = (~this.__lo) + 1;
        const carry = (this.__lo === 0) ? 1 : 0;
        const hi = ~this.__hi + carry;
        return makeInt(lo, hi);
      },

      'ltImpl': function(other) {
        return sk.__.boolToBool(compare(this, other) < 0);
      },

      'gtImpl': function(other) {
        return sk.__.boolToBool(compare(this, other) > 0);
      },

      'leImpl': function(other) {
        return sk.__.boolToBool(compare(this, other) <= 0);
      },

      'geImpl': function(other) {
        return sk.__.boolToBool(compare(this, other) >= 0);
      },

      'andImpl': function(other) {
        return makeInt(this.__lo & other.__lo, this.__hi & other.__hi);
      },

      'orImpl': function(other) {
        return makeInt(this.__lo | other.__lo, this.__hi | other.__hi);
      },

      'xorImpl': function(other) {
        return makeInt(this.__lo ^ other.__lo, this.__hi ^ other.__hi);
      },

      'shlImpl': function(other) {
        const shift = other.__lo & 63;
        if (shift >= 32) {
          return makeInt(0, this.__lo << (shift - 32));
        } else if (shift == 0) {
          return this;
        } else {
          return makeInt(this.__lo << shift, (this.__hi << shift) | (this.__lo >>> (32 - shift)));
        }
      },

      'shrImpl': function(other) {
        const shift = other.__lo & 63;
        if (shift >= 32) {
          return makeInt(this.__hi >> (shift - 32), this.__hi >> 31)
        } else if (shift == 0) {
          return this;
        } else {
          return makeInt(this.__lo >>> shift | (this.__hi << (32 - shift)),
                         this.__hi >> shift);
        }
      },

      'ushrImpl': function(other) {
        const shift = other.__lo & 63;
        if (shift >= 32) {
          return makeInt(this.__hi >>> (shift - 32), 0)
        } else if (shift == 0) {
          return this;
        } else {
          return makeInt(this.__lo >>> shift | (this.__hi << (32 - shift)),
                         this.__hi >>> shift);
        }
      },

      'unsafe_chrImpl': function() {
        return sk.__.makeChar(intToNumber(this));
      },

      'toFloatImpl': function() {
        return sk.__.floatToFloat(intToNumber(this));
      },

      '__getSwitchValue': function() {
        return this.__lo;
      },
    },
    staticMembers: {}
  };
};
