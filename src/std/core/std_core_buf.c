#include "../../inc/std_lib.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/setup.h"
#include "../../inc/stringify.h"
#include "../../inc/panics.h"


static Value fn_buf(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjBuf* buf = ObjBuf_new(vm);
        if (!buf) {
            pyro_panic(vm, "$buf(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(buf);
    }

    else if (arg_count == 1) {
        if (IS_STR(args[0])) {
            ObjBuf* buf = ObjBuf_new_from_string(AS_STR(args[0]), vm);
            if (!buf) {
                pyro_panic(vm, "$buf(): out of memory");
                return MAKE_NULL();
            }
            return MAKE_OBJ(buf);
        } else {
            pyro_panic(vm, "$buf(): invalid argument [content], expected a string");
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
                pyro_panic(vm, "$buf(): invalid argument [fill_value], argument is out of range");
                return MAKE_NULL();
            }
            ObjBuf* buf = ObjBuf_new_with_cap_and_fill((size_t)args[0].as.i64, fill_value, vm);
            if (!buf) {
                pyro_panic(vm, "$buf(): out of memory");
                return MAKE_NULL();
            }
            return MAKE_OBJ(buf);
        } else {
            pyro_panic(vm, "$buf(): invalid argument [size], expected a positive integer");
            return MAKE_NULL();
        }
    }

    pyro_panic(vm, "$buf(): expected 0 or 2 arguments, found %zu", arg_count);
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
    uint8_t byte_value;

    if (IS_I64(args[0])) {
        if (args[0].as.i64 >= 0 && args[0].as.i64 <= 255) {
            byte_value = (uint8_t)args[0].as.i64;
        } else {
            pyro_panic(vm, "write_byte(): invalid argument [byte], integer (%d) is out of range", args[0].as.i64);
            return MAKE_NULL();
        }
    } else if (IS_CHAR(args[0])) {
        if (args[0].as.u32 <= 255) {
            byte_value = (uint8_t)args[0].as.u32;
        } else {
            pyro_panic(vm, "write_byte(): invalid argument [byte], char (%d) is out of range", args[0].as.u32);
            return MAKE_NULL();
        }
    } else {
        pyro_panic(vm, "write_byte(): invalid argument [byte]");
        return MAKE_NULL();
    }

    if (!ObjBuf_append_byte(buf, byte_value, vm)) {
        pyro_panic(vm, "write_byte(): out of memory");
    }

    return MAKE_NULL();
}


/* static Value buf_write_be_u16(PyroVM* vm, size_t arg_count, Value* args) { */
/*     ObjBuf* buf = AS_BUF(args[-1]); */

/*     if (!IS_I64(args[0])) { */
/*         pyro_panic(vm, "$write_be_u16(): invalid argument"); */
/*         return MAKE_NULL(); */
/*     } */

/*     int64_t value = args[0].as.i64; */
/*     if (value < 0 || value > UINT16_MAX) { */
/*         pyro_panic(vm, "$write_be_u16(): invalid argument, (%d) is out of range", value); */
/*         return MAKE_NULL(); */
/*     } */

/*     uint8_t byte1 = (value >> 8) & 0xFF;    // MSB */
/*     uint8_t byte2 = value & 0xFF;           // LSB */

/*     if (!ObjBuf_append_byte(buf, byte1, vm)) { */
/*         return MAKE_BOOL(false); */
/*     } */

/*     if (!ObjBuf_append_byte(buf, byte2, vm)) { */
/*         buf->count--; */
/*         return MAKE_BOOL(false); */
/*     } */

/*     return MAKE_BOOL(true); */
/* } */


/* static Value buf_write_le_u16(PyroVM* vm, size_t arg_count, Value* args) { */
/*     ObjBuf* buf = AS_BUF(args[-1]); */

/*     if (!IS_I64(args[0])) { */
/*         pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16()."); */
/*         return MAKE_NULL(); */
/*     } */

/*     int64_t value = args[0].as.i64; */
/*     if (value < 0 || value > UINT16_MAX) { */
/*         pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value); */
/*         return MAKE_NULL(); */
/*     } */

/*     uint8_t byte1 = (value >> 8) & 0xFF;    // MSB */
/*     uint8_t byte2 = value & 0xFF;           // LSB */

/*     if (!ObjBuf_append_byte(buf, byte2, vm)) { */
/*         return MAKE_BOOL(false); */
/*     } */

/*     if (!ObjBuf_append_byte(buf, byte1, vm)) { */
/*         buf->count--; */
/*         return MAKE_BOOL(false); */
/*     } */

/*     return MAKE_BOOL(true); */
/* } */


static Value buf_to_str(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);
    ObjStr* string = ObjBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "to_str(): out of memory");
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
        pyro_panic(vm, "get(): invalid argument [index], out of range");
        return MAKE_NULL();
    }
    pyro_panic(vm, "get(): invalid argument [index], expected an integer");
    return MAKE_NULL();
}


static Value buf_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "set(): invalid argument [index], expected an integer");
        return MAKE_NULL();
    }

    int64_t index = args[0].as.i64;
    if (index < 0 || (size_t)index >= buf->count) {
        pyro_panic(vm, "set(): invalid argument [index], integer is out of range");
        return MAKE_NULL();
    }

    uint8_t byte_value;
    if (IS_I64(args[1])) {
        if (args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
            byte_value = (uint8_t)args[1].as.i64;
        } else {
            pyro_panic(vm, "set(): invalid argument [byte], integer (%d) is out of range", args[1].as.i64);
            return MAKE_NULL();
        }
    } else if (IS_CHAR(args[1])) {
        if (args[1].as.u32 <= 255) {
            byte_value = (uint8_t)args[1].as.u32;
        } else {
            pyro_panic(vm, "set(): invalid argument [byte], char (%d) is out of range", args[1].as.u32);
            return MAKE_NULL();
        }
    } else {
        pyro_panic(vm, "set(): invalid argument [byte], expected an integer");
        return MAKE_NULL();
    }

    buf->bytes[index] = byte_value;
    return args[1];
}


static Value buf_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, "write(): expected 1 or more arguments, found 0");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        if (IS_BUF(args[0])) {
            ObjBuf* src_buf = AS_BUF(args[0]);
            if (!ObjBuf_append_bytes(buf, src_buf->count, src_buf->bytes, vm)) {
                pyro_panic(vm, "write(): out of memory");
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
                pyro_panic(vm, "write(): out of memory");
                return MAKE_NULL();
            }
            pyro_pop(vm);
            return MAKE_I64((int64_t)string->length);
        }
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "write(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = pyro_fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }
    ObjStr* string = AS_STR(formatted);

    pyro_push(vm, formatted);
    if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
        pyro_panic(vm, "write(): out of memory");
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
    pyro_define_method(vm, vm->class_buf, "to_str", buf_to_str, 0);
    pyro_define_method(vm, vm->class_buf, "count", buf_count, 0);
    pyro_define_method(vm, vm->class_buf, "get", buf_get, 1);
    pyro_define_method(vm, vm->class_buf, "set", buf_set, 2);
    pyro_define_method(vm, vm->class_buf, "$get_index", buf_get, 1);
    pyro_define_method(vm, vm->class_buf, "$set_index", buf_set, 2);
    pyro_define_method(vm, vm->class_buf, "write_byte", buf_write_byte, 1);
    pyro_define_method(vm, vm->class_buf, "write", buf_write, -1);
    pyro_define_method(vm, vm->class_buf, "is_empty", buf_is_empty, 0);
}
