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
  sk.__.defineGlobalFunction('Unsafe.array_make', function(size) {
    return sk.Array.unsafe_make(size);
  });
  sk.__.defineGlobalFunction('Unsafe.array_get', function(v, i) {
    return v.unsafe_get(i);
  });
  sk.__.defineGlobalFunction('Unsafe.array_set', function(v, i, x) {
    v.unsafe_set(i, x);
  });
}
