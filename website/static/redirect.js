/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

if (location.hash.substring(1).endsWith("==")) {
  location.pathname = "/playground/";
}
if (location.hostname.endsWith('.com') && !location.hostname.endsWith('skiplang.com')) {
  location.hostname = 'skiplang.com';
}
document.querySelector('header .logo').oncontextmenu = function(e) {
  location.href = '/docs/assets.html';
  return false;
}
