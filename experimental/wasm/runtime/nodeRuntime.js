/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

'use strict';

const fs = require('fs'),
      assert = require('assert'),
      path = require('path'),
      utils = require('./common/utils')

const jsRunnerPath = process.argv[1]
const jsRunnerDir = path.dirname(jsRunnerPath)
const skipDir = path.normalize(path.join(jsRunnerDir,'../../..'))

// Convert node Buffer to Uint8Array
function toUint8Array(buf) {
  var u = new Uint8Array(buf.length)
  for (var i = 0; i < buf.length; ++i) {
    u[i] = buf[i]
  }
  return u
}

const runtimeWasmPath = 'third-party/assemblyscript-runtime/dist/runtime.wasm'

const load = (wasmFile) => {
  /* load and initialize the mem mgmt runtime: */
  const runtimeWasm = path.join(skipDir, runtimeWasmPath)
  const runtimeFileBuf = fs.readFileSync(runtimeWasm)
  const wasmFileBuf = fs.readFileSync(wasmFile)
  return utils.load(toUint8Array(runtimeFileBuf), toUint8Array(wasmFileBuf))
}

/* for debugging the runtime only: */
const loadRuntime = () => {
  /* load and initialize the mem mgmt runtime: */
  const runtimeWasm = path.join(skipDir, runtimeWasmPath)
  const runtimeFileBuf = fs.readFileSync(runtimeWasm)
  return utils.loadRuntime(toUint8Array(runtimeFileBuf))
}

// Hmmm: If we end up exporting more than decodeSkipString
// may want to just re-export utils...
module.exports = {
  load,
  loadRuntime,
  decodeSkipString: utils.decodeSkipString,
  getRawData: utils.getRawData,
  getRawInt: utils.getRawInt
}
