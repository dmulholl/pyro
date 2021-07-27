" Syntax definition file for the Pyro programming language.
" v0.3.0

" Comments.
syn match pyroComment "#.*$"

" Identifiers with a '$' prefix.
syn match pyroSpecial "\$\w\+\>"

" Strings and characters.
syn region pyroString start=+"+ end=+"+ skip=+\\\\\|\\"+
syn region pyroString start=+`+ end=+`+
syn region pyroChar start=+'+ end=+'+ skip=+\\\\\|\\'+

" Keywords.
syn keyword pyroKeyword var def class
syn keyword pyroKeyword if else for while in loop
syn keyword pyroKeyword return break continue
syn keyword pyroKeyword and or xor not
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
hi def link pyroString String
hi def link pyroKeyword Statement
hi def link pyroAssert PreProc
hi def link pyroImport PreProc
hi def link pyroConstant Special
hi def link pyroNumber Constant
hi def link pyroSpecial Normal
hi def link pyroChar Constant

let b:current_syntax = "pyro"
