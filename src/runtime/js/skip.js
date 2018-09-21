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

const functionDecorators = require('./function_decorators');
const runtime = require('./runtime');

// Prelude native classes definitions
const nativeClasses = {
  Array: require('./lib/array'),
  Bool: require('./lib/bool'),
  Char: require('./lib/char'),
  Class: require('./lib/class'),
  Float: require('./lib/float'),
  Int: require('./lib/int'),
  Int8: require('./lib/int8'),
  Int16: require('./lib/int16'),
  Int32: require('./lib/int32'),
  String: require('./lib/string'),
  Regex: require('./lib/regex'),
  'Unsafe.RawStorage': require('./lib/unsafe_rawstorage'),
  Void: require('./lib/void')
};

// Prelude native global functions
const nativeGlobalModules = {
  Debug: require('./lib/debug'),
  Unsafe: require('./lib/unsafe'),
  Math: require('./lib/math'),
  System: require('./lib/system'),
};

function hasSkipProperty(object, propertyName) {
  const descriptor = Object.getOwnPropertyDescriptor(object, propertyName);
  // ... but getOwnPropertyNames also returns builtins: length, name, prototype, constructor
  // Note that if these are defined manually, they will be overridden
  return (descriptor != null) &&
    // Also include our __classname property used for debugging
    // and __type_switch_id for typeswitch codegen.
    (propertyName === '__classname' || propertyName === '__type_switch_id' ||
      typeof descriptor.value === 'function' || typeof descriptor.get === 'function');
}

function *getSkipMemberNames(object) {
  // 'class' members are not enumerable, so must use getOwnPropertyNames
  for (const key of Object.getOwnPropertyNames(object))  {
    // ... but getOwnPropertyNames also returns builtins: length, name, prototype, constructor
    // Note that if these are defined manually, they will be overridden
    if (hasSkipProperty(object, key)) {
      yield key;
    }
  }
}

// keep this in sync with ensure_identifier_chars in skipJsIstUtils.sk
function mangleIdentifierChar(ch) {
  switch (ch) {
  case '!': return "$bg";
  case '=': return "$eq";
  case '>': return "$gt";
  case '<': return "$lt";
  case '@': return "$at";
  case '.': return "$dt";
  case '%': return "$pc";
  case '+': return "$pl";
  case '-': return "$mi";
  case '*': return "$ti";
  case '/': return "$dv";
  case '&': return "$am";
  case '|': return "$br";
  default: return ch;
  }
}

const keywordList = [  // keywords
  "break",
  "case",
  "catch",
  "class",
  "const",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "else",
  "export",
  "extends",
  "finally",
  "for",
  "function",
  "if",
  "import",
  "in",
  "instanceof",
  "new",
  "return",
  "super",
  "switch",
  "this",
  "throw",
  "try",
  "typeof",
  "var",
  "void",
  "while",
  "with",
  "yield",
  // future reserved workds
  "await",
  "enum",
  // strict reserved words
  "use",
  "interface",
  "package",
  "private",
  "protected",
  "public",
  // other troublesome JS identifiers
  "arguments",
  "undefined",
];
const keywordSet = keywordList.reduce(function (set, kw) { set[kw] = true; return set; }, Object.create(null));

function manglePropertyName(p) {
  const mangledName = p.replace(/./g, mangleIdentifierChar);
  if (keywordSet[mangledName]) {
    return '$' + mangledName;
  } else {
    return mangledName;
  }
}

function copySkipMember(source, dest, propertyName) {
  if (!hasSkipProperty(source, propertyName)) {
    throw new Error('Failure to copy JS property \'' + propertyName + '\'.');
  }
  const descriptor = Object.getOwnPropertyDescriptor(source, propertyName);
  Object.defineProperty(dest, propertyName, descriptor);
}

function copySkipMembers(source, dest) {
  for (const key of getSkipMemberNames(source))  {
    // Use call to support prototypes not derived from Object.
    if (!Object.prototype.hasOwnProperty.call(dest, key)) {
      copySkipMember(source, dest, key);
    }
  }
}

function mixinMembers(derivedObject, baseObject) {
  for (const baseMemberName of getSkipMemberNames(baseObject)) {
    if (!hasSkipProperty(derivedObject, baseMemberName)) {
      copySkipMember(baseObject, derivedObject, baseMemberName);
    }
  }
}

function mixinBaseMembers(sk, derived, bases) {
  const derivedPrototype = derived.prototype;

  // JS Class objects are skip objects deriving from the skip 'Class' class.
  // Copy in instance members of Class to
  if (derived !== sk['Class']) {
    mixinMembers(derived, sk['Class'].prototype)
  }

  // Make class objects look like instances of the Class class.
  derived.__classname = 'Class';

  // Static Members from bases
  for (const base of bases) {
    mixinMembers(derived, base);
  }
  // Instance members from bases
  for (const base of bases) {
    mixinMembers(derivedPrototype, base.prototype);
  }
}

function initNativeClass(sk, name, originalCtor) {
  const nativeDefinition = nativeClasses[name];
  if (!nativeDefinition) {
    runtime.warn(`Skip: No definition found for native class ${name}.`);
    return originalCtor;
  }

  var {ctor, instanceMembers, staticMembers} = nativeDefinition(sk);

  // statics members
  copySkipMembers(originalCtor, ctor);
  for (const key of Object.getOwnPropertyNames(staticMembers))  {
    ctor[manglePropertyName(key)] = staticMembers[key];
  }

  // instance members
  copySkipMembers(originalCtor.prototype, ctor.prototype);
  for (const key of Object.getOwnPropertyNames(instanceMembers))  {
    ctor.prototype[manglePropertyName(key)] = instanceMembers[key];
  }

  // Replace the base list with the new ctor.
  ctor.prototype.__bases =
    originalCtor.prototype.__bases.map(base => base === originalCtor ? ctor : base);
  ctor.prototype.__constructor = ctor;

  return ctor;
}

function defineInstanceMethod(ctor, name, value) {
  ctor.prototype[manglePropertyName(name)] = value;
}

function defineStaticMethod(ctor, name, value) {
  ctor[manglePropertyName(name)] = value;
}

function defineGlobalFunction(sk, name, value) {
  sk[manglePropertyName(name)] = value;
}

function throwException(sk, exn) {
  // Capture stack
  exn.__stack = (sk.__.debug) ? (new Error()).stack : "";
  // Uncomment the below line can be handy to get messages when Exceptions are constructed
  // console.error(exn.__stack);
  throw exn;
}

function init() {
  const sk = {
    // for global helper functions like boolToBool
    __: {
      deepFreeze: runtime.deepFreeze,
      isInstance: runtime.isInstance,
      manglePropertyName,
      defineInstanceMethod,
      defineStaticMethod,
      defineGlobalFunction: function (name, value) { defineGlobalFunction(sk, name, value) },
    },
  };

  // Skip Generators are codegened as:
  //
  // function(args) {
  //   return sk.__.toSkipGenerator.call(this, function*() { ... yield e; ... return; });
  // }
  function toSkipGenerator(generatorFunction) {
    // Ensure the 'this' binding is set correctly.
    const generator = generatorFunction.call(this);
    const skipGenerator = Object.create(sk.Iterator.prototype);
    skipGenerator.next = function() {
      const value = generator.next();
      return value.done ? sk.__.singleton$None : value.value;
    };
    return skipGenerator;
  }
  sk.__.toSkipGenerator = toSkipGenerator;

  // Make sure sk.Exception is defined, as subclasses are generated to use:
  //   sk.SubException.prototype = Object.create(sk.Exception.prototype)
  const Exception = function () {};
  Exception.prototype.__bases = [Exception];
  sk.Exception = initNativeClass(sk, 'Exception', Exception);

  return sk;
}

function copyNativeFunctions(sk) {
  for (const key of Object.getOwnPropertyNames(nativeGlobalModules))  {
    nativeGlobalModules[key](sk);
  }
}

module.exports = Object.assign(
  // Used directly by generated code
  {
    init,
    initNativeClass,
    mixinBaseMembers,
    copyNativeFunctions,
    throwException,
  },
  functionDecorators,
  // For legacy compatibility
  runtime
);
