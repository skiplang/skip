# Lexical Structure

## Source Unit

A ***source unit*** is an ordered sequence of characters. Typically, a source unit has a one-to-one correspondence with a file in a file system, but this correspondence is not required. (See [program structure](#sec-Program-Structure).)

Conceptually, a source unit is translated using the following steps:

  1.  Transformation, which converts a source unit from a particular character repertoire and encoding scheme into a sequence of characters.
  2.  Lexical analysis, which translates a stream of input characters into a stream of tokens.
  3.  Syntactic analysis, which translates the stream of tokens into executable code.

Conforming implementations must accept source unit encoded with the UTF-8 encoding form (as defined by the Unicode standard), and transform them into a sequence of characters. Implementations can choose to accept and transform additional character encoding schemes.

## Grammars

This specification shows the syntax of the Skip programming language using two grammars. The ***lexical grammar*** defines how source characters are combined to form white space, comments, and tokens. The ***syntactic grammar*** defines how the resulting tokens are combined to form Skip programs.

The grammars are presented using ***grammar productions***, with each one defining a non-terminal symbol and the possible expansions of that non-terminal symbol into sequences of non-terminal or terminal symbols. In productions, non-terminal symbols are shown in slanted type *like this*, and terminal symbols are shown in a fixed-width font `like this`.

The first line of a grammar production is the name of the non-terminal symbol being defined, followed by one colon for a syntactic grammar production, and two colons for a lexical grammar production. Each successive indented line contains a possible expansion of the non-terminal given as a sequence of non-terminal or terminal symbols.

For example, the production:

<pre>
  <i>boolean-literal::</i>
    true
    false
</pre>

defines the lexical grammar production *boolean-literal* as being the terminals `true` or `false`. Each expansion is listed on a separate line.

Although alternatives are usually listed on separate lines, when there is a large number, the shorthand phrase “one of” may precede a list of expansions given on a single line. For example,

<pre>
  <i>hexadecimal-digit:: one of</i>
    0   1   2   3   4   5   6   7   8   9   a
    b   c   d   e   f   A   B   C   D   E   F
</pre>

## Lexical Analysis

### General

The production *input-file* is the root of the lexical structure for a source unit. Each source unit must conform to this production.

**Syntax**

<pre>
  <i>input-file::</i>
    <i>input-element</i>
    <i>input-file   input-element</i>

  <i>input-element::</i>
    <i>comment</i>
    <i>white-space</i>
    <i>token</i>
</pre>

**Defined elsewhere**

* [*comment*](#sec-Comments)
* [*token*](#sec-Tokens.General)
* [*white-space*](#sec-White-Space)

**Semantics:**

The basic elements of an *input-file* are comments, white space, and tokens.

The lexical processing of an *input-file* involves the reduction of that source unit into a sequence of [tokens](#sec-Tokens) that becomes the input to the syntactic analysis. Tokens can be separated by [white space](#sec-White-Space) and [delimited comments](#sec-Comments).

Lexical processing always results in the creation of the longest possible lexical element.

### Comments

Two forms of comments are supported: ***delimited comments*** and ***single-line comments***.

**Syntax**

<pre>
  <i>comment::</i>
    <i>single-line-comment</i>
    <i>delimited-comment</i>

  <i>single-line-comment::</i>
    //   <i>input-characters<sub>opt</sub></i>

  <i>input-characters::</i>
    <i>input-character</i>
    <i>input-characters   input-character</i>

  <i>input-character::</i>
    <i>Any source character except new-line</i>

  <i>new-line::</i>
    Carriage-return character (U+000D)
    Line-feed character (U+000A)
    Carriage-return character (U+000D) followed by line-feed character (U+000A)

  <i>delimited-comment::</i>
    /*   <i>No characters or any source character sequence except /*</i>   */
</pre>

**Semantics**

Except within a string-literal or a comment, the characters `/*` start a delimited comment, which ends with the characters `*/`. Except within a string literal or a comment, the characters `//` start a single-line comment, which ends with a new line. That new line is not part of the comment.

A delimited comment can occur in any place in a source unit in which [white space](#sec-White-Space) can occur. For example;

```
/*…*/b/*…*/=/*…*/200/*…*/;/*…*/
```

is parsed as

```
b=200;
```

### White Space

***White space*** consists of an arbitrary combination of one or more ***white-space characters***, as described below.

**Syntax**

<pre>
  <i>white-space::</i>
    <i>white-space-character</i>
    <i>white-space   white-space-character</i>

  <i>white-space-character::</i>
    <i>new-line</i>
    Form-feed character (U+000C)
    Horizontal-tab character (U+0009)
    Space character (U+0020)
</pre>

**Defined elsewhere**

* [*new-line*](#sec-Comments)

**Semantics**

The space and horizontal tab characters are considered ***horizontal white-space characters***.

### Tokens

#### General

There are several kinds of source tokens:

**Syntax**

<pre>
  <i>token::</i>
    <i>identifier</i>
    <i>keyword</i>
    <i>literal</i>
    <i>operator-or-punctuator</i>
</pre>

**Defined elsewhere**

* [*identifier*](#sec-Identifiers)
* [*keyword*](#sec-Keywords)
* [*literal*](#sec-Literals.General)
* [*operator-or-punctuator*](#sec-Operators-and-Punctuators)

#### Identifiers

**Syntax**

<pre>
  <i>identifier::</i>
    <i>type-identifier</i>
    <i>nontype-identifier</i>

  <i>type-identifier::</i>
    <i>uppercase-letter</i>
    <i>type-identifier   nondigit</i>
    <i>type-identifier   digit</i>

  <i>nontype-identifier::</i>
    <i>lowercase-letter-or-underscore</i>
    <i>nontype-identifier   nondigit</i>
    <i>nontype-identifier   digit</i>

  <i>uppercase-letter::</i> one of
    A   B   C   D   E   F   G   H   I   J   K   L   M
    N   O   P   Q   R   S   T   U   V   W   X   Y   Z

  <i>lowercase-letter-or-underscore::</i> one of
    _
    a   b   c   d   e   f   g   h   i   j   k   l   m
    n   o   p   q   r   s   t   u   v   w   x   y   z

  <i>nondigit::</i> one of
    _
    a   b   c   d   e   f   g   h   i   j   k   l   m
    n   o   p   q   r   s   t   u   v   w   x   y   z
    A   B   C   D   E   F   G   H   I   J   K   L   M
    N   O   P   Q   R   S   T   U   V   W   X   Y   Z

  <i>nontype-identifier-list::</i>
    <i>nontype-id-list</i>   ,<sub>opt</sub>

  <i>nontype-id-list::</i>
    <i>nontype-identifier</i>
    <i>nontype-id-list</i>   ,   <i>nontype-identifier</i>
</pre>

**Defined elsewhere**

* [*digit*](#sec-Integer-Literals)

**Semantics:**

*type-identifier*s other than `_` are used to name the following: [classes](#sec-Class-Declarations), [type constants](#sec-Types.Type-Constants), [type parameters](#sec-Type-Parameters), and [modules](#sec-Modules). [Note that a constant name can also be *type-identifier*, to support [constant patterns](#sec-Constant-Patterns).]

*nontype-identifier*s are used to name the following: [constants](#sec-Constants-and-Variables.General), [variables](#sec-Constants-and-Variables.General), [functions](#sec-Function-Declarations), and [methods](#sec-Methods).

*identifier*s are case-sensitive, and every character is significant.

In certain contexts, some identifiers behave as keywords. See [conditional keywords](#sec-Keywords).

The *nontype-identifier* `_` is reserved for use by the language; it is used in the following contexts:

* a [wildcard-pattern](#sec-Wildcard-Pattern)
* a [bind mutation target](#sec-Simple-Mutation)
* name of a [lambda parameter](#sec-Lambda-Creation)
* name of a [function parameter](#sec-Function-Declarations)
* name of a [constructor parameter](#sec-Constructors)

If more than one declaration of a particular identifier is visible at any point in a source unit, the syntactic context disambiguates uses that refer to different entities. Thus, there are separate ***name spaces*** for various categories of identifiers, as follows:

* Module names (which are *type-identifiers*) are separate from all other names

* In any given module, global constants and functions (which are *nontype-identifiers*) share the same namespace

* In any given module, class names and global type constant names (which are *type-identifiers*) share the same namespace

* Each class and trait has its own namespace, inside which class constants and methods (which are *nontype-identifiers*) share that same namespace

**Examples**

```
const myNaN: Float = 0.0/0.0;
fun f2c(parm: Int): void { }
class C { parm: String } {
  const _C3: Int = 30;
  fun f5(): void { void }
}
type CAlias = C;
_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ = true;
```

#### Keywords

A ***keyword*** is an identifier-like sequence of characters that is reserved, and cannot be used as an identifier. A ***conditional keyword*** is an identifier that in certain contexts, behaves like a keyword; otherwise, it is available for use as an identifier.

**Syntax**

<pre>
  <i>keyword::</i> one of
    alias  as  async  await  catch  children  class  const  else  extends
    final  from  fun  if  match  module  mutable  native  private
    protected  uses static  this  throw  trait  try  type  void 
    watch  when  with
</pre>

The following are conditional keywords: [`base`](#sec-Class-Declarations), [`capture`](#sec-Methods), [`default`](#sec-Classes.Type-Constants), [`deferred`](#sec-Methods), [`inst`](#sec-The-inst-Type), [`nonNullable`](#sec-Type-Parameters), [`this`](#sec-Primary-Expressions.General), [`untracked`](#sec-Methods), and [`value`](#sec-Class-Declarations).

#### Literals

##### General

The source code representation of a value is called a *literal*.

**Syntax**

<pre>
  <i>literal::</i>
    <i>boolean-literal</i>
    <i>character-literal</i>
    <i>integer-literal</i>
    <i>floating-literal</i>
    <i>string-literal</i>
    <i>void-literal</i>
</pre>

**Defined elsewhere**

* [*boolean-literal*](#sec-Boolean-Literals)
* [*character-literal*](#sec-Character-Literals)
* [*floating-literal*](#sec-Floating-Point-Literals)
* [*integer-literal*](#sec-Integer-Literals)
* [*string-literal*](#sec-String-Literals)
* [*void-literal*](#sec-Void-Literal)

##### Boolean Literals

**Syntax**

<pre>
  <i>boolean-literal::</i>
    true
    false
</pre>

**Semantics**

The type of a *boolean-literal* is `Bool`.

The values `true` and `false` represent the Boolean values True and False, respectively.

**Examples**

```
const debugModeOn: Bool = false;
class C {private field: Bool = true} {
  fun getField(): Bool {
    this.field
  }
}
```

##### Character Literals

**Syntax**

<pre>
  <i>character-literal::</i>
    '   <i>sq-char</i>   '

  <i>sq-char::</i>
    <i>escape-sequence</i>
    any member of the source character set except single-quote (') or backslash (\)
</pre>

**Defined elsewhere**

* [*escape-sequence*](#sec-String-Literals)

**Semantics**

A *character-literal* is a single character delimited by a pair of single quotes ('). The delimiters are not part of the literal's content.

The type of a *character-literal* is `Char`.

**Examples**

```
'A'           // A
'\t'          // Horizontal Tab
'\x41'        // A
'\u0041'      // A
'\U00000041'  // A
```

##### Integer Literals

**Syntax**

<pre>
  <i>integer-literal::</i>
    <i>decimal-literal</i>
    <i>hexadecimal-literal</i>

  <i>decimal-literal::</i>
    0
    <i>nonzero-decimal-literal</i>

  <i>nonzero-decimal-literal::</i>
    <i>nonzero-digit</i>
    <i>decimal-literal   digit</i>

  <i>nonzero-digit:: one of</i>
    1   2   3   4   5   6   7   8   9

  <i>digit:: one of</i>
    0   1   2   3   4   5   6   7   8   9

  <i>hexadecimal-literal::</i>
    <i>hexadecimal-prefix   hexadecimal-digit</i>
    <i>hexadecimal-literal   hexadecimal-digit</i>

  <i>hexadecimal-prefix:: one of</i>
    0x   0X

  <i>hexadecimal-digit:: one of</i>
    0   1   2   3   4   5   6   7   8   9   a
    b   c   d   e   f   A   B   C   D   E   F
</pre>

**Constraints**

The value of an *integer-literal* must be representable by its type.

**Semantics**

The value of a *decimal-literal* is computed using base 10; that of a *hexadecimal-literal*, base 16.

The type of an *integer-literal* is `Int`.

For an implementation using twos-complement integer representation for negative numbers, in which the smallest representable integer value is -9223372036854775808, when a *decimal-literal* representing the value 9223372036854775808 (2^63) appears as the token immediately following a [unary minus operator](#sec-Unary-Arithmetic-Operators) token, the result (of both tokens) is a literal of type `Int` with the value −9223372036854775808 (−2^63). Similarly, if the smallest representable integer value is -2147483648, when a *decimal-literal* representing the value 2147483648 (2^31) appears as the token immediately following a unary minus operator token, the result (of both tokens) is a literal of type `Int` with the value −2147483648 (−2^31).

**Examples**

```
10          // decimal 10
0xAbC123    // hexadecimal AbC123
```

##### Floating-Point Literals

**Syntax**

<pre>
  <i>ﬂoating-literal::</i>
    <i>fractional-literal   exponent-part<sub>opt</sub></i>
    <i>digit-sequence   exponent-part</i>

  <i>fractional-literal::</i>
    <i>digit-sequence</i>   .   <i>digit-sequence</i>

  <i>exponent-part::</i>
    e   <i>sign<sub>opt</sub>   digit-sequence</i>
    E   <i>sign<sub>opt</sub>   digit-sequence</i>

  <i>sign:: one of</i>
    +   -

  <i>digit-sequence::</i>
    <i>digit</i>
    <i>digit-sequence   digit</i>
</pre>

**Defined elsewhere**

* [*digit*](#sec-Integer-Literals)

**Constraints**

The value of a *floating-literal* must be representable by its type.

**Semantics**

The type of a *floating-literal* is `Float`.

**Examples**

```
1.23
1E3
1.0E-3
1.234e105
```

##### String Literals

**Syntax**

<pre>
  <i>string-literal::</i>
    "   <i>char-sequence<sub>opt</sub></i>   "

  <i>char-sequence::</i>
    <i>dq-char</i>
    <i>char-sequence   dq-char</i>

  <i>dq-char::</i>
    <i>escape-sequence</i>
    any member of the source character set except double-quote (") or backslash (\)

  <i>escape-sequence::</i>
    <i>simple-escape-sequence</i>
    <i>hexadecimal-escape-sequence</i>
    <i>unicode-escape-sequence</i>

  <i>simple-escape-sequence:: one of</i>
    \"   \'   \\   \?   \a   \b   \e   \f   \n   \r   \t   \v   \0

  <i>hexadecimal-escape-sequence::</i>
    \x   <i>hex-digit   hex-digit</i>

  <i>unicode-escape-sequence::</i>
    \u   <i>hex-digit  hex-digit  hex-digit  hex-digit</i>
    \U   <i>hex-digit  hex-digit  hex-digit  hex-digit  hex-digit  hex-digit  hex-digit  hex-digit </i>
</pre>

**Defined elsewhere**

* [*hex-digit*](#sec-Integer-Literals)

**Constraints**

A *unicode-escape-sequence* must not represent a Unicode code point beyond U+10FFFF, as this is outside the range UTF-8 can encode (see [RFC 3629](http://tools.ietf.org/html/rfc3629#section-3)).

A *unicode-escape-sequence* must not represent an invalid Unicode code point, such as the high and low surrogate halves used by UTF-16 (U+D800 through U+DFFF).

**Semantics**

A *string-literal* is a sequence of zero or more characters delimited by a pair of double quotes (''). The delimiters are not part of the literal's content.

The type of a *string-literal* is `String`.

An ***escape sequence*** represents a single-character encoding, as described in the table below:

Escape sequence | Character name | Unicode character
--------------- | --------------| ------
`\"`  | Double Quote | U+0022
`\'`  | Single Quote | U+0027
`\\`  | Backslash | U+005C
`\?`  | Question Mark | U+003F
`\a`  | Alert | U+0007
`\b`  | Backspace | U+0008
`\e`  | Escape | U+001B
`\f`  | Form Feed | U+000C
`\n`  | New Line | U+000A
`\r`  | Carriage Return | U+000D
`\t`  | Horizontal Tab | U+0009
`\v`  | Vertical Tab | U+000B
`\0`  | Null | U+0000
`\x`*hh* | 2-digit hexadecimal digit value *hh* | U+00*hh*
`\u`*xxxx* or `\U`*xxxxxxxx* | UTF-8 encoding of a Unicode code point | U+*xxxx*\[*xxxx*\]

**Examples**

```
"column A\tcolumn B\x09column C\n"
"C\x41T spells \"CAT\""
"C\u0041T spells \"CAT\""
"C\U00000041T spells \"CAT\""
```

##### Void Literal

**Syntax**

<pre>
  <i>void-literal::</i>
    void
</pre>

**Semantics**

The type of a *void-literal* is `void`.

**Examples**

```
fun f(): void {   // void as a return type
  …;
  void            // void as an expression/value literal
}
// -----------------------------------------
class MyContainer<T> {
  fun map_and_fold<R, T2>(f : (R, T) ~> (R, T2), R): (R, MyContainer<T2>) { … }
  fun map<T2>(f: T ~> T2): MyContainer<T2>) {
    snd(this.map_and_fold((_, x) ~> (void, f x), void))
  }
}
// -----------------------------------------
class Dict<K: Orderable, V> { … }
class Keyset<V: Orderable>(impl : Dict<V, void>) { … }
```

#### Operators and Punctuators

**Syntax**

<pre>
  <i>operator-or-punctuator:: one of</i>
    =  +  -  *  /  %  .  ->  ~>  =>  {  }  (  )  [  ]  ;  :  ::  ,
    ^  |  !  ||  &&  ==  !=  <  <=  >  >=  =.  &
</pre>

**Semantics**

Operators and punctuators are symbols that have independent syntactic and semantic significance.

***Operators*** are used in expressions to describe operations involving one or more ***operands***, and that yield a resulting value, produce a side effect, or some combination thereof.

***Punctuators*** are used for grouping and separating.
