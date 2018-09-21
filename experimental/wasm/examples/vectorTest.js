/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */


const skWasmUtils = require('../runtime/nodeRuntime')

const wasmTarget = 'vectorTest.wasm'

skWasmUtils.load(wasmTarget)
  .then(ctx => {
    const mkVec = ctx.instance.exports['mkVec']
    const vec = mkVec(5)
    console.log('mkVec(5) --> ', vec)
    const data = skWasmUtils.getRawData(ctx.env, vec, 5 * 8)
    console.log('vec data:', data)
  })
  .catch(err => {
    console.log(err)
  })
