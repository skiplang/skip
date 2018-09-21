/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */


loadWasmApp('addTwo.wasm')
.then(ctx => {
  console.log('load Wasm complete: ', ctx)
  const main = ctx.instance.exports['sk.main']
  const ret = main()
  // const mainRetStr = skWasmUtils.decodeSkipString(ctx.env, ret)
  // console.log(mainRetStr)
})
.catch(err => {
  console.log(err)
})
