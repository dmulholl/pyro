#ifndef pyro_opcodes_h
#define pyro_opcodes_h

#include "pyro.h"

typedef enum {
    OP_ASSERT,
    OP_BINARY_AMP,
    OP_BINARY_BANG_EQUAL,
    OP_BINARY_BAR,
    OP_BINARY_CARET,
    OP_BINARY_EQUAL_EQUAL,
    OP_BINARY_GREATER,
    OP_BINARY_GREATER_EQUAL,
    OP_BINARY_GREATER_GREATER,
    OP_BINARY_IN,
    OP_BINARY_LESS,
    OP_BINARY_LESS_EQUAL,
    OP_BINARY_LESS_LESS,
    OP_BINARY_MINUS,
    OP_BINARY_PERCENT,
    OP_BINARY_PLUS,
    OP_BINARY_SLASH,
    OP_BINARY_SLASH_SLASH,
    OP_BINARY_STAR,
    OP_BINARY_STAR_STAR,
    OP_BREAK,

    OP_CALL_VALUE,
    OP_CALL_WITH_UNPACK,

    OP_CALL_METHOD,
    OP_CALL_METHOD_WITH_UNPACK,

    OP_CALL_PUB_METHOD,
    OP_CALL_PUB_METHOD_WITH_UNPACK,

    OP_CALL_SUPER_METHOD,
    OP_CALL_SUPER_METHOD_WITH_UNPACK,

    OP_CLOSE_UPVALUE,
    OP_DEFINE_PRI_FIELD,
    OP_DEFINE_PUB_FIELD,
    OP_DEFINE_STATIC_FIELD,
    OP_DEFINE_PRI_GLOBAL,
    OP_DEFINE_PUB_GLOBAL,
    OP_DEFINE_PRI_GLOBALS,
    OP_DEFINE_PUB_GLOBALS,
    OP_DEFINE_PRI_METHOD,
    OP_DEFINE_PUB_METHOD,
    OP_DEFINE_STATIC_METHOD,
    OP_DUP,
    OP_DUP_2,
    OP_ECHO,
    OP_GET_FIELD,
    OP_GET_PUB_FIELD,
    OP_GET_GLOBAL,
    OP_GET_INDEX,
    OP_GET_NEXT_FROM_ITERATOR,
    OP_GET_ITERATOR,
    OP_GET_LOCAL,
    OP_GET_MEMBER,
    OP_GET_METHOD,
    OP_GET_PUB_METHOD,
    OP_GET_SUPER_METHOD,
    OP_GET_UPVALUE,
    OP_IMPORT_ALL_MEMBERS,
    OP_IMPORT_MODULE,
    OP_IMPORT_NAMED_MEMBERS,
    OP_INHERIT,
    OP_JUMP,
    OP_JUMP_BACK,
    OP_JUMP_IF_ERR,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_NOT_ERR,
    OP_JUMP_IF_NOT_NULL,
    OP_JUMP_IF_TRUE,
    OP_LOAD_CONSTANT,
    OP_LOAD_FALSE,
    OP_LOAD_I64_0,
    OP_LOAD_I64_1,
    OP_LOAD_I64_2,
    OP_LOAD_I64_3,
    OP_LOAD_I64_4,
    OP_LOAD_I64_5,
    OP_LOAD_I64_6,
    OP_LOAD_I64_7,
    OP_LOAD_I64_8,
    OP_LOAD_I64_9,
    OP_LOAD_NULL,
    OP_LOAD_TRUE,
    OP_MAKE_CLASS,
    OP_MAKE_CLOSURE,
    OP_MAKE_CLOSURE_WITH_DEF_ARGS,
    OP_MAKE_MAP,
    OP_MAKE_VEC,
    OP_POP,
    OP_POP_ECHO_IN_REPL,
    OP_POP_JUMP_IF_FALSE,
    OP_RETURN,
    OP_SET_FIELD,
    OP_SET_PUB_FIELD,
    OP_SET_GLOBAL,
    OP_SET_INDEX,
    OP_SET_LOCAL,
    OP_SET_UPVALUE,
    OP_TRY,
    OP_UNARY_BANG,
    OP_UNARY_MINUS,
    OP_UNARY_PLUS,
    OP_UNARY_TILDE,
    OP_UNPACK,
    OP_START_WITH,
    OP_END_WITH,
} OpCode;

#endif
