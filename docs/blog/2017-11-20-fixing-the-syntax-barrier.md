---
title: Fixing the Syntax Barrier
author: Christopher Chedeau
---

One of my biggest frustration when I try to learn a new language (Rust, Elm) or work on a language that I haven't touched in a while (OCaml, C++, PHP) is around syntax. I know what I want to write and I have an approximate idea of how it should be written but don't exactly get it right.

Unfortunately, when you get it wrong, you always get an obscure parser error such as “Unexpected token x” and if you get lucky, you also get “expected <some parser type>” as well. This is correct from a parser point of view but unhelpful for a person.

Elm developers spent a lot of energy on error messages but if you write “return” by mistake, they have a pretty error but the content is still the same.

<center><img src="/blog/assets/fixing-syntax-barrier.jpg" width="500" height="184" /></center>

Elm error message when you mistakenly write a return statement

The problem with error message around syntax is that it's near impossible to Google for the error you've made.

## Other programming languages exist!

When looking at the errors I was making, the majority of them was me trying to use a syntax from a different programming language. All the programming languages only know about their own language idioms and make no attempt at trying to understand the rest of the world.

This makes total sense from a software engineering perspective: you want to carve off a world where everything is consistent and don't want to have to support everything people may write. However, this is not how people work, they go back and forth many languages on a regular basis and are mixing them up all the time.

## Skip solution

The exciting part is that now that I'm working on a programming language, I actually have a shot at fixing this!

First of all, the solution is —not— to try and accept every programming language syntaxes people could throw at us and try to come up with a super language. This would be a terrible idea at so many levels.

What we want to do is —when there's an error—, try to figure out what the person meant to write and provide hints based on that. In the example above, the token just before the syntax error is return. This is trivial to detect!

If we encounter this specific instance, we can now provide helpful information such as: “our language doesn't support the return keyword and the last expression is returned, if you wanted to do an early return, here's a link to a document that explains how to translate those patterns.

## Examples

Since my first time trying the language for the first time, I took note of every error I made. We've already added detection for 40 patterns such as:

- `a ? b : c` instead of `if (a) b else c`
- `===` instead of `==`
- `=>` instead of `->` or `~>`
- Missing `fun` before method declaration
- `boolean` instead of `Bool`
- `$variable` instead of `variable`
- Extraneous `var`, `const`, `let`, `val`
- etc...

And there's a [long list](https://github.com/skiplang/skip/issues/627) of other patterns that we want to get to at some point. The good news is that it likely follows a 80-20 distribution where fixing a few common mistakes is going to cover a big amount of errors. Also it's pretty mechanical work and can be done at any time.

## Conclusion

Syntax in my experience is a big barrier to entry for using a new language, even for experienced programmers. In many cases, when a syntax error is raised, there are heuristics that are easy to implement to figure out what the person meant to write and suggest the correct way to do it.

The ultimate goal is that you are able to paste code written in any of the top 20 most used programming languages and Skip is going to give you step by steps instructions on how to morph your code to make it valid Skip.

We've implemented many suggestions and results so far are very encouraging. As I'm writing code myself, I'm now seeing a lot of those (correct) suggestions when I make mistakes. Also, the first experience reports using Skip used to contain a lot of issues around syntax and the latest one didn't.
