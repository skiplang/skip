---
title: Simulating goto in JavaScript
author: Christopher Chedeau
---

Skip, a new programming language we’re working on, has pattern matching. In order to implement it efficiently, Tim Zakian implemented the technique described in the paper "[Compiling Pattern Matching to Good Decision Trees](http://moscova.inria.fr/~maranget/papers/ml05e-maranget.pdf)". The idea is to turn the pattern matching rules into an acyclic graph that maximizes sharing of common sub-problems.

There’s a small example in the paper: given two lists built using Cons/Nil, if the first one contains exactly 1 element, then return 1. If the second one contains exactly 1 element, then return 2. Otherwise return -1. The Skip code looks like this:

<center><img src="/blog/assets/goto-javascript.jpg" width="500" height="299" /></center>


```js
(x, y) match {
  | (Cons(_, Nil()), _             ) -> 1
  | (_             , Cons(_, Nil())) -> 2
  | (_             , _             ) -> -1
}
```

If you implement the algorithm in the paper (which is easy to read if you are interested in the details), you’re going to end up with a graph that looks like this:

At the end of the day, the output is going to be some assembly doing jumps to labels but in this case we’re interested in targeting a high level language. For languages that support goto like C or PHP, the output code would look something like this:

```js
A:  if (x instanceof Cons) {
      goto B;
    } else {
      goto C;
    }
B:  if (x.tail instanceof Nil) {
      return 1;
    } else {
      goto C;
    }
C:  if (y instanceof Cons) {
      goto D;
    } else {
      return -1;
    }
D:  if (y.tail instanceof Nil) {
      return 2;
    } else {
      return -1;
    }
```

## Goto in JavaScript

Unfortunately, we don’t have the luxury of having goto in JavaScript. But, we can get creative. There is a little known feature of JavaScript that lets you break out of nested loops:

```js
outer: while(true) {
         while(true) {
           break outer;
         }
       }
```

It turns out that you don’t even need the loop, you can put the label on a block and break out of it.

```js
A: {
  B: {
    break A;
  }
}
```

This is going to jump to the end of the A block. Given this trick, we can rewrite our initial example by nesting labelled blocks.

```js
D: {
  C: {
    B: {
      if (x instanceof Cons) {
        break B;
      } else {
        break C;
      }
    } // end of B
    if (x.tail instanceof Nil) {
      return 1;
    } else {
      break C;
    }
  } // end of C
  if (y instanceof Cons) {
    break D;
  } else {
    return -1;
  }
} // end of D
if (y.tail instanceof Nil) {
  return 2;
} else {
  return -1;
}
```

Voila! We managed to turn all our goto into something that JavaScript can run.

## Backward goto

In our case, we only needed forward jumps. If we wanted backward jumps, the idea is to use continue instead of break. Unfortunately, JavaScript won’t let you do continue on just blocks, so we need to use while loop.

```js
A: while(true) {
     if ( ... ) {
       continue A;
     }

     break;
   }
```

## Further Optimizations

In the A case, we either jump to B or C. Since B is right after A, we don’t actually need the jump in that case. Same for the jump to D, so we can remove both. It’s also possible to reorder blocks to create more opportunities to remove blocks this way, even though in this case it doesn’t help. Here’s the output after this cleanup:

```js
C: {
  if (!(x instanceof Cons)) {
    break C;
  }
  if (x.tail instanceof Nil) {
    return 1;
  }
} // end of C
if (!(y instanceof Cons)) {
  return -1;
}
if (y.tail instanceof Nil) {
  return 2;
} else {
  return -1;
}
```

This piece of code is going to do the smallest amount of operations to get the correct result but it looks weird. In this particular case, the version written by a human would be just as fast. My gut feeling is that it’s possible to transform the first one into the second one but it’s a wishlist for now :)

```js
if (x instanceof Const && x.tail instanceof Nil) {
  return 1;
} else if (y instanceof Const && y.tail instanceof Nil) {
  return 2;
} else {
  return -1;
}
```
