/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

const webpack = require('webpack');

const config = {
  node: {
    fs: 'empty',
  },
  output: {
    library: 'skip',
    libraryTarget: 'amd'
  },
  externals: [
    'child_process'
  ]
};

module.exports = config;
