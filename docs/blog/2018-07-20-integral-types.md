---
title: Integral Types
author: Aditya Durvasula
---

Skip now supports UInt8, UInt16, UInt32, Int8, Int16, Int32 along with the original Int (64 bit) type.

The new integer types can be used to efficiently store a value in a specific range, while all operations always work by first converting to 64 bit Ints. Conversion back to a smaller integer type is explicit and the user can choose to truncate the value or fail if it is outside the representable range.

```
// Int literals are 64 bit
x = 20;    
y = 100;
// All operations return Int
z: Int = x + y;

// Other integer types are created with a method call:
x8 = UInt8::create(20);
y32 = Int32::create(100);
// Operations convert to Int first and return an Int:
z: Int = x8 + y32;

z8: UInt8 = UInt8::truncate(z); // ignore overflow
z8: UInt8 = UInt8::create(z);   // fail if overflow
```

## How does it work?

The original Int class is the workhorse that provides the implementations for all the useful math and comparison methods by compiling down to LLVM or JS. Each operation is specialized on the types of the arguments so that operations between two Ints incurs no additional overhead. Operations between other integer types will inline the conversion and operation together.

## What next?

Now that we have the UInt8 type we plan to add more features for working with binary data. Aaron Orenstein is already adding support for converting between String encodings and converting a String from utf8 bytes. We're also planning to add support for binary IO as well as serialization of Skip objects to/from binary formats such as Thrift.
