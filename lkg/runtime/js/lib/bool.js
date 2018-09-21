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
  function Bool(value) {
    this.__value = value;
  }

  const true_ = new Bool(true);
  const false_ = new Bool(false);

  function boolToBool(value) {
    return value ? true_ : false_;
  }

  sk.__.boolToBool = boolToBool;

  return {
    ctor: Bool,
    instanceMembers: {
      '__deepFreeze': function(cache) {
        return this;
      },
      '!': function() {
        return boolToBool(!this.__value);
      },
      '==': function(other) {
        return boolToBool(this.__value === other.__value);
      },
      '!=': function(other) {
        return boolToBool(this.__value !== other.__value);
      },
      '<': function(other) {
        return boolToBool(this.__value < other.__value);
      },
      '>': function(other) {
        return boolToBool(this.__value > other.__value);
      },
      '<=': function(other) {
        return boolToBool(this.__value <= other.__value);
      },
      '>=': function(other) {
        return boolToBool(this.__value >= other.__value);
      },
      '__getSwitchValue': function() {
        return this.__value;
      },
    },
    staticMembers: {}
  };
}
