#include "debug.h"
#include "vm.h"
#include "opcodes.h"


// A single-byte instruction with no arguments.
static size_t atomic_instruction(PyroVM* vm, const char* name, size_t ip) {
    pyro_out(vm, "%s\n", name);
    return ip + 1;
}


// An instruction with a two-byte argument which indexes into the constants table.
static size_t constant_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_out(vm, "%-24s %4d    ", name, index);
    pyro_dump_value(vm, fn->constants[index]);
    pyro_out(vm, "\n");
    return ip + 3;
}


// An instruction with a one-byte argument representing a uint8_t.
static size_t u8_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint8_t arg = fn->code[ip + 1];
    pyro_out(vm, "%-24s %4d\n", name, arg);
    return ip + 2;
}


// An instruction with a two-byte argument representing a uint16_t in big-endian format.
static size_t u16_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t arg = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_out(vm, "%-24s %4d\n", name, arg);
    return ip + 3;
}


// A jump instruction with a 2-byte argument.
static size_t jump_instruction(PyroVM* vm, const char* name, int sign, ObjFn* fn, size_t ip) {
    uint16_t offset = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    pyro_out(vm, "%-24s %4d -> %d\n", name, ip, ip + 3 + sign * offset);
    return ip + 3;
}


// A method-invoking instruction.
static size_t invoke_instruction(PyroVM* vm, const char* name, ObjFn* fn, size_t ip) {
    uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
    uint8_t arg_count = fn->code[ip + 3];
    pyro_out(vm, "%-24s %4d    ", name, const_index);
    pyro_dump_value(vm, fn->constants[const_index]);
    pyro_out(vm, "    (%d args)\n", arg_count);
    return ip + 4;
}


// Returns the index of the next instruction, if there is one.
size_t pyro_disassemble_instruction(PyroVM* vm, ObjFn* fn, size_t ip) {
    pyro_out(vm, "%04d ", ip);
    if (ip > 0 && ObjFn_get_line_number(fn, ip) == ObjFn_get_line_number(fn, ip - 1)) {
        pyro_out(vm, "   |    ");
    } else {
        pyro_out(vm, "%4zu    ", ObjFn_get_line_number(fn, ip));
    }

    uint8_t instruction = fn->code[ip];

    switch (instruction) {
        case OP_ASSERT:
            return atomic_instruction(vm, "OP_ASSERT", ip);
        case OP_ADD:
            return atomic_instruction(vm, "OP_ADD", ip);
        case OP_BITWISE_AND:
            return atomic_instruction(vm, "OP_BITWISE_AND", ip);
        case OP_BITWISE_NOT:
            return atomic_instruction(vm, "OP_BITWISE_NOT", ip);
        case OP_BITWISE_OR:
            return atomic_instruction(vm, "OP_BITWISE_OR", ip);
        case OP_BITWISE_XOR:
            return atomic_instruction(vm, "OP_BITWISE_XOR", ip);
        case OP_CALL:
            return u8_instruction(vm, "OP_CALL", fn, ip);
        case OP_CLASS:
            return constant_instruction(vm, "OP_CLASS", fn, ip);
        case OP_CLOSE_UPVALUE:
            return atomic_instruction(vm, "OP_CLOSE_UPVALUE", ip);
        case OP_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ip += 3;

            pyro_out(vm, "%-24s %4d    ", "OP_CLOSURE", const_index);
            pyro_dump_value(vm, fn->constants[const_index]);
            pyro_out(vm, "\n");

            ObjFn* wrapped_fn = AS_FN(fn->constants[const_index]);
            for (size_t i = 0; i < wrapped_fn->upvalue_count; i++) {
                int is_local = wrapped_fn->code[ip++];
                int index = wrapped_fn->code[ip++];
                pyro_out(vm, "%04d    |      <%s %d>\n", ip - 2, is_local ? "local" : "upvalue", index);
            }

            return ip;
        }
        case OP_DEFINE_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            ip += 2;

            pyro_out(vm, "%-24s %4d\n", "OP_DEFINE_GLOBALS", count);

            for (uint8_t i = 0; i < count; i++) {
                uint16_t index = (fn->code[ip] << 8) | fn->code[ip + 1];
                pyro_out(vm, "%04d    |    %4d    ", ip, index);
                pyro_dump_value(vm, fn->constants[index]);
                pyro_out(vm, "\n");
                ip += 2;
            }

            return ip;
        }
        case OP_DEFINE_GLOBAL:
            return constant_instruction(vm, "OP_DEFINE_GLOBAL", fn, ip);
        case OP_DUP:
            return atomic_instruction(vm, "OP_DUP", ip);
        case OP_DUP2:
            return atomic_instruction(vm, "OP_DUP2", ip);
        case OP_ECHO:
            return u8_instruction(vm, "OP_ECHO", fn, ip);
        case OP_EQUAL:
            return atomic_instruction(vm, "OP_EQUAL", ip);
        case OP_FIELD:
            return constant_instruction(vm, "OP_FIELD", fn, ip);
        case OP_FLOAT_DIV:
            return atomic_instruction(vm, "OP_FLOAT_DIV", ip);
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
        case OP_GREATER:
            return atomic_instruction(vm, "OP_GREATER", ip);
        case OP_GREATER_EQUAL:
            return atomic_instruction(vm, "OP_GREATER_EQUAL", ip);
        case OP_IMPORT:
            return u8_instruction(vm, "OP_IMPORT", fn, ip);
        case OP_INHERIT:
            return atomic_instruction(vm, "OP_INHERIT", ip);
        case OP_INVOKE_METHOD:
            return invoke_instruction(vm, "OP_INVOKE_METHOD", fn, ip);
        case OP_INVOKE_SUPER_METHOD:
            return invoke_instruction(vm, "OP_INVOKE_SUPER_METHOD", fn, ip);
        case OP_ITER:
            return atomic_instruction(vm, "OP_ITER", ip);
        case OP_ITER_NEXT:
            return atomic_instruction(vm, "OP_ITER_NEXT", ip);
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
        case OP_LESS:
            return atomic_instruction(vm, "OP_LESS", ip);
        case OP_LESS_EQUAL:
            return atomic_instruction(vm, "OP_LESS_EQUAL", ip);
        case OP_LOAD_CONSTANT:
            return constant_instruction(vm, "OP_LOAD_CONSTANT", fn, ip);
        case OP_LOAD_FALSE:
            return atomic_instruction(vm, "OP_LOAD_FALSE", ip);
        case OP_LOAD_NULL:
            return atomic_instruction(vm, "OP_LOAD_NULL", ip);
        case OP_LOAD_TRUE:
            return atomic_instruction(vm, "OP_LOAD_TRUE", ip);
        case OP_LOOP:
            return jump_instruction(vm, "OP_LOOP", -1, fn, ip);
        case OP_LSHIFT:
            return atomic_instruction(vm, "OP_LSHIFT", ip);
        case OP_MAKE_MAP:
            return u16_instruction(vm, "OP_MAKE_MAP", fn, ip);
        case OP_MAKE_VEC:
            return u16_instruction(vm, "OP_MAKE_VEC", fn, ip);
        case OP_METHOD:
            return constant_instruction(vm, "OP_METHOD", fn, ip);
        case OP_MODULO:
            return atomic_instruction(vm, "OP_MOD", ip);
        case OP_MULTIPLY:
            return atomic_instruction(vm, "OP_MULTIPLY", ip);
        case OP_NEGATE:
            return atomic_instruction(vm, "OP_NEGATE", ip);
        case OP_LOGICAL_NOT:
            return atomic_instruction(vm, "OP_LOGICAL_NOT", ip);
        case OP_NOT_EQUAL:
            return atomic_instruction(vm, "OP_NOT_EQUAL", ip);
        case OP_POP:
            return atomic_instruction(vm, "OP_POP", ip);
        case OP_POP_JUMP_IF_FALSE:
            return jump_instruction(vm, "OP_POP_JUMP_IF_FALSE", 1, fn, ip);
        case OP_POWER:
            return atomic_instruction(vm, "OP_POWER", ip);
        case OP_RETURN:
            return atomic_instruction(vm, "OP_RETURN", ip);
        case OP_RSHIFT:
            return atomic_instruction(vm, "OP_RSHIFT", ip);
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
        case OP_SUBTRACT:
            return atomic_instruction(vm, "OP_SUBTRACT", ip);
        case OP_TRUNC_DIV:
            return atomic_instruction(vm, "OP_TRUNC_DIV", ip);
        case OP_TRY:
            return atomic_instruction(vm, "OP_TRY", ip);
        case OP_UNPACK:
            return u8_instruction(vm, "OP_UNPACK", fn, ip);
        default:
            pyro_out(vm, "INVALID OPCODE [%d]\n", instruction);
            return ip + 1;
    }
}


void pyro_disassemble_function(PyroVM* vm, ObjFn* fn) {
    pyro_out(vm, "== %s constant table ==\n", fn->name == NULL ? "<fn>" : fn->name->bytes);
    for (size_t i = 0; i < fn->constants_count; i++) {
        pyro_out(vm, "%04d    ", i);
        pyro_dump_value(vm, fn->constants[i]);
        pyro_out(vm, "\n");
    }
    pyro_out(vm, "\n");

    pyro_out(vm, "== %s bytecode ==\n", fn->name == NULL ? "<fn>" : fn->name->bytes);
    for (size_t ip = 0; ip < fn->code_count;) {
        ip = pyro_disassemble_instruction(vm, fn, ip);
    }
    pyro_out(vm, "\n");
}
