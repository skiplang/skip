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

  // constructor
  function SkArray(value) {
    this.__value = value;
  }

  function arraySize(array) {
    return array.__value.length;
  }

  return {
    ctor: SkArray,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return new SkArray(this.__value.map(v => v.__deepFreeze(cache)));
      },
      'size': function() {
        return sk.__.intToInt(arraySize(this));
      },
      'sizeImpl': function() {
        return sk.__.intToInt(arraySize(this));
      },
      'unsafe_set': function(index, value) {
        this.__value[sk.__.intToNumber(index)] = value;
      },
      'unsafe_get': function(index) {
        return this.__value[sk.__.intToNumber(index)];
      },
      // NOTE: Replaces Sk version for perf.
      'get': function(index) {
        index = sk.__.intToNumber(index);
        if (index < 0 || index >= arraySize(this)) {
          throw new Error("TODO: OutOfBounds");
        }
        return this.__value[index];
      },

      'chill': function() {
        const size = arraySize(this);
        const result = new Array(size);
        for (let index = 0; index < size; index++) {
          result[index] = this.__value[index];
        }
        return new SkArray(result);
      },
    },
    staticMembers: {
      'unsafe_make': function(length) {
        return new SkArray(new Array(sk.__.intToNumber(length)));
      },

      // NOTE: This replaces the Sk version for perf.
      'mfillBy': function(size, f) {
        size = sk.__.intToNumber(size);
        if (size < 0) {
          throw new Error("Called Array::mfill with negative number");
        }
        const result = Array(size);
        for (let index = 0; index < size; index++) {
          result[index] = f(sk.__.intToInt(index));
        }
        return new SkArray(result);
      },
    }
  };
};
