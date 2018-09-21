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
  // Maps JS strings to Skip strings
  // Caches all small string values.
  const stringCache = new Map();

  const StringUtf8View = sk.__.manglePropertyName('String.Utf8View');

  // constructor
  function String_(value, hasCombiningCharsOpt) {
    /* TODO: Consider enabling this?
    const cachedResult = stringCache.get(value)
    if (cachedResult != null) {
      return cachedResult;
    }
    */

    this.__value = value;
    /* TODO: Enable string cache
    if (value.length < 20) {
      stringCache.set(value, this);
    }
    */
    this.__hasCombiningChars = hasCombiningCharsOpt;
    this.__utf8 = null;
  }

  function ensureHasCombiningChars(stringValue) {
    if (stringValue.__hasCombiningChars === undefined) {
      stringValue.__hasCombiningChars = hasCombiningChars(stringValue.__value);
    }
  }

  function getHasCombiningChars(stringValue) {
    ensureHasCombiningChars(stringValue);
    return stringValue.__hasCombiningChars;
  }

  function unsafe_get(stringValue, index) {
    return sk.__.makeChar(codePointAt(stringValue.__value, getHasCombiningChars(stringValue), index));
  }

  // TODO: throw OutOfBounds()
  function throwOutOfBounds(index) {
    throw Error("Out of bounds in String native at offset: " + index);
  }

  // See ECMAScript Spec - String.prototype.codePointAt
  function lengthOfCharAt(value, index) {
    return value.codePointAt(index) >= 0x10000 ? 2 : 1;
  }

  function hasCombiningChars(value) {
    for (let i = 0 ; i < value.length; i++) {
      if (lengthOfCharAt(value, i) !== 1) {
        return true;
      }
    }
    return false;
  }

  function indexOfChar(value, hasCombiningChars, charIndex) {
    if (!hasCombiningChars) {
      return charIndex;
    } else {
      let index = 0
      while (charIndex > 0) {
        index += lengthOfCharAt(value, index);
        charIndex -= 1;
      }
      return index;
    }
  }

  function codePointAt(value, hasCombiningChars, charIndex) {
    return value.codePointAt(indexOfChar(value, hasCombiningChars, charIndex));
  }

  function length(value, hasCombiningChars) {
    if (!hasCombiningChars) {
      return value.length;
    } else {
      let result = 0;
      for (let i = 0; i < value.length; i += lengthOfCharAt(value, i)) {
        result += 1;
      }
      return result;
    }
  }

  function lengthOfString(stringValue) {
    return length(stringValue.__value, getHasCombiningChars(stringValue))
  }

  function stringToString(str) {
    return new String_(str);
  }

  function addGlobalFlag(regex) {
    return new RegExp(regex.source, regex.flags + 'g');
  }

  // Convert a JS integer to a Skip UInt8
  const toUInt8 = sk.__.toUInt8;

  // Convert a Skip String to a Skip Array<UInt8>
  // Adapted from Google Closure Library:
  // https://github.com/google/closure-library/blob/ccdb3bd5c80094bc65a56053aa3ea782e2711b62/closure/goog/crypt/crypt.js#L117
  function stringToUtf8(string) {
    const str = string.__value;
    const result = [];
    let index = 0;
    for (var i = 0; i < str.length; i++) {
      const c = str.charCodeAt(i);
      if (c < 128) {
        result[index++] = toUInt8(c);
      } else if (c < 2048) {
        result[index++] = toUInt8((c >> 6) | 192);
        result[index++] = toUInt8((c & 63) | 128);
      } else if (
          ((c & 0xFC00) == 0xD800) && (i + 1) < str.length &&
          ((str.charCodeAt(i + 1) & 0xFC00) == 0xDC00)) {
        // Surrogate Pair
        c = 0x10000 + ((c & 0x03FF) << 10) + (str.charCodeAt(++i) & 0x03FF);
        result[index++] = toUInt8((c >> 18) | 240);
        result[index++] = toUInt8(((c >> 12) & 63) | 128);
        result[index++] = toUInt8(((c >> 6) & 63) | 128);
        result[index++] = toUInt8((c & 63) | 128);
      } else {
        result[index++] = toUInt8((c >> 12) | 224);
        result[index++] = toUInt8(((c >> 6) & 63) | 128);
        result[index++] = toUInt8((c & 63) | 128);
      }
    }
    return new sk.Array(result);
  }

  // Convert a Skip UInt8 to a JS number
  const fromUInt8 = x => sk.__.intToNumber(x.toInt());

  // Convert a Skip Array<UInt8> to a Skip String
  // Adapted from Google Closure Library:
  // https://github.com/google/closure-library/blob/ccdb3bd5c80094bc65a56053aa3ea782e2711b62/closure/goog/crypt/crypt.js#L151
  function stringFromUtf8(array) {
    const bytes = array.__value;
    const result = [];
    let byteIndex = 0;
    let resultIndex = 0;
    let hasCombiningChars = false;
    while (byteIndex < bytes.length) {
      var c1 = fromUInt8(bytes[byteIndex++]);
      if (c1 < 128) {
        result[resultIndex++] = String.fromCharCode(c1);
      } else if (c1 > 191 && c1 < 224) {
        var c2 = fromUInt8(bytes[byteIndex++]);
        result[resultIndex++] = String.fromCharCode((c1 & 31) << 6 | c2 & 63);
      } else if (c1 > 239 && c1 < 365) {
        // Surrogate Pair
        hasCombiningChars = true;
        var c2 = fromUInt8(bytes[byteIndex++]);
        var c3 = fromUInt8(bytes[byteIndex++]);
        var c4 = fromUInt8(bytes[byteIndex++]);
        var u = ((c1 & 7) << 18 | (c2 & 63) << 12 | (c3 & 63) << 6 | c4 & 63) -
            0x10000;
        result[resultIndex++] = String.fromCharCode(0xD800 + (u >> 10));
        result[resultIndex++] = String.fromCharCode(0xDC00 + (u & 1023));
      } else {
        var c2 = fromUInt8(bytes[byteIndex++]);
        var c3 = fromUInt8(bytes[byteIndex++]);
        result[resultIndex++] =
            String.fromCharCode((c1 & 15) << 12 | (c2 & 63) << 6 | c3 & 63);
      }
    }
    return new String_(result.join(''), hasCombiningChars)
  }

  sk.__.stringToString = stringToString;

  return {
    ctor: String_,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return this;
      },
      'unsafe_get': function(x) {
        return unsafe_get(this, sk.__.intToNumber(x));
      },
      'length': function() {
        return sk.__.intToInt(lengthOfString(this));
      },
      'concat': function(other) {
        const hasCombiningChars = (this.__hasCombiningChars === undefined || other.__hasCombiningChars == undefined)
          ? undefined
          : this.__hasCombiningChars || other.__hasCombiningChars;
        return new String_(this.__value + other.__value, hasCombiningChars);
      },
      'contains': function(other) {
        return new sk.__.boolToBool(this.__value.includes(other.__value));
      },
      'toFloat_raw': function() {
        return new sk.Float(Number(this.__value));
      },
      'compare_raw': function(other) {
        // Note: Don't use localeCompare as it does strange things for punctuation
        let result = this.__value == other.__value ? 0
          : (this.__value < other.__value ? -1 : 1);
        return sk.__.intToInt(result);
      },
      '__getSwitchValue': function() {
        return this.__value;
      },
      'sub': function(start, len) {
        start = sk.__.intToNumber(start);
        len = sk.__.intToNumber(len);
        const this_len = lengthOfString(this);
        if(start < 0 || len < 0 || (start + len) > this_len) {
          throwOutOfBounds(start + len);
        }
        if (start === 0 && len === this_len) {
          return this;
        }
        const startIndex = indexOfChar(this.__value, getHasCombiningChars(this), start);
        const endIndex = indexOfChar(this.__value, getHasCombiningChars(this), start + len);
        return new String_(this.__value.substring(startIndex, endIndex), len !== (endIndex - startIndex));
      },
      'get': function(index) {
        index = sk.__.intToNumber(index);
        if (index < 0 || index >= lengthOfString(this)) {
          throwOutOfBounds(index);
        }
        return unsafe_get(this, index);
      },
      'searchIndex': function(offset, f) {
        offset = sk.__.intToNumber(offset);
        const this_len = lengthOfString(this);
        while (offset >= 0 && offset < this_len) {
          if (f(unsafe_get(this, offset)).__value) {
            return sk.__.intToInt(offset);
          }
          offset += 1;
        }
        return sk.__.intToInt(-1);
      },

      // Regex support
      'matches': function(regex) {
        return new sk.__.boolToBool(regex.test(this.__value));
      },
      'match': function(regex) {
        var res = this.__value.match(regex);
        if (res === null) {
          return new sk.None();
        }
        return new sk.Some(
          new sk.Regex$dtMatch(
            new sk.Array(res.map(x => new sk.String(x)))
          )
        );
      },
      'matchAll': function(regex) {
        var re = addGlobalFlag(regex);
        var res = [];
        var elem;
        while ((elem = re.exec(this.__value)) !== null) {
          res.push(
            new sk.Regex$dtMatch(
              new sk.Array(elem.map(x => new sk.String(x)))
            )
          );
        }
        return new sk.Array(res).values();
      },
      'replaceRegex': function(regex, cb) {
        var re = addGlobalFlag(regex);
        return new sk.String(
          this.__value.replace(re, function() {
            return cb(
              new sk.Array(
                [].slice.apply(arguments)
                  // Last two arguments are length and original string
                  .slice(0, -2)
                  .map(x => new sk.String(x))
              )
            ).__value;
          })
        );
      },
      'splitRegex': function(regex) {
        return new sk.Array(
          this.__value.split(regex).map(x => new sk.String(x))
        ).values();
      },
      'utf8': function() {
        // lazily initialize the utf8 view
        let utf8 = this.__utf8;
        if (utf8 === null) {
          utf8 = this.__utf8 = new sk[StringUtf8View](stringToUtf8(this));
        }
        return utf8;
      },
    },
    staticMembers: {
      'fromChars': function(chars) {
        let inner_chars = chars.__value;
        let res = inner_chars.map(ch => ch.__value).join('');
        return new String_(res, res.length !== inner_chars.length);
      },
      'fromUtf8': stringFromUtf8,
    }
  };
};
