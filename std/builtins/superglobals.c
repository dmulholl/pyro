#include "../../inc/pyro.h"


static PyroValue fn_fmt(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$fmt(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$fmt(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$fmt()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_eprint(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$eprint(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        int64_t result = pyro_stderr_write_s(vm, string);
        if (result < 0) {
            pyro_panic(vm, "$eprint(): unable to write to the standard error stream");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$eprint(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$eprint()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result = pyro_stderr_write_s(vm, string);
    if (result < 0) {
        pyro_panic(vm, "$eprint(): unable to write to the standard error stream");
        return pyro_null();
    }

    return pyro_i64(result);
}


static PyroValue fn_eprintln(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        int64_t result = pyro_stderr_write_n(vm, "\n", 1);
        if (result < 0) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        int64_t result1 = pyro_stderr_write_s(vm, string);
        if (result1 < 0) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return pyro_null();
        }
        int64_t result2 = pyro_stderr_write_n(vm, "\n", 1);
        if (result2 < 0) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return pyro_null();
        }
        return pyro_i64(result1 + result2);
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$eprintln(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$eprintln()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result1 = pyro_stderr_write_s(vm, string);
    if (result1 < 0) {
        pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
        return pyro_null();
    }

    int64_t result2 = pyro_stderr_write_n(vm, "\n", 1);
    if (result2 < 0) {
        pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
        return pyro_null();
    }

    return pyro_i64(result1 + result2);
}


static PyroValue fn_print(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$print(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        int64_t result = pyro_stdout_write_s(vm, string);
        if (result < 0) {
            pyro_panic(vm, "$print(): unable to write to the standard output stream");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$print(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$print()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result = pyro_stdout_write_s(vm, string);
    if (result < 0) {
        pyro_panic(vm, "$print(): unable to write to the standard output stream");
        return pyro_null();
    }

    return pyro_i64(result);
}


static PyroValue fn_println(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        int64_t result = pyro_stdout_write_n(vm, "\n", 1);
        if (result < 0) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        int64_t result1 = pyro_stdout_write_s(vm, string);
        if (result1 < 0) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return pyro_null();
        }
        int64_t result2 = pyro_stdout_write_n(vm, "\n", 1);
        if (result2 < 0) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return pyro_null();
        }
        return pyro_i64(result1 + result2);
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$println(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$println()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result1 = pyro_stdout_write_s(vm, string);
    if (result1 < 0) {
        pyro_panic(vm, "$println(): unable to write to the standard output stream");
        return pyro_null();
    }

    int64_t result2 = pyro_stdout_write_n(vm, "\n", 1);
    if (result2 < 0) {
        pyro_panic(vm, "$println(): unable to write to the standard output stream");
        return pyro_null();
    }

    return pyro_i64(result1 + result2);
}


static PyroValue fn_exit(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        vm->halt_flag = true;
        vm->exit_flag = true;
        vm->exit_code = args[0].as.i64;
        return pyro_null();
    }

    PyroStr* error_message = pyro_stringify_value(vm, args[0]);
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result = pyro_stderr_write_s(vm, error_message);
    if (result < 0) {
        pyro_panic(vm, "$exit(): unable to write to the standard error stream");
        return pyro_null();
    }

    result = pyro_stderr_write(vm, "\n");
    if (result < 0) {
        pyro_panic(vm, "$exit(): unable to write to the standard error stream");
        return pyro_null();
    }

    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = 1;
    return pyro_null();
}


static PyroValue fn_panic(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$panic(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }

        PyroStr* escaped_string = pyro_double_escape_percents(vm, string->bytes, string->count);
        if (!escaped_string) {
            pyro_panic(vm, "$panic(): out of memory");
            return pyro_null();
        }

        pyro_panic(vm, escaped_string->bytes);
        return pyro_null();
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$panic(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$panic()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    PyroStr* escaped_string = pyro_double_escape_percents(vm, string->bytes, string->count);
    if (!escaped_string) {
        pyro_panic(vm, "$panic(): out of memory");
        return pyro_null();
    }

    pyro_panic(vm, escaped_string->bytes);
    return pyro_null();
}


static PyroValue fn_is_module(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_MOD(args[0]));
}


static PyroValue fn_is_obj(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_OBJ(args[0]));
}


static PyroValue fn_clock(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_f64((double)clock() / CLOCKS_PER_SEC);
}


static PyroValue fn_is_nan(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_F64(args[0]) && isnan(args[0].as.f64));
}


static PyroValue fn_is_inf(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_F64(args[0]) && isinf(args[0].as.f64));
}


static PyroValue fn_f64(PyroVM* vm, size_t arg_count, PyroValue* args) {
    switch (args[0].type) {
        case PYRO_VALUE_I64:
            return pyro_f64((double)args[0].as.i64);

        case PYRO_VALUE_F64:
            return args[0];

        case PYRO_VALUE_CHAR:
            return pyro_f64((double)args[0].as.u32);

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(args[0])) {
                PyroStr* string = PYRO_AS_STR(args[0]);
                double value;
                if (pyro_parse_string_as_float(string->bytes, string->count, &value)) {
                    return pyro_f64(value);
                }
                pyro_panic(vm, "$f64(): invalid argument, unable to parse string");
                return pyro_null();
            }
            pyro_panic(vm, "$f64(): invalid argument");
            return pyro_null();
        }

        default:
            pyro_panic(vm, "$f64(): invalid argument");
            return pyro_null();
    }
}


static PyroValue fn_is_f64(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_F64(args[0]));
}


static PyroValue fn_i64(PyroVM* vm, size_t arg_count, PyroValue* args) {
    switch (args[0].type) {
        case PYRO_VALUE_I64:
            return args[0];

        case PYRO_VALUE_CHAR:
            return pyro_i64((int64_t)args[0].as.u32);

        case PYRO_VALUE_F64: {
            if (args[0].as.f64 >= -9223372036854775808.0    // -2^63 == I64_MIN
                && args[0].as.f64 < 9223372036854775808.0   // 2^63 == I64_MAX + 1
            ) {
                return pyro_i64((int64_t)args[0].as.f64);
            }
            pyro_panic(vm, "$i64(): invalid argument, floating-point value is out-of-range");
            return pyro_null();
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(args[0])) {
                PyroStr* string = PYRO_AS_STR(args[0]);
                int64_t value;
                if (pyro_parse_string_as_int(string->bytes, string->count, &value)) {
                    return pyro_i64(value);
                }
                pyro_panic(vm, "$i64(): invalid argument, unable to parse string");
                return pyro_null();
            }
            pyro_panic(vm, "$i64(): invalid argument");
            return pyro_null();
        }

        default:
            pyro_panic(vm, "$i64(): invalid argument");
            return pyro_null();
    }
}


static PyroValue fn_is_i64(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_I64(args[0]));
}


static PyroValue fn_bool(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(pyro_is_truthy(args[0]));
}


static PyroValue fn_is_bool(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_BOOL(args[0]));
}


static PyroValue fn_has_method(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[1])) {
        pyro_panic(vm, "$has_method(): invalid argument [method_name], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_has_method(vm, args[0], PYRO_AS_STR(args[1])));
}


static PyroValue fn_has_field(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[1])) {
        pyro_panic(vm, "$has_field(): invalid argument [field_name], expected a string");
        return pyro_null();
    }
    PyroValue field_name = args[1];

    if (PYRO_IS_INSTANCE(args[0])) {
        PyroMap* field_index_map = PYRO_AS_INSTANCE(args[0])->obj.class->all_field_indexes;
        PyroValue field_index;
        if (PyroMap_get(field_index_map, field_name, &field_index, vm)) {
            return pyro_bool(true);
        }
    }

    return pyro_bool(false);
}


static PyroValue fn_is_instance(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_INSTANCE(args[0]));
}


static PyroValue fn_is_instance_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_CLASS(args[1])) {
        pyro_panic(vm, "$is_instance_of(): invalid argument [class], expected a class object");
        return pyro_null();
    }
    PyroClass* target_class = PYRO_AS_CLASS(args[1]);

    if (!PYRO_IS_INSTANCE(args[0])) {
        return pyro_bool(false);
    }
    PyroClass* instance_class = PYRO_AS_INSTANCE(args[0])->obj.class;

    while (instance_class != NULL) {
        if (instance_class == target_class) {
            return pyro_bool(true);
        }
        instance_class = instance_class->superclass;
    }

    return pyro_bool(false);
}


static PyroValue fn_shell_shortcut(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* out_str;
    PyroStr* err_str;
    int exit_code;

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$(): invalid argument [command], expected a string");
        return pyro_null();
    }

    if (!pyro_exec_shell_cmd(vm, PYRO_AS_STR(args[0])->bytes, NULL, 0, &out_str, &err_str, &exit_code)) {
        return pyro_null();
    }

    return pyro_obj(out_str);
}


static PyroValue fn_shell(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* out_str;
    PyroStr* err_str;
    int exit_code;

    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$shell(): invalid argument [command], expected a string");
            return pyro_null();
        }
        if (!pyro_exec_shell_cmd(vm, PYRO_AS_STR(args[0])->bytes, NULL, 0, &out_str, &err_str, &exit_code)) {
            return pyro_null();
        }
    } else if (arg_count == 2) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$shell(): invalid argument [command], expected a string");
            return pyro_null();
        }
        if (PYRO_IS_STR(args[1])) {
            if (!pyro_exec_shell_cmd(
                vm,
                PYRO_AS_STR(args[0])->bytes,
                (uint8_t*)PYRO_AS_STR(args[1])->bytes,
                PYRO_AS_STR(args[1])->count,
                &out_str,
                &err_str,
                &exit_code
            )) {
                return pyro_null();
            }
        } else if (PYRO_IS_BUF(args[1])) {
            if (!pyro_exec_shell_cmd(
                vm,
                PYRO_AS_STR(args[0])->bytes,
                PYRO_AS_BUF(args[1])->bytes,
                PYRO_AS_BUF(args[1])->count,
                &out_str,
                &err_str,
                &exit_code
            )) {
                return pyro_null();
            }
        } else {
            pyro_panic(vm, "$shell(): invalid argument [input], expected a string or buffer");
            return pyro_null();
        }
    } else {
        pyro_panic(vm, "$shell(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    PyroTup* tup = PyroTup_new(3, vm);
    if (!tup) {
        pyro_panic(vm, "$shell(): out of memory");
        return pyro_null();
    }

    tup->values[0] = pyro_i64(exit_code);
    tup->values[1] = pyro_obj(out_str);
    tup->values[2] = pyro_obj(err_str);

    return pyro_obj(tup);
}


static PyroValue fn_debug(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* string = pyro_debugify_value(vm, args[0]);
    if (vm->halt_flag) {
        return pyro_null();
    }
    return pyro_obj(string);
}


static PyroValue fn_read_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$read_file(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroBuf* buf = pyro_read_file_into_buf(vm, PYRO_AS_STR(args[0])->bytes, "$read_file()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    PyroStr* string = PyroBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "$read_file(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_write_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$write_file(): invalid argument [path], expected a string");
        return pyro_null();
    }

    if (!PYRO_IS_STR(args[1]) && !PYRO_IS_BUF(args[1])) {
        pyro_panic(vm, "$write_file(): invalid argument [content], expected a string or buffer");
        return pyro_null();
    }

    FILE* stream = fopen(PYRO_AS_STR(args[0])->bytes, "w");
    if (!stream) {
        pyro_panic(vm, "$write_file(): unable to open file '%s'", PYRO_AS_STR(args[0])->bytes);
        return pyro_null();
    }

    if (PYRO_IS_BUF(args[1])) {
        PyroBuf* buf = PYRO_AS_BUF(args[1]);
        size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, stream);
        if (n < buf->count) {
            pyro_panic(vm, "$write_file(): I/O write error");
            fclose(stream);
            return pyro_null();
        }
        fclose(stream);
        return pyro_i64((int64_t)n);
    }

    PyroStr* string = PYRO_AS_STR(args[1]);
    size_t n = fwrite(string->bytes, sizeof(char), string->count, stream);
    if (n < string->count) {
        pyro_panic(vm, "$write_file(): I/O write error");
        fclose(stream);
        return pyro_null();
    }
    fclose(stream);
    return pyro_i64((int64_t)n);
}


static PyroValue fn_hash(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_i64((int64_t)pyro_hash_value(vm, args[0]));
}


static PyroValue fn_sleep(PyroVM* vm, size_t arg_count, PyroValue* args) {
    double time_in_seconds;

    if (PYRO_IS_I64(args[0]) && args[0].as.i64 >= 0) {
        time_in_seconds = (double)args[0].as.i64;
    } else if (PYRO_IS_F64(args[0]) && args[0].as.f64 >= 0) {
        time_in_seconds = args[0].as.f64;
    } else {
        pyro_panic(vm, "$sleep(): invalid argument [time_in_seconds], expected a positive number");
        return pyro_null();
    }

    if (pyro_sleep(time_in_seconds) != 0) {
        pyro_panic(vm, "$sleep(): OS error");
    }

    return pyro_null();
}


static PyroValue fn_is_callable(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_CLOSURE(args[0])) {
        return pyro_bool(true);
    } else if (PYRO_IS_NATIVE_FN(args[0])) {
        return pyro_bool(true);
    } else if (PYRO_IS_CLASS(args[0])) {
        return pyro_bool(true);
    } else if (PYRO_IS_BOUND_METHOD(args[0])) {
        return pyro_bool(true);
    } else if (PYRO_IS_INSTANCE(args[0]) && pyro_has_method(vm, args[0], vm->str_dollar_call)) {
        return pyro_bool(true);
    }
    return pyro_bool(false);
}


static PyroValue fn_is_class(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_CLASS(args[0])) {
        return pyro_bool(true);
    }
    return pyro_bool(false);
}


static PyroValue fn_is_pyro_func(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_CLOSURE(args[0])) {
        return pyro_bool(true);
    }
    return pyro_bool(false);
}


static PyroValue fn_is_native_func(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_NATIVE_FN(args[0])) {
        return pyro_bool(true);
    }
    return pyro_bool(false);
}


static PyroValue fn_is_iterable(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(pyro_has_method(vm, args[0], vm->str_dollar_iter));
}


static PyroValue fn_is_iterator(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(pyro_has_method(vm, args[0], vm->str_dollar_next));
}


static PyroValue fn_env(PyroVM* vm, size_t arg_count, PyroValue* args) {
    // Note: getenv() is part of the C standard library.
    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$env(): invalid argument [name], expected a string");
            return pyro_null();
        }

        PyroStr* name = PYRO_AS_STR(args[0]);
        char* value = getenv(name->bytes);
        if (!value) {
            return pyro_obj(vm->error);
        }

        PyroStr* string = PyroStr_COPY(value);
        if (!string) {
            pyro_panic(vm, "$env(): out of memory");
            return pyro_null();
        }

        return pyro_obj(string);
    }

    // Note: setenv() is a POSIX function, not part of the C standard library.
    if (arg_count == 2) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$env(): invalid argument [name], expected a string");
            return pyro_null();
        }
        PyroStr* name = PYRO_AS_STR(args[0]);

        PyroStr* value = pyro_stringify_value(vm, args[1]);
        if (vm->halt_flag) {
            return pyro_null();
        }

        return pyro_bool(pyro_setenv(name->bytes, value->bytes));
    }

    pyro_panic(vm, "$env(): expected 1 or 2 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_is_null(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_NULL(args[0]));
}


static PyroValue fn_input(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count > 1) {
        pyro_panic(vm, "$input(): expected 0 or 1 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$input(): invalid argument [prompt], expected a string");
            return pyro_null();
        }
        PyroStr* prompt = PYRO_AS_STR(args[0]);
        pyro_stdout_write_s(vm, prompt);
        pyro_stdout_flush(vm);
    }

    PyroFile* file = vm->stdin_file;
    PyroStr* string = PyroFile_read_line(file, vm);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return string ? pyro_obj(string) : pyro_null();
}


static PyroValue fn_exec(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0 || arg_count > 3) {
        pyro_panic(vm, "$exec(): expected 1, 2, or 3 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$exec(): invalid argument [code], expected a string");
        return pyro_null();
    }
    PyroStr* code = PYRO_AS_STR(args[0]);

    PyroMod* module;
    if (arg_count > 2) {
        if (!PYRO_IS_MOD(args[2])) {
            pyro_panic(vm, "$exec(): invalid argument [module], expected a module");
            return pyro_null();
        }
        module = PYRO_AS_MOD(args[2]);
    } else {
        module = PyroMod_new(vm);
        if (!module) {
            pyro_panic(vm, "$exec(): out of memory");
            return pyro_null();
        }
    }

    const char* source_id;
    if (arg_count > 1) {
        if (!PYRO_IS_STR(args[1])) {
            pyro_panic(vm, "$exec(): invalid argument [source_id], expected a string");
            return pyro_null();
        }
        source_id = PYRO_AS_STR(args[1])->bytes;
    } else {
        source_id = "<exec>";
    }

    // Push the module onto the stack to keep it safe from the garbage collector.
    if (!pyro_push(vm, pyro_obj(module))) return pyro_null();
    pyro_exec_code(vm, code->bytes, code->count, source_id, module);
    pyro_pop(vm);

    return pyro_obj(module);
}


static PyroValue fn_type(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroStr* type_name = pyro_get_type_name(vm, args[0]);
    return pyro_obj(type_name);
}


static PyroValue fn_method(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroValue obj = args[0];

    if (!PYRO_IS_STR(args[1])) {
        pyro_panic(vm, "$method(): invalid argument [method_name], expected a string");
        return pyro_null();
    }
    PyroStr* method_name = PYRO_AS_STR(args[1]);

    PyroValue method = pyro_get_method(vm, obj, method_name);
    if (PYRO_IS_NULL(method)) {
        return pyro_obj(vm->error);
    }

    PyroBoundMethod* bound_method = PyroBoundMethod_new(vm, obj, PYRO_AS_OBJ(method));
    if (!bound_method) {
        pyro_panic(vm, "$method(): out of memory");
        return pyro_null();
    }

    return pyro_obj(bound_method);
}


static PyroValue fn_methods(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroClass* class = pyro_get_class(vm, args[0]);
    if (!class) {
        PyroIter* iter = PyroIter_empty(vm);
        if (!iter) {
            pyro_panic(vm, "$methods(): out of memory");
            return pyro_null();
        }
        return pyro_obj(iter);
    }

    PyroIter* iter = PyroIter_new((PyroObject*)class->pub_instance_methods, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "$methods(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue fn_field(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[1])) {
        pyro_panic(vm, "$field(): invalid argument [field_name], expected a string");
        return pyro_null();
    }
    PyroValue field_name = args[1];

    if (PYRO_IS_INSTANCE(args[0])) {
        PyroMap* field_index_map = PYRO_AS_INSTANCE(args[0])->obj.class->all_field_indexes;
        PyroValue field_index;
        if (PyroMap_get(field_index_map, field_name, &field_index, vm)) {
            return PYRO_AS_INSTANCE(args[0])->fields[field_index.as.i64];
        }
    }

    return pyro_obj(vm->error);
}


static PyroValue fn_fields(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroClass* class = pyro_get_class(vm, args[0]);
    if (!class) {
        PyroIter* iter = PyroIter_empty(vm);
        if (!iter) {
            pyro_panic(vm, "$fields(): out of memory");
            return pyro_null();
        }
        return pyro_obj(iter);
    }

    PyroIter* iter = PyroIter_new((PyroObject*)class->pub_field_indexes, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "$fields(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue fn_is_func(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_CLOSURE(args[0]) || PYRO_IS_NATIVE_FN(args[0]));
}


static PyroValue fn_is_method(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_BOUND_METHOD(args[0]));
}


static PyroValue fn_stdout(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        if (vm->stdout_file) {
            return pyro_obj(vm->stdout_file);
        }
        return pyro_null();
    }

    if (arg_count == 1) {
        if (!PYRO_IS_FILE(args[0])) {
            pyro_panic(vm, "$stdout(): invalid argument, expected a file");
            return pyro_null();
        }
        vm->stdout_file = PYRO_AS_FILE(args[0]);
        return pyro_null();
    }

    pyro_panic(vm, "$stdout(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_stderr(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        if (vm->stderr_file) {
            return pyro_obj(vm->stderr_file);
        }
        return pyro_null();
    }

    if (arg_count == 1) {
        if (!PYRO_IS_FILE(args[0])) {
            pyro_panic(vm, "$stderr(): invalid argument, expected a file");
            return pyro_null();
        }
        vm->stderr_file = PYRO_AS_FILE(args[0]);
        return pyro_null();
    }

    pyro_panic(vm, "$stderr(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_stdin(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        if (vm->stdin_file) {
            return pyro_obj(vm->stdin_file);
        }
        return pyro_null();
    }

    if (arg_count == 1) {
        if (!PYRO_IS_FILE(args[0])) {
            pyro_panic(vm, "$stdin(): invalid argument, expected a file");
            return pyro_null();
        }
        vm->stdin_file = PYRO_AS_FILE(args[0]);
        return pyro_null();
    }

    pyro_panic(vm, "$stdin(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_import(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$import(): invalid argument [path], expected a string");
        return pyro_null();
    }
    PyroStr* import_path = PYRO_AS_STR(args[0]);

    PyroMod* module = PyroMod_new(vm);
    if (!module) {
        pyro_panic(vm, "$import(): out of memory");
        return pyro_null();
    }

    if (!pyro_push(vm, pyro_obj(module))) {
        return pyro_null();
    }

    pyro_import_module_from_string(vm, import_path->bytes, module);
    pyro_pop(vm);
    return pyro_obj(module);
}


static PyroValue fn_add(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0]) && PYRO_IS_I64(args[1])) {
        int64_t result;
        if (pyro_ckd_add(&result, args[0].as.i64, args[1].as.i64)) {
            pyro_panic(vm, "$add(): signed integer overflow");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    return pyro_op_binary_plus(vm, args[0], args[1]);
}


static PyroValue fn_sub(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0]) && PYRO_IS_I64(args[1])) {
        int64_t result;
        if (pyro_ckd_sub(&result, args[0].as.i64, args[1].as.i64)) {
            pyro_panic(vm, "$sub(): signed integer overflow");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    return pyro_op_binary_minus(vm, args[0], args[1]);
}


static PyroValue fn_mul(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0]) && PYRO_IS_I64(args[1])) {
        int64_t result;
        if (pyro_ckd_mul(&result, args[0].as.i64, args[1].as.i64)) {
            pyro_panic(vm, "$mul(): signed integer overflow");
            return pyro_null();
        }
        return pyro_i64(result);
    }

    return pyro_op_binary_star(vm, args[0], args[1]);
}


/* -------- */
/*  Public  */
/* -------- */


void pyro_load_std_builtins(PyroVM* vm) {
    pyro_define_superglobal(vm, "$roots", pyro_obj(vm->import_roots));

    PyroTup* args = PyroTup_new(0, vm);
    if (args) {
        pyro_define_superglobal(vm, "$args", pyro_obj(args));
    }

    pyro_define_superglobal_fn(vm, "$", fn_shell_shortcut, 1);
    pyro_define_superglobal_fn(vm, "$bool", fn_bool, 1);
    pyro_define_superglobal_fn(vm, "$clock", fn_clock, 0);
    pyro_define_superglobal_fn(vm, "$debug", fn_debug, 1);
    pyro_define_superglobal_fn(vm, "$env", fn_env, -1);
    pyro_define_superglobal_fn(vm, "$eprint", fn_eprint, -1);
    pyro_define_superglobal_fn(vm, "$eprintln", fn_eprintln, -1);
    pyro_define_superglobal_fn(vm, "$exec", fn_exec, -1);
    pyro_define_superglobal_fn(vm, "$exit", fn_exit, 1);
    pyro_define_superglobal_fn(vm, "$f64", fn_f64, 1);
    pyro_define_superglobal_fn(vm, "$field", fn_field, 2);
    pyro_define_superglobal_fn(vm, "$fields", fn_fields, 1);
    pyro_define_superglobal_fn(vm, "$fmt", fn_fmt, -1);
    pyro_define_superglobal_fn(vm, "$has_field", fn_has_field, 2);
    pyro_define_superglobal_fn(vm, "$has_method", fn_has_method, 2);
    pyro_define_superglobal_fn(vm, "$hash", fn_hash, 1);
    pyro_define_superglobal_fn(vm, "$i64", fn_i64, 1);
    pyro_define_superglobal_fn(vm, "$input", fn_input, -1);
    pyro_define_superglobal_fn(vm, "$is_bool", fn_is_bool, 1);
    pyro_define_superglobal_fn(vm, "$is_callable", fn_is_callable, 1);
    pyro_define_superglobal_fn(vm, "$is_class", fn_is_class, 1);
    pyro_define_superglobal_fn(vm, "$is_f64", fn_is_f64, 1);
    pyro_define_superglobal_fn(vm, "$is_i64", fn_is_i64, 1);
    pyro_define_superglobal_fn(vm, "$is_inf", fn_is_inf, 1);
    pyro_define_superglobal_fn(vm, "$is_instance", fn_is_instance, 1);
    pyro_define_superglobal_fn(vm, "$is_instance_of", fn_is_instance_of, 2);
    pyro_define_superglobal_fn(vm, "$is_iterable", fn_is_iterable, 1);
    pyro_define_superglobal_fn(vm, "$is_iterator", fn_is_iterator, 1);
    pyro_define_superglobal_fn(vm, "$is_module", fn_is_module, 1);
    pyro_define_superglobal_fn(vm, "$is_nan", fn_is_nan, 1);
    pyro_define_superglobal_fn(vm, "$is_native_func", fn_is_native_func, 1);
    pyro_define_superglobal_fn(vm, "$is_null", fn_is_null, 1);
    pyro_define_superglobal_fn(vm, "$is_obj", fn_is_obj, 1);
    pyro_define_superglobal_fn(vm, "$is_pyro_func", fn_is_pyro_func, 1);
    pyro_define_superglobal_fn(vm, "$method", fn_method, 2);
    pyro_define_superglobal_fn(vm, "$methods", fn_methods, 1);
    pyro_define_superglobal_fn(vm, "$panic", fn_panic, -1);
    pyro_define_superglobal_fn(vm, "$print", fn_print, -1);
    pyro_define_superglobal_fn(vm, "$println", fn_println, -1);
    pyro_define_superglobal_fn(vm, "$read_file", fn_read_file, 1);
    pyro_define_superglobal_fn(vm, "$shell", fn_shell, -1);
    pyro_define_superglobal_fn(vm, "$sleep", fn_sleep, 1);
    pyro_define_superglobal_fn(vm, "$type", fn_type, 1);
    pyro_define_superglobal_fn(vm, "$write_file", fn_write_file, 2);
    pyro_define_superglobal_fn(vm, "$is_func", fn_is_func, 1);
    pyro_define_superglobal_fn(vm, "$is_method", fn_is_method, 1);
    pyro_define_superglobal_fn(vm, "$stdin", fn_stdin, -1);
    pyro_define_superglobal_fn(vm, "$stdout", fn_stdout, -1);
    pyro_define_superglobal_fn(vm, "$stderr", fn_stderr, -1);
    pyro_define_superglobal_fn(vm, "$import", fn_import, 1);
    pyro_define_superglobal_fn(vm, "$add", fn_add, 2);
    pyro_define_superglobal_fn(vm, "$sub", fn_sub, 2);
    pyro_define_superglobal_fn(vm, "$mul", fn_mul, 2);
}
