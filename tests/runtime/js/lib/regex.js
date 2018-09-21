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
  return {
    ctor: function() {},
    instanceMembers: {},
    staticMembers: {
      'createWithoutCheck': function(pattern, flags) {
        return new RegExp(pattern.__value, 'u' + (flags ? flags.__value : ''));
      }
    }
  };
};
