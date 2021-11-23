#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/os.h"
#include "../vm/errors.h"


static Value fn_buf(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(buf);
}


static Value fn_is_buf(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_BUF(args[0]));
}


static Value buf_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    return I64_VAL(buf->count);
}


static Value buf_write_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_byte().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > 255) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_byte().", value);
        return NULL_VAL();
    }

    if (!ObjBuf_append_byte(buf, value, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }

    return NULL_VAL();
}


static Value buf_write_be_u16(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value);
        return NULL_VAL();
    }

    uint8_t byte1 = (value >> 8) & 0xFF;    // MSB
    uint8_t byte2 = value & 0xFF;           // LSB

    if (!ObjBuf_append_byte(buf, byte1, vm)) {
        return BOOL_VAL(false);
    }

    if (!ObjBuf_append_byte(buf, byte2, vm)) {
        buf->count--;
        return BOOL_VAL(false);
    }

    return BOOL_VAL(true);
}


static Value buf_write_le_u16(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value);
        return NULL_VAL();
    }

    uint8_t byte1 = (value >> 8) & 0xFF;    // MSB
    uint8_t byte2 = value & 0xFF;           // LSB

    if (!ObjBuf_append_byte(buf, byte2, vm)) {
        return BOOL_VAL(false);
    }

    if (!ObjBuf_append_byte(buf, byte1, vm)) {
        buf->count--;
        return BOOL_VAL(false);
    }

    return BOOL_VAL(true);
}


static Value buf_to_str(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    ObjStr* string = ObjBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(string);
}


static Value buf_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < buf->count) {
            return I64_VAL(buf->bytes[index]);
        }
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value buf_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < buf->count) {
            if (IS_I64(args[1])) {
                int64_t value = args[1].as.i64;
                if (value >= 0 && value <= 255) {
                    buf->bytes[index] = value;
                    return args[1];
                }
                pyro_panic(vm, ERR_VALUE_ERROR, "Byte value out of range.");
                return NULL_VAL();
            }
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid byte value type, expected an integer.");
            return NULL_VAL();
        }
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value buf_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or more arguments for :write(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (!string) {
            return BOOL_VAL(false);
        }
        pyro_push(vm, OBJ_VAL(string));
        if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        }
        pyro_pop(vm);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to :write() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = pyro_fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    ObjStr* string = AS_STR(formatted);

    pyro_push(vm, formatted);
    if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    pyro_pop(vm);

    return NULL_VAL();
}


void pyro_load_std_core_buf(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$buf", fn_buf, 0);
    pyro_define_global_fn(vm, "$is_buf", fn_is_buf, 1);

    // Methods.
    pyro_define_method(vm, vm->buf_class, "to_str", buf_to_str, 0);
    pyro_define_method(vm, vm->buf_class, "count", buf_count, 0);
    pyro_define_method(vm, vm->buf_class, "get", buf_get, 1);
    pyro_define_method(vm, vm->buf_class, "set", buf_set, 2);
    pyro_define_method(vm, vm->buf_class, "$get_index", buf_get, 1);
    pyro_define_method(vm, vm->buf_class, "$set_index", buf_set, 2);
    pyro_define_method(vm, vm->buf_class, "write_byte", buf_write_byte, 1);
    pyro_define_method(vm, vm->buf_class, "write_be_u16", buf_write_be_u16, 1);
    pyro_define_method(vm, vm->buf_class, "write_le_u16", buf_write_le_u16, 1);
    pyro_define_method(vm, vm->buf_class, "write", buf_write, -1);
}
