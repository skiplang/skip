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

  // constructor: unused
  function RawStorage(value) {}

  const uninitializedRawStorage = {
    __deepFreeze: function(cache) {
      return this;
    },
  };

  return {
    ctor: RawStorage,
    instanceMembers: {
    },
    staticMembers: {
      'uninitialized': function() {
        return uninitializedRawStorage;
      },

      'make': function(value) {
        return value;
      },
      'unsafeGet': function(value) {
        if (value === uninitializedRawStorage) {
          throw new Error('RawStorage::unsafeGet() called on uninitialized value.');
        }
        return value;
      },
    }
  };
};
