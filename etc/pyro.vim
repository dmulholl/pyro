" Syntax definition file for the Pyro programming language.
" Version: v11.

" Comments.
syn match pyroComment "#.*$"

" Identifiers with a '$' prefix.
syn match pyroDollar "[$]\w*"

" Strings and characters.
syn region pyroBacktickedString start=+`+ end=+`+
syn region pyroDoubleQuotedString start=+"+ end=+"+ skip=+\\\\\|\\"+ contains=pyroEscape,pyroInterpolation
syn region pyroChar start=+'+ end=+'+ skip=+\\\\\|\\'+ contains=pyroEscape

" Escapes inside string literals.
syn match pyroEscape #\\[0befnrtv$'"\\]# contained
syn match pyroEscape #\\x\x\{2}# contained
syn match pyroEscape #\%(\\u\x\{4}\|\\U\x\{8}\)# contained
syn match pyroInterpolation #${[_$a-zA-Z0-9 .,:;?()\[\]`'"!&|^*/%+-]\+}# contained

" Keywords.
syn keyword pyroKeyword var let def class typedef with extends interface enum
syn keyword pyroKeyword pub pri static
syn keyword pyroKeyword if else for while in loop
syn keyword pyroKeyword return break continue
syn keyword pyroKeyword try echo
syn keyword pyroKeyword mod rem
syn keyword pyroAssert assert
syn keyword pyroImport import as
syn keyword pyroConstant true false null self super

" Numbers.
syn case ignore
syn match pyroNumber "\<\d\+\(\d\|_\)*\>"
syn match pyroNumber "\<0x\(\x\|_\)\+\>"
syn match pyroNumber "\<0o\(\o\|_\)\+\>"
syn match pyroNumber "\<0b[01_]\+\>"
syn match pyroNumber "\<\d\(\d\|_\)*\.\(\d\|_\)\+\(e[-+]\=\(\d\|_\)\+\)\=\>"
syn match pyroNumber "\<\d\(\d\|_\)*e[-+]\=\(\d\|_\)\+\>"
syn case match

" Operators.
syn match pyroOperator "||"
syn match pyroOperator "&&"
syn match pyroOperator "??"
syn match pyroOperator "!!"
syn match pyroOperator "!="
syn match pyroOperator "=="
syn match pyroOperator "<="
syn match pyroOperator ">="
syn match pyroOperator ":?"
syn match pyroOperator ":|"
syn match pyroOperator "<<"
syn match pyroOperator ">>"
syn match pyroOperator "+="
syn match pyroOperator "-="

" Default highlighting styles.
hi def link pyroComment Comment
hi def link pyroDoubleQuotedString String
hi def link pyroBacktickedString String
hi def link pyroChar String
hi def link pyroKeyword Statement
hi def link pyroAssert PreProc
hi def link pyroImport PreProc
hi def link pyroConstant Special
hi def link pyroNumber Constant
hi def link pyroDollar PreProc
hi def link pyroEscape Constant
hi def link pyroInterpolation Constant
hi def link pyroOperator Statement

let b:current_syntax = "pyro"
