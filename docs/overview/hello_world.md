---
id: hello_world
title: Hello world!
---

This document is a walkthrough of the Skip programming language. It is not meant to be an exhaustive language spec, but a good place to learn the language and its core design principles.

**Note:** the reader is expected to be familiar with at least one main stream programming language, this is not a good place to learn how to program.

## Hello world!

Let's start with a classic.

```
fun main(): void {
  print_raw("Hello world!")
}
```

A couple of things can already be noticed. Functions need type annotation: we specified the type of its return type (`void`). The special function named `main` is invoked as the entry point of your program. Also, we didn't use the keyword `return` because Skip is an expression based language: there is no notion of statement. We will see how to compose expressions in sequences later.
