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

const fs = require('fs');
const path = require('path');
const child_process = require('child_process');
const {getArguments} = require('../runtime');
const {createReactiveFunction} = require('../function_decorators');

module.exports = function(sk) {
  const FileProvider = {
    get(filename) {
      return new sk.String(fs.readFileSync(filename.__value, 'utf8'));
    },
    subscribe(callback, filename) {
      let prevMtime = fs.statSync(filename.__value).mtime.getTime();
      const intervalId = setInterval(() => {
        const nextMtime = fs.statSync(filename.__value).mtime.getTime();
        if (nextMtime > prevMtime) {
          prevMtime = nextMtime;
          callback();
        }
      }, 250);
      return () => clearInterval(intervalId);
    },
  };

  const open_file = createReactiveFunction(FileProvider);

  function string_to_file(value, fileName) {
    fs.writeFileSync(fileName.__value, value.__value);
  }

  function appendTextFile(fileName, value) {
    fs.appendFileSync(fileName.__value, value.__value);
  }

  function skipKeysOfObject(object) {
    // Filter out $frozen but not $atparamX
    return Object.keys(object).filter(key => key.startsWith("$atparam") || !key.startsWith("$"));
  }

  function inspect(value) {
    if (Array.isArray(value)) {
      return new sk.InspectCall(
        new sk.String(''),
        new sk.Array(value.map(elem => inspect(elem)))
      );
    }

    if (typeof value === 'object' && value.inspect) {
      return value.inspect();
    }

    if (typeof value === 'object' && value.__classname === undefined) {
      return new sk.InspectLiteral(new sk.String("void"));
    } else if (typeof value === 'object') {
      if (value instanceof Error) {
        return new sk.InspectCall(
          new sk.String("Error"),
          new sk.Array([inspect(new sk.String(value.toString()))])
        );
      }
      // Big hack :)
      var isNamed = value.__constructor.toString().indexOf('__named_parameters') !== -1;

      if (isNamed) {
        return new sk.InspectObject(
          new sk.String(value.__classname),
          new sk.Array(skipKeysOfObject(value).sort().map(key => new sk.Tuple2(
            new sk.String(key),
            inspect(value[key])
          )))
        );
      } else {
        return new sk.InspectCall(
          new sk.String(value.__classname),
          new sk.Array(skipKeysOfObject(value).map(key => inspect(value[key])))
        );
      }
    }

    if (typeof value === 'string') {
      return new sk.InspectString(new sk.String(value));
    }

    if (typeof value === 'function') {
      if (value.__classname) {
        return new sk.InspectSpecial(new sk.String('class'));
      }
      return new sk.InspectSpecial(new sk.String('lambda'));
    }

    return new sk.InspectSpecial(
      new sk.String('unknown: ' + typeof value + ' ' + value)
    );
  }

  // NOTE: write truncates strings > 2^16
  const MAX_WRITE_SIZE = 1 << 15;
  function print_stream_js(stream, str) {
    while (str.length > 0) {
      stream.write(str.substr(0, MAX_WRITE_SIZE));
      str = str.substr(MAX_WRITE_SIZE);
    }
  }

  function testIsNode() {
    try {
      // Apparently this is how its done ...
      // ... no I am not making this up
      return process.stdout != null;
    } catch (e) {
      return false;
    }
  };

  const isNode = testIsNode();

  function print_stdout_browser(str) {
    console.log(str);
  }

  function print_stdout_node(str) {
    print_stream_js(process.stdout, str);
  }

  function print_stderr_browser(str) {
    console.error(str);
  }

  function print_stderr_node(str) {
    print_stream_js(process.stderr, str);
  }

  let print_stdout_js = isNode ? print_stdout_node : print_stdout_browser;
  let print_stderr_js = isNode ? print_stderr_node : print_stderr_browser;

  function print_stdout(str) {
    print_stdout_js(str.__value);
  }

  function print_stderr(str) {
    print_stderr_js(str.__value);
  }

  let jsNs;
  if (typeof performance !== 'undefined') {
    jsNs = () => performance.now() * 1000000; // ms -> ns
  } else if (typeof process !== 'undefined') {
    jsNs = function() {
      const time = process.hrtime(); // [seconds, nanoseconds]
      return time[0] * 1000000000 + time[1];
    }
  } else {
    jsNow = function() {
      throw new Error('now() not available');
    }
  }

  // return profiling timestamp in ns
  function nowNanos() {
    return sk.__.fromNumber(jsNs());
  }

  // all durations in ns
  let profile_total = 0;
  let profile_started = null;
  let profile_records = [];
  function profile_start() {
    if (profile_started !== null) {
      throw new Error('Profile already started.');
    }
    profile_started = jsNs();
    profile_total = 0;
  }

  function profile_stop() {
    const time = jsNs();
    if (profile_started === null) {
      throw new Error('Profile not started.');
    }
    const total = profile_total + (time - profile_started);
    const total_ms = total / 1000000;
    profile_started = null;
    profile_total = 0;
    profile_records.push(total_ms.toString());
    return new sk.Float(total_ms);
  }

  function profile_report() {
    const nl = new sk.String('\n');
    profile_records.forEach(function(t) {
      print_stdout(new sk.String(t));
      print_stdout(nl);
    });
    profile_records.length = 0;
  }

  function print_raw(value) {
    print_stdout(value);
  }

  function print_error(value) {
    print_stderr(value);
  }

  function arguments_() {
    return new sk.Array(getArguments().map(arg => new sk.String(arg)));
  }

  function print_last_exception_stack_trace_and_exit(error) {
    if (error.__stack !== "") {
      print_stdout_js(error.__stack.toString() + '\n');
    }
  }

  function sk_internal_exit(value) {
    process.exit(sk.__.intToNumber(value));
  }

  function debug_break() {
    debugger;
  }

  function print_stack_trace() {
    const stack = (new Error()).stack;
    print_stdout_js(stack.toString() + '\n');
  }

  function getcwd() {
    return new sk.String(process.cwd());
  }

  function getBuildVersion() {
    return new sk.String("Not supported.");
  }

  // Filename
  function basename(filename) {
    return new sk.String(path.basename(filename.__value));
  }

  function dirname(filename) {
    return new sk.String(path.dirname(filename.__value));
  }

  function join(dirname, filename) {
    return new sk.String(path.join(dirname.__value, filename.__value));
  }

  // FileSystem
  function exists(filename) {
    return sk.__.boolToBool(fs.existsSync(filename.__value));
  }

  function is_directory(filename) {
    return sk.__.boolToBool(
      fs.existsSync(filename.__value) && fs.lstatSync(filename.__value).isDirectory());
  }

  function ensure_directory(dirname) {
    var dir = dirname.__value;
    if (!fs.existsSync(dir)){
      fs.mkdirSync(dir);
    }
    return null;
  }

  function readdir(dirname) {
    return new sk.Array(
      fs.readdirSync(dirname.__value)
        .map(filename => new sk.String(filename)));
  }

  // Subprocess
  const SubprocessOutput = sk[sk.__.manglePropertyName('Subprocess.Output')];
  const toUInt8 = sk.__.toUInt8;

  function bufferToUInt8Array(buffer) {
    var value = [];
    for (let index = 0; index < buffer.length; index++) {
      value.push(toUInt8(buffer.readUInt8(index)))
    }
    return new sk.Array(value);
  }

  function spawnHelper(skipArgs) {
    var cmd = skipArgs.__value[0].__value;
    var args = skipArgs.__value.slice(1).map(x => x.__value);
    var result = child_process.spawnSync(cmd, args);
    var returnCode = sk.__.intToInt(result.status);
    var stdout = bufferToUInt8Array(result.stdout);
    var stderr = bufferToUInt8Array(result.stderr);
    return new SubprocessOutput({
      returnCode,
      stdout,
      stderr
    });
  }

  // Other
  function read_stdin() {
    return new sk.String(fs.readFileSync(0 /* stdin */));
  }

  function awaitSynchronously(awaitHandle) {
    return awaitHandle.then(() => {});
  }

  sk.__.defineGlobalFunction('arguments', arguments_);
  sk.__.defineGlobalFunction('awaitSynchronously', awaitSynchronously);
  sk.__.defineGlobalFunction('debug_break', debug_break);
  sk.__.defineGlobalFunction('inspect', inspect);
  sk.__.defineGlobalFunction('internalExit', sk_internal_exit);
  sk.__.defineGlobalFunction('nowNanos', nowNanos);
  sk.__.defineGlobalFunction('profile_start', profile_start);
  sk.__.defineGlobalFunction('profile_stop', profile_stop);
  sk.__.defineGlobalFunction('profile_report', profile_report);
  sk.__.defineGlobalFunction('open_file', open_file);
  sk.__.defineGlobalFunction('print_error', print_error);
  sk.__.defineGlobalFunction('print_last_exception_stack_trace_and_exit', print_last_exception_stack_trace_and_exit);
  sk.__.defineGlobalFunction('print_raw', print_raw);
  sk.__.defineGlobalFunction('string_to_file', string_to_file);
  sk.__.defineGlobalFunction('print_stack_trace', print_stack_trace);
  sk.__.defineGlobalFunction('getcwd', getcwd);
  sk.__.defineGlobalFunction('Filename.basename', basename);
  sk.__.defineGlobalFunction('Filename.dirname', dirname);
  sk.__.defineGlobalFunction('Filename.join', join);
  sk.__.defineGlobalFunction('FileSystem.appendTextFile', appendTextFile);
  sk.__.defineGlobalFunction('FileSystem.exists', exists);
  sk.__.defineGlobalFunction('FileSystem.is_directory', is_directory);
  sk.__.defineGlobalFunction('FileSystem.ensure_directory', ensure_directory);
  sk.__.defineGlobalFunction('FileSystem.readdir', readdir);
  sk.__.defineGlobalFunction('Subprocess.spawnHelper', spawnHelper);
  sk.__.defineGlobalFunction('read_stdin', read_stdin);
};
