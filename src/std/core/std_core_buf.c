#include "../std_lib.h"

#include "../../vm/vm.h"
#include "../../vm/utils.h"
#include "../../vm/setup.h"
#include "../../vm/stringify.h"
#include "../../vm/panics.h"


static Value fn_buf(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjBuf* buf = ObjBuf_new(vm);
        if (!buf) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_OBJ(buf);
    }

    else if (arg_count == 1) {
        if (IS_STR(args[0])) {
            ObjBuf* buf = ObjBuf_new_from_string(AS_STR(args[0]), vm);
            if (!buf) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return MAKE_NULL();
            }
            return MAKE_OBJ(buf);
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid content argument for $buf().");
            return MAKE_NULL();
        }
    }

    else if (arg_count == 2) {
        if (IS_I64(args[0]) && args[0].as.i64 >= 0) {
            uint8_t fill_value;
            if (IS_I64(args[1]) && args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
                fill_value = (uint8_t)args[1].as.i64;
            } else if (IS_CHAR(args[1]) && args[1].as.u32 <= 255) {
                fill_value = (uint8_t)args[1].as.u32;
            } else {
                pyro_panic(vm, ERR_TYPE_ERROR, "Invalid fill argument for $buf().");
                return MAKE_NULL();
            }
            ObjBuf* buf = ObjBuf_new_with_cap_and_fill((size_t)args[0].as.i64, fill_value, vm);
            if (!buf) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return MAKE_NULL();
            }
            return MAKE_OBJ(buf);
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid size argument for $buf().");
            return MAKE_NULL();
        }
    }

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 2 arguments for $buf(), found %d.", arg_count);
    return MAKE_NULL();
}


static Value fn_is_buf(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_BUF(args[0]));
}


static Value buf_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    return MAKE_I64(buf->count);
}


static Value buf_write_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_byte().");
        return MAKE_NULL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > 255) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_byte().", value);
        return MAKE_NULL();
    }

    if (!ObjBuf_append_byte(buf, value, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }

    return MAKE_NULL();
}


static Value buf_write_be_u16(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16().");
        return MAKE_NULL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value);
        return MAKE_NULL();
    }

    uint8_t byte1 = (value >> 8) & 0xFF;    // MSB
    uint8_t byte2 = value & 0xFF;           // LSB

    if (!ObjBuf_append_byte(buf, byte1, vm)) {
        return MAKE_BOOL(false);
    }

    if (!ObjBuf_append_byte(buf, byte2, vm)) {
        buf->count--;
        return MAKE_BOOL(false);
    }

    return MAKE_BOOL(true);
}


static Value buf_write_le_u16(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16().");
        return MAKE_NULL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value);
        return MAKE_NULL();
    }

    uint8_t byte1 = (value >> 8) & 0xFF;    // MSB
    uint8_t byte2 = value & 0xFF;           // LSB

    if (!ObjBuf_append_byte(buf, byte2, vm)) {
        return MAKE_BOOL(false);
    }

    if (!ObjBuf_append_byte(buf, byte1, vm)) {
        buf->count--;
        return MAKE_BOOL(false);
    }

    return MAKE_BOOL(true);
}


static Value buf_to_str(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    ObjStr* string = ObjBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    return MAKE_OBJ(string);
}


static Value buf_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < buf->count) {
            return MAKE_I64(buf->bytes[index]);
        }
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
    return MAKE_NULL();
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
                return MAKE_NULL();
            }
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid byte value type, expected an integer.");
            return MAKE_NULL();
        }
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
    return MAKE_NULL();
}


static Value buf_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or more arguments for :write(), found 0.");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        if (IS_BUF(args[0])) {
            ObjBuf* src_buf = AS_BUF(args[0]);
            if (!ObjBuf_append_bytes(buf, src_buf->count, src_buf->bytes, vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return MAKE_NULL();
            }
            return MAKE_I64((int64_t)src_buf->count);
        } else {
            ObjStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return MAKE_NULL();
            }
            pyro_push(vm, MAKE_OBJ(string));
            if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return MAKE_NULL();
            }
            pyro_pop(vm);
            return MAKE_I64((int64_t)string->length);
        }
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to :write() should be a format string.");
        return MAKE_NULL();
    }

    Value formatted = pyro_fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }
    ObjStr* string = AS_STR(formatted);

    pyro_push(vm, formatted);
    if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    pyro_pop(vm);

    return MAKE_I64((int64_t)string->length);
}


static Value buf_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    return MAKE_BOOL(buf->count == 0);
}


void pyro_load_std_core_buf(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$buf", fn_buf, -1);
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
    pyro_define_method(vm, vm->buf_class, "is_empty", buf_is_empty, 0);
}