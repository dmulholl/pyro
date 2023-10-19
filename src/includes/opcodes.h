#ifndef pyro_opcodes_h
#define pyro_opcodes_h

typedef enum {
    PYRO_OPCODE_ASSERT,
    PYRO_OPCODE_BINARY_AMP,
    PYRO_OPCODE_BINARY_BANG_EQUAL,
    PYRO_OPCODE_BINARY_BAR,
    PYRO_OPCODE_BINARY_CARET,
    PYRO_OPCODE_BINARY_EQUAL_EQUAL,
    PYRO_OPCODE_BINARY_GREATER,
    PYRO_OPCODE_BINARY_GREATER_EQUAL,
    PYRO_OPCODE_BINARY_GREATER_GREATER,
    PYRO_OPCODE_BINARY_IN,
    PYRO_OPCODE_BINARY_LESS,
    PYRO_OPCODE_BINARY_LESS_EQUAL,
    PYRO_OPCODE_BINARY_LESS_LESS,
    PYRO_OPCODE_BINARY_MINUS,
    PYRO_OPCODE_BINARY_PERCENT,
    PYRO_OPCODE_BINARY_PLUS,
    PYRO_OPCODE_BINARY_SLASH,
    PYRO_OPCODE_BINARY_SLASH_SLASH,
    PYRO_OPCODE_BINARY_STAR,
    PYRO_OPCODE_BINARY_STAR_STAR,
    PYRO_OPCODE_BREAK,
    PYRO_OPCODE_CALL_METHOD,
    PYRO_OPCODE_CALL_METHOD_WITH_UNPACK,
    PYRO_OPCODE_CALL_PUB_METHOD,
    PYRO_OPCODE_CALL_PUB_METHOD_WITH_UNPACK,
    PYRO_OPCODE_CALL_SUPER_METHOD,
    PYRO_OPCODE_CALL_SUPER_METHOD_WITH_UNPACK,
    PYRO_OPCODE_CALL_VALUE,
    PYRO_OPCODE_CALL_VALUE_0,
    PYRO_OPCODE_CALL_VALUE_1,
    PYRO_OPCODE_CALL_VALUE_2,
    PYRO_OPCODE_CALL_VALUE_3,
    PYRO_OPCODE_CALL_VALUE_4,
    PYRO_OPCODE_CALL_VALUE_5,
    PYRO_OPCODE_CALL_VALUE_6,
    PYRO_OPCODE_CALL_VALUE_7,
    PYRO_OPCODE_CALL_VALUE_8,
    PYRO_OPCODE_CALL_VALUE_9,
    PYRO_OPCODE_CALL_VALUE_WITH_UNPACK,
    PYRO_OPCODE_CONCAT_STRINGS,
    PYRO_OPCODE_CLOSE_UPVALUE,
    PYRO_OPCODE_DEFINE_PRI_FIELD,
    PYRO_OPCODE_DEFINE_PRI_GLOBAL,
    PYRO_OPCODE_DEFINE_PRI_GLOBALS,
    PYRO_OPCODE_DEFINE_PRI_METHOD,
    PYRO_OPCODE_DEFINE_PUB_FIELD,
    PYRO_OPCODE_DEFINE_PUB_GLOBAL,
    PYRO_OPCODE_DEFINE_PUB_GLOBALS,
    PYRO_OPCODE_DEFINE_PUB_METHOD,
    PYRO_OPCODE_DEFINE_STATIC_FIELD,
    PYRO_OPCODE_DEFINE_STATIC_METHOD,
    PYRO_OPCODE_DUP,
    PYRO_OPCODE_DUP_2,
    PYRO_OPCODE_ECHO,
    PYRO_OPCODE_END_WITH,
    PYRO_OPCODE_FORMAT,
    PYRO_OPCODE_GET_FIELD,
    PYRO_OPCODE_GET_GLOBAL,
    PYRO_OPCODE_GET_INDEX,
    PYRO_OPCODE_GET_ITERATOR,
    PYRO_OPCODE_GET_LOCAL,
    PYRO_OPCODE_GET_LOCAL_0,
    PYRO_OPCODE_GET_LOCAL_1,
    PYRO_OPCODE_GET_LOCAL_2,
    PYRO_OPCODE_GET_LOCAL_3,
    PYRO_OPCODE_GET_LOCAL_4,
    PYRO_OPCODE_GET_LOCAL_5,
    PYRO_OPCODE_GET_LOCAL_6,
    PYRO_OPCODE_GET_LOCAL_7,
    PYRO_OPCODE_GET_LOCAL_8,
    PYRO_OPCODE_GET_LOCAL_9,
    PYRO_OPCODE_GET_MEMBER,
    PYRO_OPCODE_GET_METHOD,
    PYRO_OPCODE_GET_NEXT_FROM_ITERATOR,
    PYRO_OPCODE_GET_PUB_FIELD,
    PYRO_OPCODE_GET_PUB_METHOD,
    PYRO_OPCODE_GET_SUPER_METHOD,
    PYRO_OPCODE_GET_UPVALUE,
    PYRO_OPCODE_IMPORT_MODULE,
    PYRO_OPCODE_IMPORT_NAMED_MEMBERS,
    PYRO_OPCODE_INHERIT,
    PYRO_OPCODE_JUMP,
    PYRO_OPCODE_JUMP_BACK,
    PYRO_OPCODE_JUMP_IF_ERR,
    PYRO_OPCODE_JUMP_IF_FALSE,
    PYRO_OPCODE_JUMP_IF_NOT_ERR,
    PYRO_OPCODE_JUMP_IF_NOT_KINDA_FALSEY,
    PYRO_OPCODE_JUMP_IF_NOT_NULL,
    PYRO_OPCODE_JUMP_IF_TRUE,
    PYRO_OPCODE_LOAD_CONSTANT,
    PYRO_OPCODE_LOAD_CONSTANT_0,
    PYRO_OPCODE_LOAD_CONSTANT_1,
    PYRO_OPCODE_LOAD_CONSTANT_2,
    PYRO_OPCODE_LOAD_CONSTANT_3,
    PYRO_OPCODE_LOAD_CONSTANT_4,
    PYRO_OPCODE_LOAD_CONSTANT_5,
    PYRO_OPCODE_LOAD_CONSTANT_6,
    PYRO_OPCODE_LOAD_CONSTANT_7,
    PYRO_OPCODE_LOAD_CONSTANT_8,
    PYRO_OPCODE_LOAD_CONSTANT_9,
    PYRO_OPCODE_LOAD_FALSE,
    PYRO_OPCODE_LOAD_I64_0,
    PYRO_OPCODE_LOAD_I64_1,
    PYRO_OPCODE_LOAD_I64_2,
    PYRO_OPCODE_LOAD_I64_3,
    PYRO_OPCODE_LOAD_I64_4,
    PYRO_OPCODE_LOAD_I64_5,
    PYRO_OPCODE_LOAD_I64_6,
    PYRO_OPCODE_LOAD_I64_7,
    PYRO_OPCODE_LOAD_I64_8,
    PYRO_OPCODE_LOAD_I64_9,
    PYRO_OPCODE_LOAD_NULL,
    PYRO_OPCODE_LOAD_TRUE,
    PYRO_OPCODE_MAKE_CLASS,
    PYRO_OPCODE_MAKE_CLOSURE,
    PYRO_OPCODE_MAKE_CLOSURE_WITH_DEF_ARGS,
    PYRO_OPCODE_MAKE_MAP,
    PYRO_OPCODE_MAKE_SET,
    PYRO_OPCODE_MAKE_TUP,
    PYRO_OPCODE_MAKE_VEC,
    PYRO_OPCODE_POP,
    PYRO_OPCODE_POP_ECHO_IN_REPL,
    PYRO_OPCODE_POP_JUMP_IF_FALSE,
    PYRO_OPCODE_RETURN,
    PYRO_OPCODE_RETURN_TUPLE,
    PYRO_OPCODE_SET_FIELD,
    PYRO_OPCODE_SET_GLOBAL,
    PYRO_OPCODE_SET_INDEX,
    PYRO_OPCODE_SET_LOCAL,
    PYRO_OPCODE_SET_LOCAL_0,
    PYRO_OPCODE_SET_LOCAL_1,
    PYRO_OPCODE_SET_LOCAL_2,
    PYRO_OPCODE_SET_LOCAL_3,
    PYRO_OPCODE_SET_LOCAL_4,
    PYRO_OPCODE_SET_LOCAL_5,
    PYRO_OPCODE_SET_LOCAL_6,
    PYRO_OPCODE_SET_LOCAL_7,
    PYRO_OPCODE_SET_LOCAL_8,
    PYRO_OPCODE_SET_LOCAL_9,
    PYRO_OPCODE_SET_PUB_FIELD,
    PYRO_OPCODE_SET_UPVALUE,
    PYRO_OPCODE_START_WITH,
    PYRO_OPCODE_STRINGIFY,
    PYRO_OPCODE_TRY,
    PYRO_OPCODE_UNARY_BANG,
    PYRO_OPCODE_UNARY_MINUS,
    PYRO_OPCODE_UNARY_PLUS,
    PYRO_OPCODE_UNARY_TILDE,
    PYRO_OPCODE_UNPACK,
} PyroOpcode;

#endif
