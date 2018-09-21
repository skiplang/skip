---
id: tutorial
title: Tutorial
---

Welcome to the Skip programming language!

We are going to present you the language through a series of short exercises.
Follow the instructions in the comments and click "Run" whenever you are ready.
When you see a message that says "Pass", you are done. If you see an error: try again!

```runnable
fun test(): String {
  // TODO: uncomment the next line
  // "Hello Skip!"
}

// --

fun main(): void {
  assertEqual(test(), "Hello Skip!");
  print_raw("√ Pass")
}
```

The function 'debug' lets you print any value. Feel free to use it whenever you need!

```runnable
fun test(): String {
  // TODO: uncomment the next line
  // debug(123); debug("Hello"); debug(Vector[1, 2, 3]);
  "Hello Skip!"
}

// --

fun main(): void {
  assertEqual(test(), "Hello Skip!");
  print_raw("√ Pass")
}
```

Skip does not have a "return" keyword (it is an expression based language), the last value of a sequence is the one returned.
Let's try it!

```runnable
fun test(): String {
  x = "Hello";
  y = "Skip!";
  // TODO: return the value "Hello Skip!"
  // HINT: objects of type String define an infix method called '+'

}

// --

fun main(): void {
  assertEqual(test(), "Hello Skip!");
  print_raw("√ Pass")
}
```

Expression based means that 'everything is an expression'.
In that spirit if-then-else is an expression too!

```runnable
fun test(leaving: Bool): String {
  // TODO: uncomment the next line
  // x = if(leaving) "Bye" else "Hello";
  y = "Skip!";
  x + " " + y
}

// --

fun main(): void {
  assertEqual(test(true), "Bye Skip!");
  print_raw("√ Pass")
}
```

Locals can be modified, but you need to use an exclamation mark (e.g. ```!x = 1``` sets the value of ```x``` to ```1```).

```runnable
fun incrementIfTrue(x: Int, cond: Bool): Int {
  if(cond) {
    // TODO: set the value of 'x' to 'x + 1'

  };
  x
}

// --

fun main(): void {
  assertEqual(incrementIfTrue(33, true), 34);
  assertEqual(incrementIfTrue(33, false), 33);
  print_raw("√ Pass")
}
```

Skip objects are immutable by default: ```Point(0, 0)``` creates a new immutable object of type Point.

```runnable
class Point(x: Int, y: Int) {
  fun incrX(): this {
    // TODO: return a new point that moved x by 1
    // HINT: 'this.x' allows you to access the field called 'x'.

  }
}

// --

extension class Point uses Equality, Show {
  // Defining equality for a Point
  fun ==(other: this): Bool {
    other.x == this.x && other.y == this.y    
  }

  // Defining string representation of a Point
  fun toString(): String {
    `Point(${this.x.toString()}, ${this.y.toString()})`
  }
}

fun main(): void {
  assertEqual(Point(33, 0).incrX(), Point(34, 0));
  print_raw("√ Pass")
}
```

When an object starts to have too many fields, you should prefer named arguments.
```Point {x => 0, y => 0}``` creates a new Point with named arguments.

```runnable
class Point {x: Int, y: Int} {
  fun incrX(): this {
    // TODO: return a new point that moved x by 1

  }
}

// --

extension class Point uses Equality, Show {
  // Defining equality for a Point
  fun ==(other: this): Bool {
    other.x == this.x && other.y == this.y    
  }

  // Defining string representation of a Point
  fun toString(): String {
    `Point(${this.x.toString()}, ${this.y.toString()})`
  }
}

fun main(): void {
  assertEqual(Point{x => 33, y => 0}.incrX(), Point{x => 34, y => 0});
  print_raw("√ Pass")
}
```

```this with {x => 0}``` creates a copy ```this``` with the field ```x``` set to zero.
Let's try the same exercise again, but this time try to use the ```with``` construction.

```runnable
class Point {x: Int, y: Int} {
  fun incrX(): this {
    // TODO: return a new point that moved x by 1

  }
}

// --

extension class Point uses Equality, Show {
  // Defining equality for a Point
  fun ==(other: this): Bool {
    other.x == this.x && other.y == this.y    
  }

  // Defining string representation of a Point
  fun toString(): String {
    `Point(${this.x.toString()}, ${this.y.toString()})`
  }
}

fun main(): void {
  assertEqual(Point{x => 33, y => 0}.incrX(), Point{x => 34, y => 0});
  print_raw("√ Pass")
}
```

Using ```with``` is fine, but we can do even better!
A very common pattern when manipulating immutable object is to define the same local with a new version of the object: ```!this = this with {field => value}```.
It works, but it's a bit verbose, to remedy that Skip lets you write: ```!this.field = value```.
Let's try to write the same exercise one last time, but this time by using an '!'.

```runnable
class Point {x: Int, y: Int} {
  fun incrX(): this {
    // TODO: return a new point that moved x by 1

  }
}

// --

extension class Point uses Equality, Show {
  // Defining equality for a Point
  fun ==(other: this): Bool {
    other.x == this.x && other.y == this.y    
  }

  // Defining string representation of a Point
  fun toString(): String {
    `Point(${this.x.toString()}, ${this.y.toString()})`
  }
}

fun main(): void {
  assertEqual(Point{x => 33, y => 0}.incrX(), Point{x => 34, y => 0});
  print_raw("√ Pass")
}

```

Now, let's write the mutable version. Notice the keywords 'mutable' placed in front of the field and the method.
```this.!field = value``` modifies object in place, much like in any other programming language.

```runnable
mutable class Point {mutable x: Int, y: Int} {
  mutable fun incrX(): void {
    // TODO: increment the field 'x' by 1
    // HINT: 'this.!x = value' sets the field 'x' to 'value'

  }
}

// --

extension class Point uses Equality, Show {
  // Defining equality for a Point
  fun ==(other: this): Bool {
    other.x == this.x && other.y == this.y    
  }

  // Defining string representation of a Point
  fun toString(): String {
    `Point(${this.x.toString()}, ${this.y.toString()})`
  }
}

fun main(): void {
  point = mutable Point{x => 33, y => 0};
  point.incrX();
  assertEqual(freeze(point), Point{x => 34, y => 0});
  print_raw("√ Pass")
}

```

Mutable objects must be explicitly created with the 'mutable' keyword.
For example, ```mutable Point{...}``` creates a new mutable Point.
Both mutable and immutable methods are accessible until the object is frozen.
The freeze keyword turns any mutable object into an immutable copy
(note that the mutable methods won't be available anymore after freezing).


```runnable

fun test(): Point {
  point = mutable Point{x=>1, y=>2};
  // TODO: call the mutable method incrX

  freeze(point)
}

// --

mutable class Point {mutable x: Int, y: Int} {
  mutable fun incrX(): void {
    this.!x = this.x + 1    
  }
}

fun main(): void {
  assertEqual(test().x, 2);
  print_raw("√ Pass")
}
```

Creating a mutable object and then freezing it is an encouraged pattern in Skip, especially when dealing with collections.
The two most common type of collections are ```Vector``` and ```Map```.
A ```Vector``` is an array of values that is contiguous in memory and that can grow in its mutable form.
A ```Map``` is a hashtable that can also grow in its mutable form.

```runnable
fun incrValues(v: Map<String, Int>): Map<String, Int> {
  result = mutable Map[];
  v.each((key, value) -> {
    // TODO: complete the code
    // HINT: 'map![key] = value' is a useful pattern!

  });
  freeze(result)
}

// --

fun main(): void {
  assertEqual(incrValues(Map["foo" => 1])["foo"], 2);
  print_raw("√ Pass")
}
```

Now let's get to the most interesting part: memoization!
Memoization consists in remembering the results produced by a function.
But lets try it with an example: run the code, then uncomment the keyword 'memoized' and run it again.
You will see that the 'move' function is only called once on the second run.

```runnable

/* memoized */ fun move(p: Point): Point {
  debug("Moved: " + p.toString());
  !p.x = p.x + 1;
  p
}

fun test(): Bool {
  zero = Point(0, 0);
  point1 = move(zero);
  point2 = move(zero);
  point1 == point2
}

// --

class Point (x: Int, y: Int) {
  fun ==(point: Point): Bool {
    this.x == point.x && this.y == point.y
  }
  fun toString(): String {
    "(" + this.x.toString() + ", " + this.y.toString() + ")"
  }
}

fun main(): void {
  _ = test();
  print_raw("√ Pass")
}
```

Skip keeps track of external dependencies.
Try modifying the coordinates of the points and watch the system update.

TODO: Demo

Let's finish with a more advanced example, so that you have a chance to write some Skip code.
Write a program that computes the furthest point from (0, 0).
But write it in such a way that updates take O(log(n)).

TODO: Demo
