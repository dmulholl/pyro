#include "../includes/pyro.h"


// A single-byte instruction with no arguments.
static size_t atomic_instruction(PyroVM* vm, const char* name, size_t ip) {
    pyro_stdout_write_f(vm, "%s\n", name);
    return ip + 1;
}


// An instruction with a two-byte argument which indexes into the constants table.
static size_t constant_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint16_t index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-32s %4d    ", name, index);
    pyro_dump_value(vm, fn->constants[index]);
    pyro_stdout_write_f(vm, "\n");
    return ip + 3;
}


// An instruction with a one-byte argument representing a uint8_t.
static size_t u8_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint8_t arg = fn->code[ip + 1];
    pyro_stdout_write_f(vm, "%-32s %4d\n", name, arg);
    return ip + 2;
}


// An instruction with two one-byte arguments each representing a uint8_t.
static size_t u8_x2_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint8_t arg1 = fn->code[ip + 1];
    uint8_t arg2 = fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-32s %4d %4d\n", name, arg1, arg2);
    return ip + 3;
}


// An instruction with a two-byte argument representing a uint16_t in big-endian format.
static size_t u16_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint16_t arg = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-32s %4d\n", name, arg);
    return ip + 3;
}


// An instruction with two two-byte arguments representing uint16_t values in big-endian format.
static size_t u16_x2_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint16_t arg1 = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    uint16_t arg2 = (fn->code[ip + 3] << 8) | fn->code[ip + 4];
    pyro_stdout_write_f(vm, "%-32s %4d %4d\n", name, arg1, arg2);
    return ip + 5;
}


// A jump instruction with a 2-byte argument.
static size_t jump_instruction(PyroVM* vm, const char* name, int sign, PyroFn* fn, size_t ip) {
    uint16_t offset = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-32s %4d -> %d\n", name, ip, ip + 3 + sign * offset);
    return ip + 3;
}


// A method-invoking instruction.
static size_t invoke_instruction(PyroVM* vm, const char* name, PyroFn* fn, size_t ip) {
    uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    uint8_t arg_count = fn->code[ip + 3];
    pyro_stdout_write_f(vm, "%-32s %4d    ", name, const_index);
    pyro_dump_value(vm, fn->constants[const_index]);
    pyro_stdout_write_f(vm, "    (%d args)\n", arg_count);
    return ip + 4;
}


// Returns the index of the next instruction, if there is one.
size_t pyro_disassemble_instruction(PyroVM* vm, PyroFn* fn, size_t ip) {
    pyro_stdout_write_f(vm, "%04d ", ip);
    if (ip > 0 && PyroFn_get_line_number(fn, ip) == PyroFn_get_line_number(fn, ip - 1)) {
        pyro_stdout_write_f(vm, "   |    ");
    } else {
        pyro_stdout_write_f(vm, "%4zu    ", PyroFn_get_line_number(fn, ip));
    }

    uint8_t instruction = fn->code[ip];

    switch (instruction) {
        case PYRO_OPCODE_ASSERT_FAILED:
            return atomic_instruction(vm, "ASSERT_FAILED", ip);

        case PYRO_OPCODE_BINARY_PLUS:
            return atomic_instruction(vm, "BINARY_PLUS", ip);

        case PYRO_OPCODE_BINARY_AMP:
            return atomic_instruction(vm, "BINARY_AMP", ip);

        case PYRO_OPCODE_UNARY_TILDE:
            return atomic_instruction(vm, "UNARY_TILDE", ip);

        case PYRO_OPCODE_BINARY_BAR:
            return atomic_instruction(vm, "BINARY_BAR", ip);

        case PYRO_OPCODE_BINARY_IN:
            return atomic_instruction(vm, "BINARY_IN", ip);

        case PYRO_OPCODE_BINARY_CARET:
            return atomic_instruction(vm, "BINARY_CARET", ip);

        case PYRO_OPCODE_CALL_VALUE:
            return u8_instruction(vm, "CALL_VALUE", fn, ip);

        case PYRO_OPCODE_CALL_VALUE_WITH_UNPACK:
            return u8_instruction(vm, "CALL_VALUE_WITH_UNPACK", fn, ip);

        case PYRO_OPCODE_MAKE_CLASS:
            return constant_instruction(vm, "MAKE_CLASS", fn, ip);

        case PYRO_OPCODE_CLOSE_UPVALUE:
            return atomic_instruction(vm, "CLOSE_UPVALUE", ip);

        case PYRO_OPCODE_MAKE_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ip += 3;

            pyro_stdout_write_f(vm, "%-32s %4d    ", "MAKE_CLOSURE", const_index);
            pyro_dump_value(vm, fn->constants[const_index]);
            pyro_stdout_write_f(vm, "\n");

            PyroFn* wrapped_fn = PYRO_AS_PYRO_FN(fn->constants[const_index]);
            for (size_t i = 0; i < wrapped_fn->upvalue_count; i++) {
                int is_local = wrapped_fn->code[ip++];
                int index = wrapped_fn->code[ip++];
                pyro_stdout_write_f(vm, "%04d    |      <%s %d>\n", ip - 2, is_local ? "local" : "upvalue", index);
            }

            return ip;
        }

        case PYRO_OPCODE_MAKE_CLOSURE_WITH_DEFAULT_ARGS: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ip += 4;

            pyro_stdout_write_f(vm, "%-32s %4d    ", "MAKE_CLOSURE_WITH_DEFAULT_ARGS", const_index);
            pyro_dump_value(vm, fn->constants[const_index]);
            pyro_stdout_write_f(vm, "\n");

            PyroFn* wrapped_fn = PYRO_AS_PYRO_FN(fn->constants[const_index]);
            for (size_t i = 0; i < wrapped_fn->upvalue_count; i++) {
                int is_local = wrapped_fn->code[ip++];
                int index = wrapped_fn->code[ip++];
                pyro_stdout_write_f(vm, "%04d    |      <%s %d>\n", ip - 2, is_local ? "local" : "upvalue", index);
            }

            return ip;
        }

        case PYRO_OPCODE_DEFINE_PRI_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            ip += 2;

            pyro_stdout_write_f(vm, "%-32s %4d\n", "DEFINE_PRI_GLOBALS", count);

            for (uint8_t i = 0; i < count; i++) {
                uint16_t index = (fn->code[ip] << 8) | fn->code[ip + 1];
                pyro_stdout_write_f(vm, "%04d    |    %4d    ", ip, index);
                pyro_dump_value(vm, fn->constants[index]);
                pyro_stdout_write_f(vm, "\n");
                ip += 2;
            }

            return ip;
        }

        case PYRO_OPCODE_DEFINE_PUB_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            ip += 2;

            pyro_stdout_write_f(vm, "%-32s %4d\n", "DEFINE_PUB_GLOBALS", count);

            for (uint8_t i = 0; i < count; i++) {
                uint16_t index = (fn->code[ip] << 8) | fn->code[ip + 1];
                pyro_stdout_write_f(vm, "%04d    |    %4d    ", ip, index);
                pyro_dump_value(vm, fn->constants[index]);
                pyro_stdout_write_f(vm, "\n");
                ip += 2;
            }

            return ip;
        }

        case PYRO_OPCODE_DEFINE_PRI_GLOBAL:
            return constant_instruction(vm, "DEFINE_PRI_GLOBAL", fn, ip);

        case PYRO_OPCODE_DEFINE_PUB_GLOBAL:
            return constant_instruction(vm, "DEFINE_PUB_GLOBAL", fn, ip);

        case PYRO_OPCODE_DUP:
            return atomic_instruction(vm, "DUP", ip);

        case PYRO_OPCODE_STRINGIFY:
            return atomic_instruction(vm, "STRINGIFY", ip);

        case PYRO_OPCODE_FORMAT:
            return atomic_instruction(vm, "FORMAT", ip);

        case PYRO_OPCODE_DUP_2:
            return atomic_instruction(vm, "DUP_2", ip);

        case PYRO_OPCODE_ECHO:
            return u8_instruction(vm, "ECHO", fn, ip);

        case PYRO_OPCODE_BINARY_EQUAL_EQUAL:
            return atomic_instruction(vm, "BINARY_EQUAL_EQUAL", ip);

        case PYRO_OPCODE_DEFINE_PRI_FIELD:
            return constant_instruction(vm, "DEFINE_PRI_FIELD", fn, ip);

        case PYRO_OPCODE_DEFINE_PUB_FIELD:
            return constant_instruction(vm, "DEFINE_PUB_FIELD", fn, ip);

        case PYRO_OPCODE_DEFINE_STATIC_FIELD:
            return constant_instruction(vm, "DEFINE_STATIC_FIELD", fn, ip);

        case PYRO_OPCODE_BINARY_SLASH:
            return atomic_instruction(vm, "BINARY_SLASH", ip);

        case PYRO_OPCODE_GET_FIELD:
            return constant_instruction(vm, "GET_FIELD", fn, ip);

        case PYRO_OPCODE_GET_PUB_FIELD:
            return constant_instruction(vm, "GET_PUB_FIELD", fn, ip);

        case PYRO_OPCODE_GET_GLOBAL:
            return constant_instruction(vm, "GET_GLOBAL", fn, ip);

        case PYRO_OPCODE_GET_INDEX:
            return atomic_instruction(vm, "GET_INDEX", ip);

        case PYRO_OPCODE_GET_LOCAL:
            return u8_instruction(vm, "GET_LOCAL", fn, ip);

        case PYRO_OPCODE_GET_MEMBER:
            return constant_instruction(vm, "GET_MEMBER", fn, ip);

        case PYRO_OPCODE_GET_METHOD:
            return constant_instruction(vm, "GET_METHOD", fn, ip);

        case PYRO_OPCODE_GET_PUB_METHOD:
            return constant_instruction(vm, "GET_PUB_METHOD", fn, ip);

        case PYRO_OPCODE_GET_SUPER_METHOD:
            return constant_instruction(vm, "GET_SUPER_METHOD", fn, ip);

        case PYRO_OPCODE_GET_UPVALUE:
            return u8_instruction(vm, "GET_UPVALUE", fn, ip);

        case PYRO_OPCODE_BINARY_GREATER:
            return atomic_instruction(vm, "BINARY_GREATER", ip);

        case PYRO_OPCODE_BINARY_GREATER_EQUAL:
            return atomic_instruction(vm, "BINARY_GREATER_EQUAL", ip);

        case PYRO_OPCODE_IMPORT_MODULE:
            return u8_instruction(vm, "IMPORT_MODULE", fn, ip);

        case PYRO_OPCODE_IMPORT_NAMED_MEMBERS:
            return u8_x2_instruction(vm, "IMPORT_NAMED_MEMBERS", fn, ip);

        case PYRO_OPCODE_INHERIT:
            return atomic_instruction(vm, "INHERIT", ip);

        case PYRO_OPCODE_CALL_METHOD:
            return invoke_instruction(vm, "CALL_METHOD", fn, ip);

        case PYRO_OPCODE_CALL_PUB_METHOD:
            return invoke_instruction(vm, "CALL_PUB_METHOD", fn, ip);

        case PYRO_OPCODE_CALL_METHOD_WITH_UNPACK:
            return invoke_instruction(vm, "CALL_METHOD_WITH_UNPACK", fn, ip);

        case PYRO_OPCODE_CALL_PUB_METHOD_WITH_UNPACK:
            return invoke_instruction(vm, "CALL_PUB_METHOD_WITH_UNPACK", fn, ip);

        case PYRO_OPCODE_CALL_SUPER_METHOD:
            return invoke_instruction(vm, "CALL_SUPER_METHOD", fn, ip);

        case PYRO_OPCODE_CALL_SUPER_METHOD_WITH_UNPACK:
            return invoke_instruction(vm, "CALL_SUPER_METHOD_WITH_UNPACK", fn, ip);

        case PYRO_OPCODE_GET_ITERATOR:
            return atomic_instruction(vm, "GET_ITERATOR", ip);

        case PYRO_OPCODE_GET_NEXT_FROM_ITERATOR:
            return atomic_instruction(vm, "GET_NEXT_FROM_ITERATOR", ip);

        case PYRO_OPCODE_JUMP:
            return jump_instruction(vm, "JUMP", 1, fn, ip);

        case PYRO_OPCODE_JUMP_IF_ERR:
            return jump_instruction(vm, "JUMP_IF_ERR", 1, fn, ip);

        case PYRO_OPCODE_JUMP_IF_FALSE:
            return jump_instruction(vm, "JUMP_IF_FALSE", 1, fn, ip);

        case PYRO_OPCODE_JUMP_IF_NOT_ERR:
            return jump_instruction(vm, "JUMP_IF_NOT_ERR", 1, fn, ip);

        case PYRO_OPCODE_JUMP_IF_NOT_NULL:
            return jump_instruction(vm, "JUMP_IF_NOT_NULL", 1, fn, ip);

        case PYRO_OPCODE_JUMP_IF_TRUE:
            return jump_instruction(vm, "JUMP_IF_TRUE", 1, fn, ip);

        case PYRO_OPCODE_BINARY_LESS:
            return atomic_instruction(vm, "BINARY_LESS", ip);

        case PYRO_OPCODE_BINARY_LESS_EQUAL:
            return atomic_instruction(vm, "BINARY_LESS_EQUAL", ip);

        case PYRO_OPCODE_LOAD_CONSTANT:
            return constant_instruction(vm, "LOAD_CONSTANT", fn, ip);

        case PYRO_OPCODE_LOAD_FALSE:
            return atomic_instruction(vm, "LOAD_FALSE", ip);

        case PYRO_OPCODE_LOAD_I64_0:
            return atomic_instruction(vm, "LOAD_I64_0", ip);

        case PYRO_OPCODE_LOAD_I64_1:
            return atomic_instruction(vm, "LOAD_I64_1", ip);

        case PYRO_OPCODE_LOAD_I64_2:
            return atomic_instruction(vm, "LOAD_I64_2", ip);

        case PYRO_OPCODE_LOAD_I64_3:
            return atomic_instruction(vm, "LOAD_I64_3", ip);

        case PYRO_OPCODE_LOAD_I64_4:
            return atomic_instruction(vm, "LOAD_I64_4", ip);

        case PYRO_OPCODE_LOAD_I64_5:
            return atomic_instruction(vm, "LOAD_I64_5", ip);

        case PYRO_OPCODE_LOAD_I64_6:
            return atomic_instruction(vm, "LOAD_I64_6", ip);

        case PYRO_OPCODE_LOAD_I64_7:
            return atomic_instruction(vm, "LOAD_I64_7", ip);

        case PYRO_OPCODE_LOAD_I64_8:
            return atomic_instruction(vm, "LOAD_I64_8", ip);

        case PYRO_OPCODE_LOAD_I64_9:
            return atomic_instruction(vm, "LOAD_I64_9", ip);

        case PYRO_OPCODE_GET_LOCAL_0:
            return atomic_instruction(vm, "GET_LOCAL_0", ip);

        case PYRO_OPCODE_GET_LOCAL_1:
            return atomic_instruction(vm, "GET_LOCAL_1", ip);

        case PYRO_OPCODE_GET_LOCAL_2:
            return atomic_instruction(vm, "GET_LOCAL_2", ip);

        case PYRO_OPCODE_GET_LOCAL_3:
            return atomic_instruction(vm, "GET_LOCAL_3", ip);

        case PYRO_OPCODE_GET_LOCAL_4:
            return atomic_instruction(vm, "GET_LOCAL_4", ip);

        case PYRO_OPCODE_GET_LOCAL_5:
            return atomic_instruction(vm, "GET_LOCAL_5", ip);

        case PYRO_OPCODE_GET_LOCAL_6:
            return atomic_instruction(vm, "GET_LOCAL_6", ip);

        case PYRO_OPCODE_GET_LOCAL_7:
            return atomic_instruction(vm, "GET_LOCAL_7", ip);

        case PYRO_OPCODE_GET_LOCAL_8:
            return atomic_instruction(vm, "GET_LOCAL_8", ip);

        case PYRO_OPCODE_GET_LOCAL_9:
            return atomic_instruction(vm, "GET_LOCAL_9", ip);

        case PYRO_OPCODE_SET_LOCAL_0:
            return atomic_instruction(vm, "SET_LOCAL_0", ip);

        case PYRO_OPCODE_SET_LOCAL_1:
            return atomic_instruction(vm, "SET_LOCAL_1", ip);

        case PYRO_OPCODE_SET_LOCAL_2:
            return atomic_instruction(vm, "SET_LOCAL_2", ip);

        case PYRO_OPCODE_SET_LOCAL_3:
            return atomic_instruction(vm, "SET_LOCAL_3", ip);

        case PYRO_OPCODE_SET_LOCAL_4:
            return atomic_instruction(vm, "SET_LOCAL_4", ip);

        case PYRO_OPCODE_SET_LOCAL_5:
            return atomic_instruction(vm, "SET_LOCAL_5", ip);

        case PYRO_OPCODE_SET_LOCAL_6:
            return atomic_instruction(vm, "SET_LOCAL_6", ip);

        case PYRO_OPCODE_SET_LOCAL_7:
            return atomic_instruction(vm, "SET_LOCAL_7", ip);

        case PYRO_OPCODE_SET_LOCAL_8:
            return atomic_instruction(vm, "SET_LOCAL_8", ip);

        case PYRO_OPCODE_SET_LOCAL_9:
            return atomic_instruction(vm, "SET_LOCAL_9", ip);

        case PYRO_OPCODE_LOAD_NULL:
            return atomic_instruction(vm, "LOAD_NULL", ip);

        case PYRO_OPCODE_LOAD_TRUE:
            return atomic_instruction(vm, "LOAD_TRUE", ip);

        case PYRO_OPCODE_JUMP_BACK:
            return jump_instruction(vm, "JUMP_BACK", -1, fn, ip);

        case PYRO_OPCODE_BINARY_LESS_LESS:
            return atomic_instruction(vm, "BINARY_LESS_LESS", ip);

        case PYRO_OPCODE_MAKE_MAP:
            return u16_instruction(vm, "MAKE_MAP", fn, ip);

        case PYRO_OPCODE_MAKE_OBJECT:
            return u16_instruction(vm, "MAKE_OBJECT", fn, ip);

        case PYRO_OPCODE_MAKE_SET:
            return u16_instruction(vm, "MAKE_SET", fn, ip);

        case PYRO_OPCODE_MAKE_TUP:
            return u16_instruction(vm, "MAKE_TUP", fn, ip);

        case PYRO_OPCODE_MAKE_VEC:
            return u16_instruction(vm, "MAKE_VEC", fn, ip);

        case PYRO_OPCODE_DEFINE_PRI_METHOD:
            return constant_instruction(vm, "DEFINE_PRI_METHOD", fn, ip);

        case PYRO_OPCODE_DEFINE_PUB_METHOD:
            return constant_instruction(vm, "DEFINE_PUB_METHOD", fn, ip);

        case PYRO_OPCODE_DEFINE_STATIC_METHOD:
            return constant_instruction(vm, "DEFINE_STATIC_METHOD", fn, ip);

        case PYRO_OPCODE_BINARY_PERCENT:
            return atomic_instruction(vm, "BINARY_PERCENT", ip);

        case PYRO_OPCODE_BINARY_STAR:
            return atomic_instruction(vm, "BINARY_STAR", ip);

        case PYRO_OPCODE_UNARY_MINUS:
            return atomic_instruction(vm, "UNARY_MINUS", ip);

        case PYRO_OPCODE_UNARY_PLUS:
            return atomic_instruction(vm, "UNARY_PLUS", ip);

        case PYRO_OPCODE_UNARY_BANG:
            return atomic_instruction(vm, "UNARY_BANG", ip);

        case PYRO_OPCODE_BINARY_BANG_EQUAL:
            return atomic_instruction(vm, "BINARY_BANG_EQUAL", ip);

        case PYRO_OPCODE_POP:
            return atomic_instruction(vm, "POP", ip);

        case PYRO_OPCODE_POP_ECHO_IN_REPL:
            return atomic_instruction(vm, "POP_ECHO_IN_REPL", ip);

        case PYRO_OPCODE_POP_JUMP_IF_FALSE:
            return jump_instruction(vm, "POP_JUMP_IF_FALSE", 1, fn, ip);

        case PYRO_OPCODE_BINARY_STAR_STAR:
            return atomic_instruction(vm, "BINARY_STAR_STAR", ip);

        case PYRO_OPCODE_RETURN:
            return atomic_instruction(vm, "RETURN", ip);

        case PYRO_OPCODE_RETURN_TUPLE:
            return u8_instruction(vm, "RETURN_TUPLE", fn, ip);

        case PYRO_OPCODE_BINARY_GREATER_GREATER:
            return atomic_instruction(vm, "BINARY_GREATER_GREATER", ip);

        case PYRO_OPCODE_SET_FIELD:
            return constant_instruction(vm, "SET_FIELD", fn, ip);

        case PYRO_OPCODE_SET_PUB_FIELD:
            return constant_instruction(vm, "SET_PUB_FIELD", fn, ip);

        case PYRO_OPCODE_SET_GLOBAL:
            return constant_instruction(vm, "SET_GLOBAL", fn, ip);

        case PYRO_OPCODE_SET_INDEX:
            return atomic_instruction(vm, "SET_INDEX", ip);

        case PYRO_OPCODE_SET_LOCAL:
            return u8_instruction(vm, "SET_LOCAL", fn, ip);

        case PYRO_OPCODE_SET_UPVALUE:
            return u8_instruction(vm, "SET_UPVALUE", fn, ip);

        case PYRO_OPCODE_BINARY_MINUS:
            return atomic_instruction(vm, "BINARY_MINUS", ip);

        case PYRO_OPCODE_BINARY_SLASH_SLASH:
            return atomic_instruction(vm, "BINARY_SLASH_SLASH", ip);

        case PYRO_OPCODE_TRY:
            return atomic_instruction(vm, "TRY", ip);

        case PYRO_OPCODE_UNPACK:
            return u8_instruction(vm, "UNPACK", fn, ip);

        case PYRO_OPCODE_START_WITH:
            return atomic_instruction(vm, "START_WITH", ip);

        case PYRO_OPCODE_END_WITH:
            return atomic_instruction(vm, "END_WITH", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_0:
            return atomic_instruction(vm, "LOAD_CONSTANT_0", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_1:
            return atomic_instruction(vm, "LOAD_CONSTANT_1", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_2:
            return atomic_instruction(vm, "LOAD_CONSTANT_2", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_3:
            return atomic_instruction(vm, "LOAD_CONSTANT_3", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_4:
            return atomic_instruction(vm, "LOAD_CONSTANT_4", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_5:
            return atomic_instruction(vm, "LOAD_CONSTANT_5", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_6:
            return atomic_instruction(vm, "LOAD_CONSTANT_6", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_7:
            return atomic_instruction(vm, "LOAD_CONSTANT_7", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_8:
            return atomic_instruction(vm, "LOAD_CONSTANT_8", ip);

        case PYRO_OPCODE_LOAD_CONSTANT_9:
            return atomic_instruction(vm, "LOAD_CONSTANT_9", ip);

        case PYRO_OPCODE_CALL_VALUE_0:
            return atomic_instruction(vm, "CALL_VALUE_0", ip);

        case PYRO_OPCODE_CALL_VALUE_1:
            return atomic_instruction(vm, "CALL_VALUE_1", ip);

        case PYRO_OPCODE_CALL_VALUE_2:
            return atomic_instruction(vm, "CALL_VALUE_2", ip);

        case PYRO_OPCODE_CALL_VALUE_3:
            return atomic_instruction(vm, "CALL_VALUE_3", ip);

        case PYRO_OPCODE_CALL_VALUE_4:
            return atomic_instruction(vm, "CALL_VALUE_4", ip);

        case PYRO_OPCODE_CALL_VALUE_5:
            return atomic_instruction(vm, "CALL_VALUE_5", ip);

        case PYRO_OPCODE_CALL_VALUE_6:
            return atomic_instruction(vm, "CALL_VALUE_6", ip);

        case PYRO_OPCODE_CALL_VALUE_7:
            return atomic_instruction(vm, "CALL_VALUE_7", ip);

        case PYRO_OPCODE_CALL_VALUE_8:
            return atomic_instruction(vm, "CALL_VALUE_8", ip);

        case PYRO_OPCODE_CALL_VALUE_9:
            return atomic_instruction(vm, "CALL_VALUE_9", ip);

        case PYRO_OPCODE_CONCAT_STRINGS:
            return u16_instruction(vm, "CONCAT_STRINGS", fn, ip);

        case PYRO_OPCODE_I64_ADD:
            return atomic_instruction(vm, "I64_ADD", ip);

        case PYRO_OPCODE_MAKE_ENUM:
            return u16_x2_instruction(vm, "MAKE_ENUM", fn, ip);

        case PYRO_OPCODE_IS_ERR:
            return atomic_instruction(vm, "IS_ERR", ip);

        case PYRO_OPCODE_IS_STR:
            return atomic_instruction(vm, "IS_STR", ip);

        case PYRO_OPCODE_IS_RUNE:
            return atomic_instruction(vm, "IS_RUNE", ip);

        case PYRO_OPCODE_IS_I64:
            return atomic_instruction(vm, "IS_I64", ip);

        case PYRO_OPCODE_IS_F64:
            return atomic_instruction(vm, "IS_F64", ip);

        case PYRO_OPCODE_MAKE_STR:
            return atomic_instruction(vm, "MAKE_STR", ip);

        case PYRO_OPCODE_CALL_COUNT:
            return atomic_instruction(vm, "CALL_COUNT", ip);

        default:
            pyro_stdout_write_f(vm, "INVALID OPCODE [%d]\n", instruction);
            return ip + 1;
    }
}


void pyro_disassemble_function(PyroVM* vm, PyroFn* fn, const char* src_id) {
    char* fn_name = fn->name ? fn->name->bytes : "<fn>";

    pyro_stdout_write_f(vm, "\x1B[1;32msource id\x1B[0m %s\n", src_id);
    pyro_stdout_write_f(vm, "\x1B[1;32m function\x1B[0m %s\n\n", fn_name);

    pyro_stdout_write(vm, "\x1B[1;32mconstant table\x1B[0m\n");
    for (size_t i = 0; i < fn->constants_count; i++) {
        pyro_stdout_write_f(vm, "%04d    ", i);
        pyro_dump_value(vm, fn->constants[i]);
        pyro_stdout_write_f(vm, "\n");
    }

    pyro_stdout_write(vm, "\n\x1B[1;32mbytecode\x1B[0m\n");
    for (size_t ip = 0; ip < fn->code_count;) {
        ip = pyro_disassemble_instruction(vm, fn, ip);
    }

    pyro_stdout_write_f(vm, "\n");
}
