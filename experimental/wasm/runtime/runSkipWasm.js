/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */


const skWasmUtils = require('./nodeRuntime')

function usage() {
  console.error('usage: node runSkWasm <FILE>.wasm')
  process.exit(1)
}

if (process.argv.length < 2) {
  usage()
}
const wasmTarget = process.argv[2]

skWasmUtils.load(wasmTarget)
  .then(ctx => {
    const main = ctx.instance.exports['skip_main']
    const ret = main()
    const mainRetStr = skWasmUtils.decodeSkipString(ctx.env, ret)
    console.log(mainRetStr)
  })
  .catch(err => {
    console.log(err)
  })
