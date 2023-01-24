" Syntax definition file for the Pyro programming language.
" v0.4.1

" Comments.
syn match pyroComment "#.*$"

" Identifiers with a '$' prefix.
syn match pyroSpecial "$\w\+\>"

" Strings and characters.
syn region pyroBacktickedString start=+`+ end=+`+
syn region pyroDoubleQuotedString start=+"+ end=+"+ skip=+\\\\\|\\"+ contains=pyroEscaped
syn region pyroChar start=+'+ end=+'+ skip=+\\\\\|\\'+ contains=pyroEscaped

" Escapes inside string literals.
syn match pyroEscaped +\\[$abfnrtv'"\\]+ contained
syn match pyroEscaped "\\\o\{1,3}" contained
syn match pyroEscaped "\\x\x\{2}" contained
syn match pyroEscaped "\%(\\u\x\{4}\|\\U\x\{8}\)" contained
syn match pyroEscaped /${[_$a-zA-Z0-9 .,:;(){}'"]\+}/ contained

" Keywords.
syn keyword pyroKeyword var def class typedef with
syn keyword pyroKeyword pub pri static
syn keyword pyroKeyword if else for while in loop
syn keyword pyroKeyword return break continue
syn keyword pyroKeyword try echo
syn keyword pyroAssert assert
syn keyword pyroImport import as
syn keyword pyroConstant true false null self super

" Numbers.
syn case ignore
syn match pyroNumber "\<\(\d\|_\)\+\>"
syn match pyroNumber "\<0x\(\x\|_\)\+\>"
syn match pyroNumber "\<0o\(\o\|_\)\+\>"
syn match pyroNumber "\<0b[01_]\+\>"
syn match pyroNumber "\<\d\+\.\d*\(e[-+]\=\d\+\)\=\>"
syn match pyroNumber "\<\d\+e[-+]\=\d\+\>"
syn case match

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
hi def link pyroSpecial PreProc
hi def link pyroEscaped Constant

let b:current_syntax = "pyro"
