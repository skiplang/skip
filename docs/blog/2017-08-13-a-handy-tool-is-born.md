---
title: A Handy Tool is Born
author: Mat Hostetter
---

Recently Christopher Chedeau asked me if I could write a tool like [Godbolt's Compiler Explorer](https://godbolt.org/) that would allow developers to see what machine code the Skip compiler produces for their code.

That sounded like a good weekend hack, so I threw something together that shows the assembly code for any combination of functions or files, as either human-readable text or JSON.

## A Small Example That Made Me Happy

Let's try using pattern matching to print `"B"` if passed an instance `B` and `"C"` if passed a `C`:

```
base class A
class B() extends A
class C() extends A

fun printTypeName(a: A): void {
  print_string(a match { B() -> "B" | C() -> "C" })
}
```

Here's the x86_64 code shown by the tool, which was optimized first by the Skip compiler and then by LLVM:

```
_rx.printTypeName:                      ## @rx.printTypeName
## BB#0:                                ## %b0.entry
 movq -8(%rdi), %rax
 movq (%rax), %rdi
 jmp _rx.print_string        ## TAILCALL
```

What happened here? The optimizer noticed that the `match` is simply mapping types to compile-time constants, so rather than generating a conditional branch it used a graph coloring algorithm to reserve a slot in each class's vtable and stored the constants there. Excellent!

## A Smaller Example That Made Me Sad

Now let's change the code slightly, to simply return the String rather than print it:

```
fun printTypeName(a: A): String {
  a match { B() -> "B" | C() -> "C" }
}
```

You would expect an even shorter function, but instead we get:

```
_rx.printTypeName:                      ## @rx.printTypeName
## BB#0:                                ## %b0.entry
 movq -8(%rdi), %rcx
 movabsq $-4035225266123964350, %rdx ## imm = 0xC800000000000042
 leaq 1(%rdx), %rax
 testb $1, (%rcx)
 cmoveq %rdx, %rax
 retq
```

Why is it worse? It turns out that multiple `return` statements coming from the front end trick the `match` optimizer into thinking it needs to generate control flow. So it falls back to its second choice, storing a bit in each vtable and branching on that to do the type dispatch. Fortunately LLVM stepped in and optimized that branch into a conditional move, but it's still not as tight as it could have been.

I'll admit that when I started writing this post I wasn't expecting to see the code get worse in the second example, which shows how handy a tool like this can be.

Oh, and some of you may be wondering "what the heck is that huge integer?" That's the Skip String constant `"B"`. Aaron Orenstein implemented a "short string optimization" where Strings smaller than a pointer (i.e. <= 7 UTF-8 bytes) are simply stored inline in the pointer itself. LLVM then cutely adds one to that to produce the String `"C"`.

Next up, hopefully I can convince Christopher to use the JSON API to package this into something more user-friendly.
