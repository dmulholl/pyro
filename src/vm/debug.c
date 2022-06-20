#include "debug.h"
#include "opcodes.h"
#include "utils.h"
#include "io.h"


// A single-byte instruction with no arguments.
static size_t atomic_instruction(PyroVM* vm, const char* name, size_t ip) {
    pyro_stdout_write_f(vm, "%s\n", name);
    return ip + 1;
}


// An instruction with a two-byte argument which indexes into the constants table.
static size_t constant_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-24s %4d    ", name, index);
    pyro_dump_value(vm, fn->constants[index]);
    pyro_stdout_write_f(vm, "\n");
    return ip + 3;
}


// An instruction with a one-byte argument representing a uint8_t.
static size_t u8_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint8_t arg = fn->code[ip + 1];
    pyro_stdout_write_f(vm, "%-24s %4d\n", name, arg);
    return ip + 2;
}


// An instruction with two one-byte arguments each representing a uint8_t.
static size_t u8_x2_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint8_t arg1 = fn->code[ip + 1];
    uint8_t arg2 = fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-24s %4d %4d\n", name, arg1, arg2);
    return ip + 3;
}


// An instruction with a two-byte argument representing a uint16_t in big-endian format.
static size_t u16_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t arg = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-24s %4d\n", name, arg);
    return ip + 3;
}


// A jump instruction with a 2-byte argument.
static size_t jump_instruction(PyroVM* vm, const char* name, int sign, ObjFn* fn, size_t ip) {
    uint16_t offset = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_stdout_write_f(vm, "%-24s %4d -> %d\n", name, ip, ip + 3 + sign * offset);
    return ip + 3;
}


// A method-invoking instruction.
static size_t invoke_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    uint8_t arg_count = fn->code[ip + 3];
    pyro_stdout_write_f(vm, "%-24s %4d    ", name, const_index);
    pyro_dump_value(vm, fn->constants[const_index]);
    pyro_stdout_write_f(vm, "    (%d args)\n", arg_count);
    return ip + 4;
}


// Returns the index of the next instruction, if there is one.
size_t pyro_disassemble_instruction(PyroVM* vm, ObjFn* fn, size_t ip) {
    pyro_stdout_write_f(vm, "%04d ", ip);
    if (ip > 0 && ObjFn_get_line_number(fn, ip) == ObjFn_get_line_number(fn, ip - 1)) {
        pyro_stdout_write_f(vm, "   |    ");
    } else {
        pyro_stdout_write_f(vm, "%4zu    ", ObjFn_get_line_number(fn, ip));
    }

    uint8_t instruction = fn->code[ip];

    switch (instruction) {
        case OP_ASSERT:
            return atomic_instruction(vm, "OP_ASSERT", ip);
        case OP_BINARY_PLUS:
            return atomic_instruction(vm, "OP_BINARY_PLUS", ip);
        case OP_BINARY_AMP:
            return atomic_instruction(vm, "OP_BINARY_AMP", ip);
        case OP_UNARY_TILDE:
            return atomic_instruction(vm, "OP_UNARY_TILDE", ip);
        case OP_BINARY_BAR:
            return atomic_instruction(vm, "OP_BINARY_BAR", ip);
        case OP_BINARY_IN:
            return atomic_instruction(vm, "OP_BINARY_IN", ip);
        case OP_BINARY_CARET:
            return atomic_instruction(vm, "OP_BINARY_CARET", ip);
        case OP_CALL:
            return u8_instruction(vm, "OP_CALL", fn, ip);
        case OP_CALL_UNPACK_LAST_ARG:
            return u8_instruction(vm, "OP_CALL_UNPACK_LAST_ARG", fn, ip);
        case OP_MAKE_CLASS:
            return constant_instruction(vm, "OP_MAKE_CLASS", fn, ip);
        case OP_CLOSE_UPVALUE:
            return atomic_instruction(vm, "OP_CLOSE_UPVALUE", ip);
        case OP_MAKE_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ip += 3;

            pyro_stdout_write_f(vm, "%-24s %4d    ", "OP_MAKE_CLOSURE", const_index);
            pyro_dump_value(vm, fn->constants[const_index]);
            pyro_stdout_write_f(vm, "\n");

            ObjFn* wrapped_fn = AS_FN(fn->constants[const_index]);
            for (size_t i = 0; i < wrapped_fn->upvalue_count; i++) {
                int is_local = wrapped_fn->code[ip++];
                int index = wrapped_fn->code[ip++];
                pyro_stdout_write_f(vm, "%04d    |      <%s %d>\n", ip - 2, is_local ? "local" : "upvalue", index);
            }

            return ip;
        }
        case OP_DEFINE_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            ip += 2;

            pyro_stdout_write_f(vm, "%-24s %4d\n", "OP_DEFINE_GLOBALS", count);

            for (uint8_t i = 0; i < count; i++) {
                uint16_t index = (fn->code[ip] << 8) | fn->code[ip + 1];
                pyro_stdout_write_f(vm, "%04d    |    %4d    ", ip, index);
                pyro_dump_value(vm, fn->constants[index]);
                pyro_stdout_write_f(vm, "\n");
                ip += 2;
            }

            return ip;
        }
        case OP_DEFINE_GLOBAL:
            return constant_instruction(vm, "OP_DEFINE_GLOBAL", fn, ip);
        case OP_DUP:
            return atomic_instruction(vm, "OP_DUP", ip);
        case OP_DUP_2:
            return atomic_instruction(vm, "OP_DUP_2", ip);
        case OP_ECHO:
            return u8_instruction(vm, "OP_ECHO", fn, ip);
        case OP_BINARY_EQUAL_EQUAL:
            return atomic_instruction(vm, "OP_BINARY_EQUAL_EQUAL", ip);
        case OP_DEFINE_FIELD:
            return constant_instruction(vm, "OP_DEFINE_FIELD", fn, ip);
        case OP_BINARY_SLASH:
            return atomic_instruction(vm, "OP_BINARY_SLASH", ip);
        case OP_GET_FIELD:
            return constant_instruction(vm, "OP_GET_FIELD", fn, ip);
        case OP_GET_GLOBAL:
            return constant_instruction(vm, "OP_GET_GLOBAL", fn, ip);
        case OP_GET_INDEX:
            return atomic_instruction(vm, "OP_GET_INDEX", ip);
        case OP_GET_LOCAL:
            return u8_instruction(vm, "OP_GET_LOCAL", fn, ip);
        case OP_GET_MEMBER:
            return constant_instruction(vm, "OP_GET_MEMBER", fn, ip);
        case OP_GET_METHOD:
            return constant_instruction(vm, "OP_GET_METHOD", fn, ip);
        case OP_GET_SUPER_METHOD:
            return constant_instruction(vm, "OP_GET_SUPER_METHOD", fn, ip);
        case OP_GET_UPVALUE:
            return u8_instruction(vm, "OP_GET_UPVALUE", fn, ip);
        case OP_BINARY_GREATER:
            return atomic_instruction(vm, "OP_BINARY_GREATER", ip);
        case OP_BINARY_GREATER_EQUAL:
            return atomic_instruction(vm, "OP_BINARY_GREATER_EQUAL", ip);
        case OP_IMPORT_ALL_MEMBERS:
            return u8_instruction(vm, "OP_IMPORT_ALL_MEMBERS", fn, ip);
        case OP_IMPORT_MODULE:
            return u8_instruction(vm, "OP_IMPORT_MODULE", fn, ip);
        case OP_IMPORT_NAMED_MEMBERS:
            return u8_x2_instruction(vm, "OP_IMPORT_NAMED_MEMBERS", fn, ip);
        case OP_INHERIT:
            return atomic_instruction(vm, "OP_INHERIT", ip);
        case OP_CALL_METHOD:
            return invoke_instruction(vm, "OP_CALL_METHOD", fn, ip);
        case OP_CALL_METHOD_UNPACK_LAST_ARG:
            return invoke_instruction(vm, "OP_CALL_METHOD_UNPACK_LAST_ARG", fn, ip);
        case OP_CALL_SUPER_METHOD:
            return invoke_instruction(vm, "OP_CALL_SUPER_METHOD", fn, ip);
        case OP_CALL_SUPER_METHOD_UNPACK_LAST_ARG:
            return invoke_instruction(vm, "OP_CALL_SUPER_METHOD_UNPACK_LAST_ARG", fn, ip);
        case OP_GET_ITERATOR_OBJECT:
            return atomic_instruction(vm, "OP_GET_ITERATOR_OBJECT", ip);
        case OP_GET_ITERATOR_NEXT_VALUE:
            return atomic_instruction(vm, "OP_GET_ITERATOR_NEXT_VALUE", ip);
        case OP_JUMP:
            return jump_instruction(vm, "OP_JUMP", 1, fn, ip);
        case OP_JUMP_IF_ERR:
            return jump_instruction(vm, "OP_JUMP_IF_ERR", 1, fn, ip);
        case OP_JUMP_IF_FALSE:
            return jump_instruction(vm, "OP_JUMP_IF_FALSE", 1, fn, ip);
        case OP_JUMP_IF_NOT_ERR:
            return jump_instruction(vm, "OP_JUMP_IF_NOT_ERR", 1, fn, ip);
        case OP_JUMP_IF_NOT_NULL:
            return jump_instruction(vm, "OP_JUMP_IF_NOT_NULL", 1, fn, ip);
        case OP_JUMP_IF_TRUE:
            return jump_instruction(vm, "OP_JUMP_IF_TRUE", 1, fn, ip);
        case OP_BINARY_LESS:
            return atomic_instruction(vm, "OP_BINARY_LESS", ip);
        case OP_BINARY_LESS_EQUAL:
            return atomic_instruction(vm, "OP_BINARY_LESS_EQUAL", ip);
        case OP_LOAD_CONSTANT:
            return constant_instruction(vm, "OP_LOAD_CONSTANT", fn, ip);
        case OP_LOAD_FALSE:
            return atomic_instruction(vm, "OP_LOAD_FALSE", ip);
        case OP_LOAD_I64_0:
            return atomic_instruction(vm, "OP_LOAD_I64_0", ip);
        case OP_LOAD_I64_1:
            return atomic_instruction(vm, "OP_LOAD_I64_1", ip);
        case OP_LOAD_I64_2:
            return atomic_instruction(vm, "OP_LOAD_I64_2", ip);
        case OP_LOAD_I64_3:
            return atomic_instruction(vm, "OP_LOAD_I64_3", ip);
        case OP_LOAD_I64_4:
            return atomic_instruction(vm, "OP_LOAD_I64_4", ip);
        case OP_LOAD_I64_5:
            return atomic_instruction(vm, "OP_LOAD_I64_5", ip);
        case OP_LOAD_I64_6:
            return atomic_instruction(vm, "OP_LOAD_I64_6", ip);
        case OP_LOAD_I64_7:
            return atomic_instruction(vm, "OP_LOAD_I64_7", ip);
        case OP_LOAD_I64_8:
            return atomic_instruction(vm, "OP_LOAD_I64_8", ip);
        case OP_LOAD_I64_9:
            return atomic_instruction(vm, "OP_LOAD_I64_9", ip);
        case OP_LOAD_NULL:
            return atomic_instruction(vm, "OP_LOAD_NULL", ip);
        case OP_LOAD_TRUE:
            return atomic_instruction(vm, "OP_LOAD_TRUE", ip);
        case OP_JUMP_BACK:
            return jump_instruction(vm, "OP_JUMP_BACK", -1, fn, ip);
        case OP_BINARY_LESS_LESS:
            return atomic_instruction(vm, "OP_BINARY_LESS_LESS", ip);
        case OP_MAKE_MAP:
            return u16_instruction(vm, "OP_MAKE_MAP", fn, ip);
        case OP_MAKE_VEC:
            return u16_instruction(vm, "OP_MAKE_VEC", fn, ip);
        case OP_DEFINE_METHOD:
            return constant_instruction(vm, "OP_DEFINE_METHOD", fn, ip);
        case OP_BINARY_PERCENT:
            return atomic_instruction(vm, "OP_BINARY_PERCENT", ip);
        case OP_BINARY_STAR:
            return atomic_instruction(vm, "OP_BINARY_STAR", ip);
        case OP_UNARY_MINUS:
            return atomic_instruction(vm, "OP_UNARY_MINUS", ip);
        case OP_UNARY_PLUS:
            return atomic_instruction(vm, "OP_UNARY_PLUS", ip);
        case OP_UNARY_BANG:
            return atomic_instruction(vm, "OP_UNARY_BANG", ip);
        case OP_BINARY_BANG_EQUAL:
            return atomic_instruction(vm, "OP_BINARY_BANG_EQUAL", ip);
        case OP_POP:
            return atomic_instruction(vm, "OP_POP", ip);
        case OP_POP_ECHO_IN_REPL:
            return atomic_instruction(vm, "OP_POP_ECHO_IN_REPL", ip);
        case OP_POP_JUMP_IF_FALSE:
            return jump_instruction(vm, "OP_POP_JUMP_IF_FALSE", 1, fn, ip);
        case OP_BINARY_STAR_STAR:
            return atomic_instruction(vm, "OP_BINARY_STAR_STAR", ip);
        case OP_RETURN:
            return atomic_instruction(vm, "OP_RETURN", ip);
        case OP_BINARY_GREATER_GREATER:
            return atomic_instruction(vm, "OP_BINARY_GREATER_GREATER", ip);
        case OP_SET_FIELD:
            return constant_instruction(vm, "OP_SET_FIELD", fn, ip);
        case OP_SET_GLOBAL:
            return constant_instruction(vm, "OP_SET_GLOBAL", fn, ip);
        case OP_SET_INDEX:
            return atomic_instruction(vm, "OP_SET_INDEX", ip);
        case OP_SET_LOCAL:
            return u8_instruction(vm, "OP_SET_LOCAL", fn, ip);
        case OP_SET_UPVALUE:
            return u8_instruction(vm, "OP_SET_UPVALUE", fn, ip);
        case OP_BINARY_MINUS:
            return atomic_instruction(vm, "OP_BINARY_MINUS", ip);
        case OP_BINARY_SLASH_SLASH:
            return atomic_instruction(vm, "OP_BINARY_SLASH_SLASH", ip);
        case OP_TRY:
            return atomic_instruction(vm, "OP_TRY", ip);
        case OP_UNPACK:
            return u8_instruction(vm, "OP_UNPACK", fn, ip);
        default:
            pyro_stdout_write_f(vm, "INVALID OPCODE [%d]\n", instruction);
            return ip + 1;
    }
}


void pyro_disassemble_function(PyroVM* vm, ObjFn* fn) {
    pyro_stdout_write_f(vm, "\x1B[1;32mconstants\x1B[0m %s\n", fn->name == NULL ? "<fn>" : fn->name->bytes);

    for (size_t i = 0; i < fn->constants_count; i++) {
        pyro_stdout_write_f(vm, "%04d    ", i);
        pyro_dump_value(vm, fn->constants[i]);
        pyro_stdout_write_f(vm, "\n");
    }

    pyro_stdout_write_f(vm, "\n\x1B[1;32mbytecode\x1B[0m %s\n", fn->name == NULL ? "<fn>" : fn->name->bytes);
    for (size_t ip = 0; ip < fn->code_count;) {
        ip = pyro_disassemble_instruction(vm, fn, ip);
    }
    pyro_stdout_write_f(vm, "\n");
}
