/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */


const utils = require('./skWasmUtils')
const { hashString, hexStr } = utils


const showHash = (str) => {
  const h = hashString(str)

  console.log('hash("' + str + '") ==> ' + h.toString() + ' ' + hexStr(h))
}

const printBytes = (str) => {
  let codes = []
  for (let i=0; i < str.length; i++) {
    const ch = str.charCodeAt(i)
    codes.push(ch.toString())
  }
  console.log('byte codes: ', codes.join(', '))
}


showHash('a')
showHash('ab')
showHash('abc')
showHash('abcd')
showHash('abcde')
showHash('abcdef')
showHash('abcdefg')

m = '[LOG] This is a a not-too-long log message, once that will commonly pop up in my application'
showHash(m)
printBytes(m)
