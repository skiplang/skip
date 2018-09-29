syn keyword skipKeyword         alias as async await children from extends in
syn keyword skipKeyword         macro module end trait type uses with
syn keyword skipKeyword         thisClassName ThisClass forEachField
syn keyword skipKeyword         fun const nextgroup=skipFunName skipwhite
syn keyword skipKeyword         class nextgroup=skipClassName skipwhite

syn match skipFunName           "[a-z][a-zA-Z0-9_]*" display contained
syn match skipClassName         "[A-Z][a-zA-Z0-9_]*" display contained

syn keyword skipAnnotation      concurrent deferred final frozen memoized
syn keyword skipAnnotation      mutable native overridable private protected
syn keyword skipAnnotation      readonly static untracked

syn keyword skipType            void

syn keyword skipClassModifier   extension base

syn keyword skipException       try catch throw

syn keyword skipConditional     if else break continue return match is yield

syn keyword skipRepeat          do while loop for

syn keyword skipIdentifier      this

syn match skipNumber            "\<[+-]\?\d\+\(\.\d\+\)\=\>"

syn keyword skipBool            true false

syn region skipChar             matchgroup=skipDelimiter start=+'+ skip=+\\\\\|\\"+ end=+'+ contains=skipEscape

syn region skipString           matchgroup=skipDelimiter start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=skipEscape fold

syn region skipInterpString     matchgroup=skipDelimiter start=+`+ skip=+\\\\\|\\"+ end=+`+ contains=skipEscape,skipInterpolate fold

syn match skipEscape            contained '\\[0abefnrtv\\\'"`\${]'
syn match skipEscape            contained '\\x[0-9A-Fa-f]\{2,2\}' contains=skipEscapeHex
syn match skipEscape            contained '\\u[0-9A-Fa-f]\{4,4\}' contains=skipEscapeHex
syn match skipEscape            contained '\\U[0-9A-Fa-f]\{8,8\}' contains=skipEscapeHex
syn match skipEscapeHex         contained '[0-9A-Fa-f]'
syn region skipInterpolate      matchgroup=skipDelimiter start=+${+ end=+}+ transparent fold

syn match skipOperator          '[;:.,()[\]|&@~?+*/%<!=>#]'
syn match skipOperator          '[\-^]'
syn match skipOperator          '\<_\>'
syn region skipBlock            matchgroup=skipOperator start='{' end='}' transparent fold
syn match skipOperator          ":" nextgroup=skipType skipwhite

syn match skipType              "[A-Z][a-zA-Z0-9_]*" display contained

syn region skipComment          start="/\*" end="\*/" contains=@Spell
syn match skipLineComment       "//.*$" contains=@Spell

let b:current_syntax = "skip"

hi def link skipKeyword         Keyword
hi def link skipAnnotation      Delimiter
hi def link skipType            Identifier
hi def link skipFunName         Function
hi def link skipClassName       Structure
hi def link skipClassModifier   StorageClass
hi def link skipException       Exception
hi def link skipConditional     Conditional
hi def link skipRepeat          Repeat
hi def link skipIdentifier      Identifier
hi def link skipNumber          Number
hi def link skipBool            Constant
hi def link skipDelimiter       Delimiter
hi def link skipChar            Character
hi def link skipString          String
hi def link skipInterpString    String
hi def link skipEscape          Keyword
hi def link skipEscapeHex       Constant
hi def link skipComment         Comment
hi def link skipLineComment     Comment
hi def link skipOperator        Operator
hi def link skipType            Type
hi def link skipDelimiter       Delimiter
