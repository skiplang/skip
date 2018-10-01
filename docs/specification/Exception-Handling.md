# Exception Handling

## General

An ***exception*** is some unusual condition in that it is outside the ordinary expected behavior. (Examples include dealing with situations in which a critical resource is needed, but is unavailable, and detecting an out-of-range value for some computation.) As such, exceptions require special handling. This chapter describes how exceptions can be created and handled.

Whenever some exceptional condition is detected at runtime, an exception is ***thrown***. An associated exception handler can ***catch*** the thrown exception and service it. Among other things, the handler might recover from the situation completely (allowing the program to continue execution), it might perform some recovery and then throw an exception to get further help, or it might perform some cleanup action and terminate the program. Exceptions may be thrown on behalf of the environment or by explicit request in a program.

Exception handling involves the use of the following constructs:

* [`try`](Expressions.md#the-try-expression), which allows a ***try-block*** of code containing one or more possible exception generations, to be tried

* [`catch`](Expressions.md#the-try-expression), which defines a ***catch-handler*** for each expected exception type thrown from the corresponding try-block or from some function it calls

* [`throw`](Expressions.md#the-throw-expression), which generates an exception of a given type, from a place called a ***throw point***.

When an exception is thrown, an ***exception object*** of some subclass type of [`Exception`](RTL-type-Exception)  is created and made available to the associated catch-handler. That object encapsulates the fields of the thrown exception object, which the handler can use to determine the course of action.

The catch-handler uses pattern-matching to determine the type of the exception thrown.

## Predefined Exception Classes

Apart from the base class [`Exception`](RTL-type-Exception), the set of predefined exception types includes the following: [`DivisionByZeroException`](RTL-type-DivisionByZeroException), [`Invalid Index`](RTL-type-Invalid-Index), [`InvalidSize`](RTL-type-InvalidSize), [`InvariantViolation`](RTL-type-InvariantViolation), [`OutOfBounds`](RTL-type-OutOfBounds), and [`UnresolvedType`](RTL-type-UnresolvedType).

## User-Defined Exceptions

An exception class is defined by having it extend class [`Exception`](RTL-type-Exception) and providing a `getMessage` method with the appropriate signature and return type.
