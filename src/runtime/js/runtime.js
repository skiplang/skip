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

// A stack of tasks, used to track function dependencies across async pause/resume points
let tasks = [];

// The arguments with which the program was invoked
// TODO: make this specific to each generated code bundle, similar to the sk object
let args = null;

// Whether to enable debugging (may log warnings)
let debugEnabled = false;

function getArguments() {
  if (args == null) {
    throw new Error('Skip: program arguments not set.');
  }
  return args;
}

function setArguments(newArgs) {
  args = newArgs;
}

function warn(msg) {
  if (debugEnabled) {
    console.warn(msg);
  }
}

function isInstance(object, cls) {
  return object.__bases.includes(cls);
}

function isDerivedException(sk, value) {
  return typeof value === 'function'
    && value.prototype != null
    && typeof value.prototype.getMessage === 'function'
    && value.prototype.__proto__ === sk.Exception.prototype;
}

function setDebug(debug) {
  debugEnabled = debug;
}

function makeExceptionsPrintStacks(sk, debug) {
  // Save this so exception.js can avoid generating stack traces when creating exceptions.
  sk.__.debug = debug;

  // This makes Error look enough like Sk.Exception that the Exception
  // handling in __main__ works for thrown JS exceptions
  Error.prototype.getMessage = function() {
    return new sk.String(this.message + '\n' + this.stack);
  }
  Error.prototype.__bases = [];

  if (debug) {
    // Default stack trace limit is 10
    Error.stackTraceLimit = Infinity;
    // Replace all overrides of Exception.getMessage() to include the stack trace
    // that was saved in Exception.constructor above.
    for (const key of Object.keys(sk)) {
      const cls = sk[key];
      if (isDerivedException(sk, cls)) {
        const old = cls.prototype.getMessage;
        cls.prototype.getMessage = function() {
          return new sk.String(old.call(this).__value + '\n' + this.__stack);
        };
      }
    }
  }
}

function isPrimitive(value) {
  switch (typeof value) {
    case 'number':
    case 'string':
    case 'boolean':
    case 'function':
    case 'undefined':
    case 'symbol':
      return true;
    case 'object':
      if (value === null) {
        return true;
      }
      const asString = Object.prototype.toString.call(value);
      return (
        // error.message and .stack aren't enumerable so we'd ordinarily lose
        // them in the copy. There's no way to write to an Error object from
        // Skip code so it's safe to just treat them as frozen.
        asString === '[object Error]' ||
        // Promises cannot be copied: the copy is not a valid `this` value for
        // Promise.prototype functions, leading to this error:
        // "Method Promise.prototype.then called on incompatible receiver #<Promise>"
        asString === '[object Promise]'
      );
    default:
      throw new Error('Unexpected host object');
  }
}

// Returns primitivies as-is, otherwise delegates to each type's
// generated __deepFreeze() method.
function deepFreeze(value, context) {
  if (isPrimitive(value)) {
    return value;
  } else {
    return value.__deepFreeze(context);
  }
}

/**
 * Determines if two lists of function arguments are equal per their
 * implementations of `Equality`.
 */
function areEqualArguments(a, b) {
  if (a.length !== b.length) {
    return false;
  }
  return a.every((aVal, index) => {
    const bVal = b[index];
    if (typeof aVal['$eq$eq'] !== 'function') {
      throw new Error(
        'Memoized functions may only accept arguments that implement `Equality` (`==`).'
      );
    }
    return aVal['$eq$eq'](bVal).__value;
  });
}

class ReactiveContext {
  constructor() {
    // autoincrementing id for fn/args identity
    this.id = 0;
    // global transaction id
    this.gtid = 0;
    // fn => args => id
    this.functionArguments = new Map();
    // object => (entry in this.functionArguments: fn => args => id)
    this.objectMethods = new Map();
    // id => {value: mixed, dependents: Set<id>}
    this.trackedValues = new Map();
    // id => [source, args]
    this.subscriptions = new Map();
    // autoincrementing id for tasks
    this.taskid = 0;
    // id => task
    this.tasks = new Map();
  }

  run(fn, args) {
    const taskid = this.taskid++;
    const task = new ReactiveTask(
      this,
      taskid,
      this.gtid,
      () => {
        this.tasks.delete(taskid);
        this.gc();
      }
    );
    this.tasks.set(taskid, task);
    return task.run(fn, args);
  }

  addSubscription(trackedID, subscriber, args) {
    this.subscriptions.set(trackedID, [subscriber, args]);
  }

  hasSubscription(trackedID) {
    return this.subscriptions.has(trackedID);
  }

  hasSubscriptions() {
    return this.subscriptions.size > 0;
  }

  gc() {
    // Find the oldest gtid still referenced by an active task
    let oldestGtid = this.gtid;
    this.tasks.forEach(task => {
      if (task.getId() < oldestGtid) {
        oldestGtid = task.getId();
      }
    });
    // Purge values older than that gtid
    this.trackedValues.forEach((trackedValue, trackedID) => {
      let value = trackedValue.next;
      while (value && value.gtid >= oldestGtid) {
        trackedValue = value;
        value = value.next;
      }
      trackedValue.next = null;
    });
    // todo: purge function/arg entries and subscriptions based on some criteria
    // such as last-accessed gtid
  }

  subscribe(callback) {
    const unsubscribers = [];
    this.subscriptions.forEach(([subscriber, args], trackedID) => {
      const unsubscribe = subscriber(
        () => {
          this.invalidateTrackedValue(trackedID);
          callback();
        },
        ...args
      );
      unsubscribers.push(unsubscribe);
    });
    return () => {
      unsubscribers.forEach(unsubscribe => {
        unsubscribe && unsubscribe();
      });
    };
  }

  // Create an identifer for the fn/args pair
  getTrackedIDForFunction(fn, args, thisValue) {
    let functionArguments = this.functionArguments;
    if (thisValue != null) {
      functionArguments = this.objectMethods.get(thisValue);
      if (!functionArguments) {
        functionArguments = new Map();
        this.objectMethods.set(thisValue, functionArguments);
      }
    }
    let argumentIDs = functionArguments.get(fn);
    if (!argumentIDs) {
      argumentIDs = new Map();
      functionArguments.set(fn, argumentIDs);
    }
    if (argumentIDs.size) {
      for (let trackedArgs of argumentIDs.keys()) {
        if (areEqualArguments(trackedArgs, args)) {
          return argumentIDs.get(trackedArgs);
        }
      }
    }
    const trackedID = this.id++;
    argumentIDs.set(args, trackedID);
    return trackedID;
  }

  // Invalidate the tracked value and any dependent values
  invalidateTrackedValue(trackedID) {
    this.gtid++;
    this.invalidateTrackedValueAtGtid(this.gtid, trackedID);
  }

  invalidateTrackedValueAtGtid(gtid, trackedID) {
    const trackedValue = this.trackedValues.get(trackedID);
    if (!trackedValue || trackedValue.gtid == gtid) {
      // Never computed or already invalidated by another dependency
      return;
    }
    // Push a new node to indicate that the value changed at this gtid
    this.trackedValues.set(trackedID, {
      dependents: new Set(),
      gtid,
      next: trackedValue,
      value: undefined,
    });
    // Invalidate dependents
    trackedValue.dependents.forEach(dependentID => {
      this.invalidateTrackedValueAtGtid(gtid, dependentID);
    });
  }

  _findTrackedValueAtGtid(gtid, trackedID) {
    let trackedValue = this.trackedValues.get(trackedID);
    let previousValue = null;
    while (trackedValue && gtid < trackedValue.gtid) {
      previousValue = trackedValue;
      trackedValue = trackedValue.next;
    }
    if (!trackedValue) {
      trackedValue = {
        dependents: new Set(),
        gtid,
        next: undefined,
        value: undefined,
      };
      if (previousValue) {
        previousValue.next = trackedValue;
      } else {
        this.trackedValues.set(trackedID, trackedValue);
      }
    }
    return trackedValue;
  }

  // Get the tracked value (undefined if not yet computed)
  getTrackedValue(gtid, trackedID) {
    return this._findTrackedValueAtGtid(gtid, trackedID).value;
  }

  // Set a tracked value without invalidating dependent values
  setTrackedValue(gtid, trackedID, value) {
    const trackedValue = this._findTrackedValueAtGtid(gtid, trackedID);
    trackedValue.value = value;
  }

  // Record a dependency between a parent/child pair of values
  addTrackedValueDependency(gtid, dependentID, dependencyID) {
    const trackedValue = this._findTrackedValueAtGtid(gtid, dependencyID);
    trackedValue.dependents.add(dependentID);
  }

  inspect() {
    let str = 'functions:\n';
    Array.from(this.functionArguments.entries()).forEach(([fn, argumentIDs]) => {
      Array.from(argumentIDs.entries()).forEach(([args, id]) => {
        const subscribed = this.subscriptions.has(id);
        str += '  ' + (fn.name || '[anonymous function]');
        str += '(' + args.map(arg => arg.toString && arg.toString().__value).join(', ') + ')';
        str += ' => ' + id + (subscribed ? '*' : '') + '\n';
      });
    });
    str += '\nvalues:\n';
    Array.from(this.trackedValues.entries()).forEach(([trackedID, trackedValue]) => {
      str += '  ' + trackedID + ':\n';
      while (trackedValue) {
        const val = trackedValue.value ? trackedValue.value.toString().__value : '(not calculated)';
        str += '    ' + trackedValue.gtid + '=' + val + (val && val.endsWith('\n') ? '' : '\n');
        trackedValue = trackedValue.next;
      }
    });
    return str;
  }
}

class ReactiveTask {
  constructor(context, taskid, gtid, onComplete) {
    this.context = context;
    this.gtid = gtid;
    this.taskid = taskid;
    this.onComplete = onComplete;
    this.trackedIDs = [];
  }

  run(fn, args) {
    enterTask(this);
    const result = Promise.resolve(fn(...args)).then(
      value => {
        this.onComplete();
        return value;
      },
      error => {
        this.onComplete();
        throw error;
      }
    );
    exitTask();
    return result;
  }

  getContext() {
    return this.context;
  }

  getId() {
    return this.taskid;
  }

  pushTrackedID(trackedID) {
    this.trackedIDs.push(trackedID);
  }

  popTrackedID() {
    this.trackedIDs.pop();
  }

  getTrackedID() {
    return this.trackedIDs[this.trackedIDs.length - 1];
  }
}

function createReactiveContext() {
  return new ReactiveContext();
}

function getCurrentTask() {
  return tasks[tasks.length - 1];
}

function enterTask(task) {
  tasks.push(task);
}

function exitTask() {
  tasks.pop();
}

module.exports = {
  // export to user
  makeExceptionsPrintStacks,
  // export to generated code
  createReactiveContext,
  deepFreeze,
  enterTask,
  exitTask,
  getCurrentTask,
  isInstance,
  getArguments,
  setArguments,
  setDebug,
  warn,
};
