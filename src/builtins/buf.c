#include "../includes/pyro.h"


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
        } else if (PYRO_IS_RUNE(args[1]) && args[1].as.u32 <= 255) {
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
    } else if (PYRO_IS_RUNE(args[0])) {
        if (args[0].as.u32 <= 255) {
            byte_value = (uint8_t)args[0].as.u32;
        } else {
            pyro_panic(vm, "write_byte(): invalid argument [byte], rune (%d) is out of range", args[0].as.u32);
            return pyro_null();
        }
    } else {
        pyro_panic(vm,
            "write_byte(): invalid argument [byte], expected an i64 or rune, got %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    if (!PyroBuf_append_byte(buf, byte_value, vm)) {
        pyro_panic(vm, "write_byte(): out of memory");
    }

    return pyro_null();
}


static PyroValue buf_to_str(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    PyroStr* string = PyroBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "to_str(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue buf_as_str(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* string = pyro_stringify_value(vm, args[-1]);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue buf_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "get(): invalid argument [index], expected i64, got %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    int64_t index = args[0].as.i64;
    if (index < 0) {
        index += buf->count;
    }

    if (index < 0 || (size_t)index >= buf->count) {
        pyro_panic(vm, "get(): index %" PRId64 " is out of range", index);
        return pyro_null();
    }

    return pyro_i64(buf->bytes[index]);
}


static PyroValue buf_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "set(): invalid argument [index], expected i64, got %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    int64_t index = args[0].as.i64;
    if (index < 0) {
        index += buf->count;
    }

    if (index < 0 || (size_t)index >= buf->count) {
        pyro_panic(vm, "set(): index %" PRId64 " is out of range", index);
        return pyro_null();
    }

    uint8_t byte_value;
    if (PYRO_IS_I64(args[1])) {
        if (args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
            byte_value = (uint8_t)args[1].as.i64;
        } else {
            pyro_panic(vm, "set(): invalid argument [value], integer (%d) is out of range", args[1].as.i64);
            return pyro_null();
        }
    } else if (PYRO_IS_RUNE(args[1])) {
        if (args[1].as.u32 <= 255) {
            byte_value = (uint8_t)args[1].as.u32;
        } else {
            pyro_panic(vm, "set(): invalid argument [value], rune (%d) is out of range", args[1].as.u32);
            return pyro_null();
        }
    } else {
        pyro_panic(vm,
            "set(): invalid argument [value], expected an i64 or rune, got %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
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


static PyroValue buf_resize(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    if (arg_count == 0 || arg_count > 2) {
        pyro_panic(vm, "resize(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "resize(): invalid argument [new_size], expected an i64, found %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    int64_t new_size = args[0].as.i64;
    uint8_t fill_value = 0;

    if (arg_count == 2) {
        if (PYRO_IS_I64(args[1]) && args[1].as.i64 >= 0 && args[1].as.i64 <= 255) {
            fill_value = (uint8_t)args[1].as.i64;
        } else if (PYRO_IS_RUNE(args[1]) && args[1].as.u32 <= 255) {
            fill_value = (uint8_t)args[1].as.u32;
        } else {
            pyro_panic(vm, "resize(): invalid argument [fill_value], expected an integer in the range [0,255]");
            return pyro_null();
        }
    }

    if (new_size < 0) {
        if (new_size == INT64_MIN) {
            pyro_panic(vm, "resize(): invalid argument [new_size], out of range");
            return pyro_null();
        }

        size_t bytes_to_truncate = (size_t)(new_size * -1);
        if (bytes_to_truncate > buf->count) {
            pyro_panic(vm, "resize(): invalid argument [new_size], out of range");
            return pyro_null();
        }

        buf->count -= bytes_to_truncate;
        return pyro_null();
    }

    size_t new_count = (size_t)new_size;

    if (new_count <= buf->count) {
        buf->count = new_count;
        return pyro_null();
    }

    if (new_count > buf->capacity) {
        if (!PyroBuf_resize_capacity(buf, new_count + 1, vm)) {
            pyro_panic(vm, "out of memory");
            return pyro_null();
        }
    }

    for (size_t index = buf->count; index < new_count; index++) {
        buf->bytes[index] = fill_value;
    }

    buf->count = new_count;
    return pyro_null();
}


static PyroValue buf_match(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = PYRO_AS_BUF(args[-1]);

    void* target;
    size_t target_count;

    if (PYRO_IS_BUF(args[0])) {
        target = PYRO_AS_BUF(args[0])->bytes;
        target_count = PYRO_AS_BUF(args[0])->count;
    } else if (PYRO_IS_STR(args[0])) {
        target = PYRO_AS_STR(args[0])->bytes;
        target_count = PYRO_AS_STR(args[0])->count;
    } else {
        pyro_panic(vm, "match(): invalid argument [target], expected a string or buffer");
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, "match(): invalid argument [index], expected a positive integer");
        return pyro_null();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target_count > buf->count) {
        return pyro_bool(false);
    }

    // Returns 0 if [count] is zero.
    if (memcmp(&buf->bytes[index], target, target_count) == 0) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


void pyro_load_builtin_type_buf(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$buf", fn_buf, -1);
    pyro_define_superglobal_fn(vm, "$is_buf", fn_is_buf, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_buf, "$get", buf_get, 1);
    pyro_define_pri_method(vm, vm->class_buf, "$set", buf_set, 2);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_buf, "to_str", buf_to_str, 0);
    pyro_define_pub_method(vm, vm->class_buf, "as_str", buf_as_str, 0);
    pyro_define_pub_method(vm, vm->class_buf, "count", buf_count, 0);
    pyro_define_pub_method(vm, vm->class_buf, "get", buf_get, 1);
    pyro_define_pub_method(vm, vm->class_buf, "byte", buf_get, 1);
    pyro_define_pub_method(vm, vm->class_buf, "set", buf_set, 2);
    pyro_define_pub_method(vm, vm->class_buf, "write_byte", buf_write_byte, 1);
    pyro_define_pub_method(vm, vm->class_buf, "write", buf_write, -1);
    pyro_define_pub_method(vm, vm->class_buf, "is_empty", buf_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_buf, "clear", buf_clear, 0);
    pyro_define_pub_method(vm, vm->class_buf, "resize", buf_resize, -1);
    pyro_define_pub_method(vm, vm->class_buf, "match", buf_match, 2);
}
