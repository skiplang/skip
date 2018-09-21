/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * assumes common/utils.js already loaded in wasmUtils
 */

const runtimeUrl = '../../runtime/dist/runtime.wasm'

const fetchAsArrayBuffer = (url) => {
  return fetch(url).then(response =>
    response.arrayBuffer()
  )
}

const loadWasmApp = (wasmUrl) => {
  return fetchAsArrayBuffer(runtimeUrl).then(runtimeBuf =>
    fetchAsArrayBuffer(wasmUrl).then(wasmBuf =>
      wasmUtils.load(runtimeBuf, wasmBuf)))
}
