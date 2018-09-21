# Conformance

The following definitions are useful when considering conformance to this specification:

  * ***behavior***: external appearance or action
  * ***behavior, implementation-defined***: behavior specific to an implementation, where that implementation must document that behavior
  * ***behavior, undefined***: behavior that is not guaranteed to produce any specific result. Usually follows an erroneous program construct or data
  * ***behavior, unspecified***: behavior for which this specification provides no requirements
  * ***constraint***: restriction, either syntactic or semantic, on how language elements can be used

In this specification, “must” is to be interpreted as a requirement on an implementation or on a program; conversely, “must not” is to be interpreted as a prohibition.

If a “must” or “must not” requirement that appears outside of a constraint is violated, the behavior is undefined. Undefined behavior is otherwise indicated in this specification by the words “undefined behavior” or by the omission of any explicit definition of behavior. There is no difference in emphasis among these three; they all describe “behavior that is undefined”.

The word “may” indicates “permission”, and is never used to mean “might”.

A ***strictly conforming program*** must use only those features of the language described in this specification. In particular, it must not produce output dependent on any unspecified, undefined, or implementation-defined behavior.

A ***conforming implementation*** must accept any strictly conforming program. A conforming implementation may have extensions, provided they do not alter the behavior of any strictly conforming program.

A ***conforming program*** is one that is acceptable to a conforming implementation.

A conforming implementation must be accompanied by a document that defines all implementation-defined characteristics and all extensions.

Some Syntax sections are followed by a Constraints section, which further restricts the grammar. After issuing a diagnostic for a constraint violation, a conforming implementation may continue program execution. Making such things constraint violations simply forces the issuance of a diagnostic; it does not require that program execution terminate.

This specification contains explanatory material—called ***informative*** or ***non-normative*** text—that, strictly speaking, is not necessary in a formal language specification. Examples are provided to illustrate possible forms of the constructions described. References are used to refer to related clauses. Notes and Implementer Notes are provided to give advice or guidance to implementers or programmers, respectively. Informative annexes provide additional information and summarize the information contained in this specification. All text not marked as informative is ***normative***.
