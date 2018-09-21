/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

'use strict';


// Based on:
// https://gist.github.com/kripken/59c67556dc03bb6d57052fedef1e61ab
//   and
// http://thecodebarbarian.com/getting-started-with-webassembly-in-node.js.html

// Loads a WebAssembly dynamic library, returns a promise.
// imports is an optional imports object
function loadWebAssembly(buffer, imports) {
  // Create the imports for the module, including the
  // standard dynamic library imports
  imports = imports || {}
  imports.env = imports.env || {}
  imports.env.memoryBase = imports.env.memoryBase || 0
  imports.env.tableBase = imports.env.tableBase || 0
/*
  if (!imports.env.memory) {
    console.log('loadWebAssembly: creating new Memory for module')
    imports.env.memory = new WebAssembly.Memory({ initial: 256 })
  }
  if (!imports.env.table) {
    console.log('loadWebAssembly: creating new Table for module')
    imports.env.table = new WebAssembly.Table({ initial: 0, element: 'anyfunc' })
  }
*/
  // Create the instance.
  return WebAssembly.instantiate(buffer, imports).then(res =>
    res.instance
  )
  /*
  return WebAssembly.compile(buffer)
    .then(module => {
      return new WebAssembly.Instance(module, imports)
    })
  */
}

/** Writes a 32-bit integer value to a buffer at the specified offset. */
function writeInt(buffer, offset, value) {
  buffer[offset    ] =  value         & 0xff;
  buffer[offset + 1] = (value >>>  8) & 0xff;
  buffer[offset + 2] = (value >>> 16) & 0xff;
  buffer[offset + 3] =  value >>> 24;
  return 4;
}

function readInt(buffer, offset) {
  let val = 0
  val = (buffer[offset + 3] << 24) | (buffer[offset + 2] << 16) | (buffer[offset + 1] << 8) | buffer[offset]
  return val
}

const DEBUG_heap = false

// Start of heap, to allow plenty of space for constants in
// data sections of our compiled module:
const HEAP_START = 32 * 1024;

const HEAP_PTR = 8;

function loadRuntime(filebuf) {
  return loadWebAssembly(filebuf)
    .then(instance => {
      const memBuffer = instance.exports.memory.buffer
      const bytes = new Uint8Array(memBuffer, 0, 64)

      // Store the offset of HEAP_START before calling init:
      writeInt(bytes, HEAP_PTR, HEAP_START)

      // Read back the ptr:
      if (DEBUG_heap) {
        const heap_ptr = readInt(bytes, HEAP_PTR)
        console.log('loadRuntime: heap ptr:', heap_ptr)
      }
      const init = instance.exports.init
      init()
      if (DEBUG_heap) {
        console.log('heap allocator initialized.')
      }
      return instance
    })
}

const hexStr = (v) => '0x' + (v >>> 0).toString(16)

// from Immutable.js:
// v8 has an optimization for storing 31-bit signed numbers.
// Values which have either 00 or 11 as the high order bits qualify.
// This function drops the highest order bit in a signed number, maintaining
// the sign bit.
function smi(i32) {
  return i32 >>> 1 & 0x40000000 | i32 & 0xbfffffff;
}

// Copied from Immutable.js; updated with an alternative constant from Mat:
function hashString(string) {
  // This is the hash from JVM
  // The hash code for a string is computed as
  // s[0] * 31 ^ (n - 1) + s[1] * 31 ^ (n - 2) + ... + s[n - 1],
  // where s[i] is the ith character of the string and n is the length of
  // the string. We "mod" the result to make it between 0 (inclusive) and 2^31
  // (exclusive) by dropping high bits.
  let hash = 0;
  const multiplier = 31; // 0x9e3779b9 from mjhosteter; Immutable used 31
  for (let ii = 0; ii < string.length; ii++) {
    hash = multiplier * hash + string.charCodeAt(ii) | 0;
  }
  return smi(hash);
}



const mkTrap = (nm) => {
  const tf = (...args) => {
    console.error("Error: call to unsupported intrinsic func: ", nm, ": ", args)
    throw new Error('Unsupported function ' + nm)
  }
  return tf
}

const acos = (v) => {
  return Math.acos(v)
}


const alignment_size = 8

const roundUp = (nbytes, alignment) => {
  return (Math.ceil(nbytes / alignment) * alignment)
}

// assumes ASCI!!
const writeStrBytes = (buffer, offset, str) => {
  for (let i = 0; i < str.length; i++) {
    const ch = str.charCodeAt(i)
    buffer[offset + i] = ch
  }
  buffer[offset + str.length] = '\0'
}

// Allocate a Skip string, assuming all ASCII:
const allocSkipString = (env, str) => {
  // Allocate an extra byte for NUL termination
  // plus 4 bytes of length
  // plus 4 bytes of hash
  const nbytes = roundUp(str.length + 9, alignment_size)
  const malloc = env.malloc

  const ptr = malloc(nbytes)
  const memBuffer = env.memory.buffer
  const bytes = new Uint8Array(memBuffer, ptr, nbytes)

  writeInt(bytes, 0, str.length)
  writeInt(bytes, 4, hashString(str))
  writeStrBytes(bytes, 8, str)

  // return start of raw char data (Skip convention):
  const ret = ptr + 8
  if (DEBUG_heap) {
    console.log('allocSkipString: returning: ', ret)
  }
  return ret
}

// decode a Skip string into a JavaScript string
const decodeSkipString = (env, char_data_ptr) => {
  // base ptr is at char_data_ptr - 8:
  const ptr = char_data_ptr - 8
  // console.log('decodeSkipString: decoding bytes at offset ', ptr)
  const memBuffer = env.memory.buffer
  // console.log('bytes at ptr: ', new Uint8Array(memBuffer, ptr, 32))
  const lenBytes = new Uint8Array(memBuffer, ptr, 4)
  const len = readInt(lenBytes, 0)
  // console.log('decodeSkipString: len: ', len)
  const strBytes = new Uint8Array(memBuffer, ptr + 8, len)
  let strs = []
  for (let i = 0; i < len; i++) {
    strs.push(String.fromCharCode(strBytes[i]))
  }
  const s = strs.join('')
  // console.log('decodeSkipString: "' + s + '"')
  return s
}

const getRawData = (env, ptr, len) => {
  const memBuffer = env.memory.buffer
  return new Uint8Array(memBuffer, ptr, len)
}

const getRawInt = (env, ptr) => {
  const memBuffer = env.memory.buffer
  const data = new Uint32Array(memBuffer, ptr, 1)
  return data[0]
}

const SKIP_true = 1
const SKIP_false = 0

const SKIP_String_eq = (env, s1_data_ptr, s2_data_ptr) => {
  if (s1_data_ptr === s2_data_ptr) {
    return SKIP_true
  }
  const s1 = s1_data_ptr - 8
  const s2 = s2_data_ptr - 8

  const memBuffer = env.memory.buffer
  const s1Bytes = new Uint8Array(memBuffer, s1, 8)
  const s2Bytes = new Uint8Array(memBuffer, s2, 8)

  // do 4 bytes of hash match?
  for (let i = 4; i < 8; i++) {
    if (s1Bytes[i] != s2Bytes[i]) {
      return SKIP_false
    }
  }
  return SKIP_true
}

const SKIP_Num_to_string = env => val => {
  const valStr = val.toString()
  const ptr = allocSkipString(env, valStr)
  // console.log('SKIP_Num_to_string called: ', val, valStr, ptr)
  return ptr
}

const SKIP_print_raw = env => (...args) => {
  const strs = args.map(ptr => decodeSkipString(env,ptr))
  console.log(...strs)
}

const DEBUG_string_concat = false
const SKIP_String_concat2 = env => (sp1, sp2) => {
  if (DEBUG_string_concat) {
    console.log('SKIP_String_concat2: enter: ', sp1, sp2)
  }
  const s1 = decodeSkipString(env, sp1)
  const s2 = decodeSkipString(env, sp2)
  const rs = s1 + s2
  const ret = allocSkipString(env, rs)
  if (DEBUG_string_concat) {
    console.log('SKIP_String_concat2: "' + s1 + '" "' + s2 + '" ==> "' + rs + '" (' + ret  + ')')
  }
  return ret
}

const SKIP_String_concat3= env => (sp1, sp2, sp3) => {
  const s1 = decodeSkipString(env, sp1)
  const s2 = decodeSkipString(env, sp2)
  const s3 = decodeSkipString(env, sp3)
  const ret = allocSkipString(env, s1 + s2 + s3)
  return ret
}

const DEBUG_obStack = false

// ?? Hmmm, do we have to actually do anything here, like
// copy the object?
const SKIP_Obstack_freeze = env => ptr => {
  if (DEBUG_obStack) {
    console.log('SKIP_Obstack_freeze(' + ptr.toString() + ')')
  }
  return ptr
}

const SKIP_intern = env => ptr => {
  // console.log('SKIP_intern(' + ptr.toString() + ')')
  return ptr
}


/*
 * AC, 9/23/17: These profile functions are hacks derived from js runtime in jslib/lib/system.js
 * TODO: figure out how to just use the same runtime fns directly!
 */
const jsNow = () => {
  const time = process.hrtime(); // [seconds, nanoseconds]
  return Math.round((time[0] * 1000) + (time[1] / 1000000));
}

let profileTotal = 0;
let profile_paused = false;
let profile_started = null;
function SKIP_profile_start() {
  if (profile_started !== null) {
    throw new Error('Profile already started.');
  }
  profile_paused = false;
  profile_started = jsNow();
  profileTotal = 0;
  console.log('SKIP_profile_start ' + profile_started.toString());
}

function SKIP_profile_stop() {
  const time = jsNow();
  console.log('SKIP_profile_stop ' + time.toString());
  if (profile_started === null || profile_paused) {
    throw new Error('Profile not started or still paused.');
  }
  const total = profileTotal + (time - profile_started);
  profile_paused = false;
  profile_started = null;
  profileTotal = 0;
  return total;
}

const SKIP_Obstack_note_inl = env => () => {
  const ptr = env.calloc(4, 1)
  // console.log('SKIP_Obstack_note_inl() ==> ', ptr)
  return ptr
}

const SKIP_Obstack_alloc32 = env => sz => {
  const ptr = env.malloc(sz)
  if (DEBUG_heap) {
    console.log('SKIP_Obstack_alloc32(' + sz + ') ===> ', ptr)
  }
  return ptr
}

const SKIP_Obstack_calloc32 = env => sz => {
  const ptr = env.calloc(sz, 1)
  if (DEBUG_heap) {
    console.log('SKIP_Obstack_calloc32(' + sz + ') ===> ', ptr)
  }
  return ptr
}

const SKIP_Obstack_clear = env => ptr => {
  if (DEBUG_heap) {
    console.log('SKIP_Obstack_clear(' + ptr + ')')
  }
}

const SKIP_round = env => v => Math.round(v)

const SKIP_abort = env => () => {
  console.log('SKIP_abort()')
  throw new Error('user level abort')
}

// Add the required Skip intrinsics:
const setupBuiltins = (env) => {
  env['abort'] = SKIP_abort(env)
  env['SKIP_Math_acos'] = acos
  env['SKIP_Obstack_alloc'] = SKIP_Obstack_alloc32(env)
  env['SKIP_Obstack_calloc'] = SKIP_Obstack_calloc32(env)
  env['SKIP_HhvmStringRet_create'] = mkTrap('SKIP_HhvmStringRet_create')
  env['SKIP_String_concat2'] = SKIP_String_concat2(env)
  env['SKIP_throw'] = mkTrap('SKIP_throw')
  env['SKIP_print_raw'] = SKIP_print_raw(env)
  env['SKIP_Float_toString'] = SKIP_Num_to_string(env)
  env['SKIP_Int_toString32'] = SKIP_Num_to_string(env)
  env['SKIP_String_concat3'] = SKIP_String_concat3(env)
  env['SKIP_Obstack_clear'] = SKIP_Obstack_clear
  env['SKIP_Obstack_freeze'] = SKIP_Obstack_freeze(env)
  env['SKIP_Obstack_note_inl'] = SKIP_Obstack_note_inl(env)
  env['SKIP_HhvmStringRet_create'] = mkTrap('SKIP_HhvmStringRet_create')
  env['SKIP_String_concat'] = mkTrap('SKIP_String_concat')
  env['SKIP_String_concat4'] = mkTrap('SKIP_String_concat4')
  env['SKIP_String_eq'] = (s1,s2) => SKIP_String_eq(env,s1,s2)
  env['SKIP_profile_start'] = SKIP_profile_start
  env['SKIP_profile_stop'] = SKIP_profile_stop
  env['SKIP_intern'] = SKIP_intern(env)
  env['_ZTIN6skip15SkipExceptionE'] = 99
  env['SKIP_debug_break'] = mkTrap('SKIP_debug_break')
  env['round'] = SKIP_round(env)
}

const DEBUG_init = false

const loadWasm = (rt,wasmFileBuf) => {
  // Use exports from the runtime instance to craft the imports for the app:
  const imports = {
    env: {}
  }
  Object.assign(imports.env, rt.exports)
  setupBuiltins(imports.env)
  const CL_SIZE = 12
  return loadWebAssembly(wasmFileBuf, imports)
      .then(instance => {
        const exports = instance.exports

        const skInit = exports['SKIP_initializeSkip']
        const iret = skInit()
        if (DEBUG_init) {
          console.log('Skip initialized ==> ', iret)
        }

        const env = imports.env
        return {env, instance}
      })
}

const load = (runtimeFileBuf, wasmFileBuf) => {
  return loadRuntime(runtimeFileBuf)
    .then(rt => loadWasm(rt,wasmFileBuf))
}

module.exports = { load, loadRuntime, hashString, hexStr, decodeSkipString, getRawData, getRawInt }
