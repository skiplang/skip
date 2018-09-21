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
  function Void() {
  }

  const void_ = new Void();

  sk.__.$void = void_;

  return {
    ctor: Void,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return this;
      },
    },
    staticMembers: {}
  };
}
