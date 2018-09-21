/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Small test to make some calls to runtime malloc
 * routine to diagnose unexpected out of bounds issue
 */
const skWasmUtils = require('../runtime/nodeRuntime')

const main = () => {
  skWasmUtils.loadRuntime()
    .then(rt => {
      // console.log('runtime loaded: ', rt)
      const initial_malloc_heap_ptr = skWasmUtils.getRawInt(rt.exports, 8)
      console.log('initial malloc heap ptr: ', initial_malloc_heap_ptr)

      const malloc = rt.exports.malloc
      const free = rt.exports.free
      console.log('allocating 24 bytes')
      const ptr = malloc(24)
      console.log('malloc returned: ', ptr)
      const p2 = malloc(100)
      console.log('100 bytes allocated ', p2)
      free(ptr)
      console.log('ptr freed')
      const p3 = malloc(24)
      console.log('24 bytes allocated at: ', p3)

      console.log('memory: ', rt.exports.memory)
      const malloc_heap_ptr = skWasmUtils.getRawInt(rt.exports, 8)
      console.log('malloc heap ptr: ', malloc_heap_ptr)
    })
    .catch(err => {
      console.log(err)
    })
}

main()
