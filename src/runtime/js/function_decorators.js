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

const {enterTask, exitTask, getCurrentTask} = require('./runtime');

// From: https://github.com/zertosh/async-to-generator/blob/master/async-to-generator.js
function asyncToGenerator(fn) {
  return function() {
    // Store the task that created the generator
    const task = getCurrentTask();
    let gen = fn.apply(this, arguments);
    return new Promise(function(resolve, reject) {
      function step(key, arg) {
        let info;
        let value;
        // Restore the task prior to executing the generator function,
        // as `step` may be executing asynchronously such that the original
        // task is no longer at the top of the stack. Because
        // generators/async functions may be arbitrarily nested, each call to
        // one must be wrapped in calls to enter/leave the task.
        task && enterTask(task);
        try {
          info = gen[key](arg);
          value = info.value;
        } catch (error) {
          reject(error);
          return;
        } finally {
          task && exitTask();
        }
        if (info.done) {
          resolve(value);
        } else {
          return Promise.resolve(value).then(function(value) {
            step('next', value);
          }, function(err) {
            step('throw', err);
          });
        }
      }
      return step('next');
    });
  };
}

/**
 * Creates a tracked function that also records subscriptions to external data
 * sources.
 */
function createReactiveFunction(provider) {
  return function(...args) {
    const task = getCurrentTask();
    if (!task) {
      return provider.get(...args);
    }
    const ctx = task.getContext();

    // Get an identifier for the results of this function/args pair
    const trackedID = ctx.getTrackedIDForFunction(provider.get, args, null);
    // Associate this tracked value as a dependency of the parent tracked value
    const dependentID = task.getTrackedID();
    ctx.addTrackedValueDependency(task.gtid, dependentID, trackedID);

    // (Re)compute the value as necessary
    let value = ctx.getTrackedValue(task.gtid, trackedID);
    if (value === undefined) {
      value = provider.get(...args);
      ctx.setTrackedValue(task.gtid, trackedID, value);
    }
    // Add a subscription for the value
    if (!ctx.hasSubscription(trackedID)) {
      ctx.addSubscription(trackedID, provider.subscribe, args);
    }
    return value;
  }
}

/**
 * Composes a function, returning a new function that behaves
 * normally when run in standard mode and tracks function dependencies when run
 * in reactive mode.
 */
function createTrackedFunction(fn, isMethod) {
  return function(...args) {
    const task = getCurrentTask();
    if (!task) {
      return fn.apply(this, args);
    }
    const ctx = task.getContext();

    // Create a tracked value for the function/arguments pair
    const thisValue = isMethod ? this : null;
    const trackedID = ctx.getTrackedIDForFunction(fn, args, thisValue);
    // Associate this tracked value as a dependency of the parent tracked value
    const dependentID = task.getTrackedID();
    ctx.addTrackedValueDependency(task.gtid, dependentID, trackedID);

    // (Re)compute the value as necessary
    let value = ctx.getTrackedValue(task.gtid, trackedID);
    if (value === undefined) {
      // Set this tracked value as the parent during its execution
      task.pushTrackedID(trackedID);
      value = fn.apply(this, args);
      // Reset the parent tracked value
      task.popTrackedID();
      ctx.setTrackedValue(task.gtid, trackedID, value);
    }
    return value;
  }
}

module.exports = {
  asyncToGenerator,
  createReactiveFunction,
  createTrackedFunction,
};
