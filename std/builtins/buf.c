#include "../../inc/pyro.h"


static PyroValue fn_buf(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        PyroBuf* buf = PyroBuf_new(vm);
        if (!buf) {
            pyro_panic(vm, "$buf(): out of memory");
            return pyro_null();
        }
        return pyro_obj(buf);
    }

    if (arg_count == 1) {
        if (PYRO_IS_STR(args[0])) {
            PyroBuf* buf = PyroBuf_new_from_string(PYRO_AS_STR(args[0]), vm);
            if (!buf) {
                pyro_panic(vm, "$buf(): out of memory");
                return pyro_null();
            }
            return pyro_obj(buf);
        }
        pyro_panic(vm, "$buf(): invalid argument [content], expected a string");
        return pyro_null();
    }

    if (arg_count == 2) {
        if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
            pyro_panic(vm, "$buf(): invalid argument [size], expected a positive integer");
            return pyro_null();
        }
        size_t size = args[0].as.i64;

        uint8_t fill_value;
        if (PYRO_IS_I64(args[1]) && args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
            fill_value = (uint8_t)args[1].as.i64;
        } else if (PYRO_IS_CHAR(args[1]) && args[1].as.u32 <= 255) {
            fill_value = (uint8_t)args[1].as.u32;
        } else {
            pyro_panic(vm, "$buf(): invalid argument [fill_value], expected an integer in the range [0,255]");
            return pyro_null();
        }

        PyroBuf* buf = PyroBuf_new_with_capacity(size + 1, vm);
        if (!buf) {
            pyro_panic(vm, "$buf(): out of memory");
            return pyro_null();
        }

        for (size_t i = 0; i < size; i++) {
            buf->bytes[i] = fill_value;
        }
        buf->count = size;

        return pyro_obj(buf);
    }

    pyro_panic(vm, "$buf(): expected 0 or 2 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_is_buf(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_BUF(args[0]));
}


static PyroValue buf_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    return pyro_i64(buf->count);
}


static PyroValue buf_write_byte(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    uint8_t byte_value;

    if (PYRO_IS_I64(args[0])) {
        if (args[0].as.i64 >= 0 && args[0].as.i64 <= 255) {
            byte_value = (uint8_t)args[0].as.i64;
        } else {
            pyro_panic(vm, "write_byte(): invalid argument [byte], integer (%d) is out of range", args[0].as.i64);
            return pyro_null();
        }
    } else if (PYRO_IS_CHAR(args[0])) {
        if (args[0].as.u32 <= 255) {
            byte_value = (uint8_t)args[0].as.u32;
        } else {
            pyro_panic(vm, "write_byte(): invalid argument [byte], char (%d) is out of range", args[0].as.u32);
            return pyro_null();
        }
    } else {
        pyro_panic(vm, "write_byte(): invalid argument [byte]");
        return pyro_null();
    }

    if (!PyroBuf_append_byte(buf, byte_value, vm)) {
        pyro_panic(vm, "write_byte(): out of memory");
    }

    return pyro_null();
}


/* static PyroValue buf_write_be_u16(PyroVM* vm, size_t arg_count, PyroValue* args) { */
/*     PyroBuf* buf = PYRO_AS_BUF(args[-1]); */

/*     if (!PYRO_IS_I64(args[0])) { */
/*         pyro_panic(vm, "$write_be_u16(): invalid argument"); */
/*         return pyro_null(); */
/*     } */

/*     int64_t value = args[0].as.i64; */
/*     if (value < 0 || value > UINT16_MAX) { */
/*         pyro_panic(vm, "$write_be_u16(): invalid argument, (%d) is out of range", value); */
/*         return pyro_null(); */
/*     } */

/*     uint8_t byte1 = (value >> 8) & 0xFF;    // MSB */
/*     uint8_t byte2 = value & 0xFF;           // LSB */

/*     if (!PyroBuf_append_byte(buf, byte1, vm)) { */
/*         return pyro_bool(false); */
/*     } */

/*     if (!PyroBuf_append_byte(buf, byte2, vm)) { */
/*         buf->count--; */
/*         return pyro_bool(false); */
/*     } */

/*     return pyro_bool(true); */
/* } */


/* static PyroValue buf_write_le_u16(PyroVM* vm, size_t arg_count, PyroValue* args) { */
/*     PyroBuf* buf = PYRO_AS_BUF(args[-1]); */

/*     if (!PYRO_IS_I64(args[0])) { */
/*         pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :write_be_u16()."); */
/*         return pyro_null(); */
/*     } */

/*     int64_t value = args[0].as.i64; */
/*     if (value < 0 || value > UINT16_MAX) { */
/*         pyro_panic(vm, ERR_VALUE_ERROR, "Out-of-range argument (%d) to :write_be_u16().", value); */
/*         return pyro_null(); */
/*     } */

/*     uint8_t byte1 = (value >> 8) & 0xFF;    // MSB */
/*     uint8_t byte2 = value & 0xFF;           // LSB */

/*     if (!PyroBuf_append_byte(buf, byte2, vm)) { */
/*         return pyro_bool(false); */
/*     } */

/*     if (!PyroBuf_append_byte(buf, byte1, vm)) { */
/*         buf->count--; */
/*         return pyro_bool(false); */
/*     } */

/*     return pyro_bool(true); */
/* } */


static PyroValue buf_to_str(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    PyroStr* string = PyroBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "to_str(): out of memory");
        return pyro_null();
    }
    return pyro_obj(string);
}


static PyroValue buf_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    if (PYRO_IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < buf->count) {
            return pyro_i64(buf->bytes[index]);
        }
        pyro_panic(vm, "get(): invalid argument [index], out of range");
        return pyro_null();
    }
    pyro_panic(vm, "get(): invalid argument [index], expected an integer");
    return pyro_null();
}


static PyroValue buf_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "set(): invalid argument [index], expected an integer");
        return pyro_null();
    }

    int64_t index = args[0].as.i64;
    if (index < 0 || (size_t)index >= buf->count) {
        pyro_panic(vm, "set(): invalid argument [index], integer is out of range");
        return pyro_null();
    }

    uint8_t byte_value;
    if (PYRO_IS_I64(args[1])) {
        if (args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
            byte_value = (uint8_t)args[1].as.i64;
        } else {
            pyro_panic(vm, "set(): invalid argument [byte], integer (%d) is out of range", args[1].as.i64);
            return pyro_null();
        }
    } else if (PYRO_IS_CHAR(args[1])) {
        if (args[1].as.u32 <= 255) {
            byte_value = (uint8_t)args[1].as.u32;
        } else {
            pyro_panic(vm, "set(): invalid argument [byte], char (%d) is out of range", args[1].as.u32);
            return pyro_null();
        }
    } else {
        pyro_panic(vm, "set(): invalid argument [byte], expected an integer");
        return pyro_null();
    }

    buf->bytes[index] = byte_value;
    return args[1];
}


static PyroValue buf_write(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, "write(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (arg_count == 1) {
        if (PYRO_IS_BUF(args[0])) {
            PyroBuf* src_buf = PYRO_AS_BUF(args[0]);
            if (!PyroBuf_append_bytes(buf, src_buf->count, src_buf->bytes, vm)) {
                pyro_panic(vm, "write(): out of memory");
                return pyro_null();
            }
            return pyro_i64((int64_t)src_buf->count);
        } else {
            PyroStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return pyro_null();
            }
            if (!PyroBuf_append_bytes(buf, string->count, (uint8_t*)string->bytes, vm)) {
                pyro_panic(vm, "write(): out of memory");
                return pyro_null();
            }
            return pyro_i64((int64_t)string->count);
        }
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "write(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "write()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    if (!PyroBuf_append_bytes(buf, string->count, (uint8_t*)string->bytes, vm)) {
        pyro_panic(vm, "write(): out of memory");
    }

    return pyro_i64((int64_t)string->count);
}


static PyroValue buf_is_empty(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    return pyro_bool(buf->count == 0);
}


static PyroValue buf_clear(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);
    PyroBuf_clear(buf, vm);
    return pyro_null();
}


void pyro_load_std_builtins_buf(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$buf", fn_buf, -1);
    pyro_define_superglobal_fn(vm, "$is_buf", fn_is_buf, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_buf, "$get", buf_get, 1);
    pyro_define_pri_method(vm, vm->class_buf, "$set", buf_set, 2);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_buf, "to_str", buf_to_str, 0);
    pyro_define_pub_method(vm, vm->class_buf, "count", buf_count, 0);
    pyro_define_pub_method(vm, vm->class_buf, "get", buf_get, 1);
    pyro_define_pub_method(vm, vm->class_buf, "set", buf_set, 2);
    pyro_define_pub_method(vm, vm->class_buf, "write_byte", buf_write_byte, 1);
    pyro_define_pub_method(vm, vm->class_buf, "write", buf_write, -1);
    pyro_define_pub_method(vm, vm->class_buf, "is_empty", buf_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_buf, "clear", buf_clear, 0);
}
