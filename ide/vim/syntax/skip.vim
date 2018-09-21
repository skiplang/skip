syn keyword skipKeywords        alias as async await children concurrent const
syn keyword skipKeywords        const deferred extends final from frozen fun 
syn keyword skipKeywords        memoized module native overridable
syn keyword skipKeywords        private protected static trait
syn keyword skipKeywords        uses when with is return in yield
syn keyword skipKeywords        mutable readonly
syn keyword skipKeywords        class type this
syn keyword skipKeywords        fun const nextgroup=skipFunName skipwhite
syn keyword skipKeywords        class nextgroup=skipClassName skipwhite

syn match skipFunName           "[a-z][a-zA-Z0-9_]*" display contained
syn match skipClassName         "[A-Z][a-zA-Z0-9_]*" display contained

syn keyword skipClassModifier   extension base

syn keyword skipExceptions      try catch throw

syn keyword skipConditional     if else break continue match

syn keyword skipLoop            do while loop for

syn match skipNumber            "\<\d\+\(\.\d\+\)\=\>"

syn keyword skipBool            true false

syn match skipColon             ":" nextgroup=skipType skipwhite
syn match skipType              "[A-Z][a-zA-Z0-9_]*" display contained


syn region skipComment          start="/\*" end="\*/" contains=skipComment,@Spell
syn match skipLineComment       "//.*" contains=@Spell

let b:current_syntax = "skip"

hi def link skipKeywords        Keyword
hi def link skipFunName         Function
hi def link skipClassName       Structure
hi def link skipClassModifier   StorageClass
hi def link skipExceptions      Exception
hi def link skipConditional     Conditional
hi def link skipLoop            Repeat
hi def link skipNumber          Number
hi def link skipBool            Constant
hi def link skipComment         Comment
hi def link skipType            Type
