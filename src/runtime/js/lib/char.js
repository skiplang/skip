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
  function Char(code) {
    this.__value = String.fromCodePoint(code);
  }

  // Cache chars so that there is only one of each value ever created
  const ARRAY_CACHE_SIZE = 128;
  const arrayCache = new Array(ARRAY_CACHE_SIZE);
  const mapCache = new Map();

  for(var i = 0; i < ARRAY_CACHE_SIZE; i++) {
    arrayCache[i] = new Char(i);
  }

  // makeChar is copied to a local var $makeChar in the generated code for perf
  sk.__.makeChar = function (code) {
    if (code < ARRAY_CACHE_SIZE) {
      return arrayCache[code];
    } else {
      let result = mapCache.get(code);
      if (result == null) {
        result = new Char(code);
        mapCache.set(code, result)
      }
      return result;
    }
  }

  return {
    ctor: Char,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return this;
      },
      '==': function(other) {
        return sk.__.boolToBool(this.__value === other.__value);
      },
      '!=': function(other) {
        return sk.__.boolToBool(this.__value !== other.__value);
      },
      '<': function(other) {
        return sk.__.boolToBool(this.__value < other.__value);
      },
      '>': function(other) {
        return sk.__.boolToBool(this.__value > other.__value);
      },
      '<=': function(other) {
        return sk.__.boolToBool(this.__value <= other.__value);
      },
      '>=': function(other) {
        return sk.__.boolToBool(this.__value >= other.__value);
      },
      'code': function() {
        return sk.__.intToInt(this.__value.codePointAt(0));
      },
      '__getSwitchValue': function() {
        return this.__value.codePointAt(0);
      },
    },
    staticMembers: {},
  };
};
