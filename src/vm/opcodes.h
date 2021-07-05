#ifndef pyro_opcodes_h
#define pyro_opcodes_h

#include "common.h"

typedef enum {
    OP_ASSERT,
    OP_BREAK,
    OP_LOAD_CONSTANT,
    OP_LOAD_NULL,
    OP_LOAD_TRUE,
    OP_LOAD_FALSE,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_FIELD,
    OP_SET_FIELD,
    OP_GET_METHOD,
    OP_GET_SUPER_METHOD,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_GREATER_EQUAL,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_FLOAT_DIV,
    OP_TRUNC_DIV,
    OP_NOT,
    OP_NEGATE,
    OP_ECHO,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_POP_JUMP_IF_FALSE,
    OP_JUMP_IF_NOT_ERR,
    OP_JUMP_IF_NOT_NULL,
    OP_JUMP_IF_TRUE,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE_METHOD,
    OP_INVOKE_SUPER_METHOD,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD,
    OP_GET_MEMBER,
    OP_IMPORT,
    OP_FIELD,
    OP_DUP,
    OP_TRY,
    OP_JUMP_IF_ERR,
    OP_ITER,
    OP_ITER_NEXT,
    OP_MAKE_VEC,
    OP_MAKE_MAP,
    OP_GET_INDEX,
    OP_SET_INDEX,
    OP_DUP2,
    OP_POWER,
} OpCode;

#endif
