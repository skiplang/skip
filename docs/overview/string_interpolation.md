---
id: string_interpolation
title: String Interpolation
---

String literals enclosed with back-ticks``(`)`` may include embedded expressions
beginning with `$` and wrapped in `{}`'s:

```
fun greet(name: String): void {
  print(`Hello ${name}, welcome to Skip!`)
}
```
