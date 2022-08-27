#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/stringify.h"
#include "../../inc/panics.h"
#include "../../inc/os.h"
#include "../../inc/io.h"
#include "../../inc/exec.h"


static Value fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "$fmt(): expected 2 or more arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$fmt(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    ObjStr* fmt_str = AS_STR(args[0]);

    char fmt_spec_buffer[16];
    size_t fmt_spec_count = 0;

    char* out_buffer = NULL;
    size_t out_capacity = 0;
    size_t out_count = 0;

    size_t fmt_str_index = 0;
    size_t next_arg_index = 1;

    while (fmt_str_index < fmt_str->length) {
        if (out_count + 2 > out_capacity) {
            size_t new_capacity = GROW_CAPACITY(out_capacity);
            char* new_array = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, new_capacity);
            if (!new_array) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "$fmt(): out of memory");
                return MAKE_NULL();
            }
            out_capacity = new_capacity;
            out_buffer = new_array;
        }

        if (fmt_str_index < fmt_str->length - 1) {
            if (fmt_str->bytes[fmt_str_index] == '\\') {
                if (fmt_str->bytes[fmt_str_index + 1] == '{') {
                    out_buffer[out_count++] = '{';
                    fmt_str_index += 2;
                    continue;
                }
            }
        }

        if (fmt_str->bytes[fmt_str_index] == '{') {
            fmt_str_index++;
            while (fmt_str_index < fmt_str->length && fmt_str->bytes[fmt_str_index] != '}') {
                if (fmt_spec_count == 15) {
                    FREE_ARRAY(vm, char, out_buffer, out_capacity);
                    pyro_panic(vm, "$fmt(): invalid format specifier, too many characters");
                    return MAKE_NULL();
                }
                fmt_spec_buffer[fmt_spec_count++] = fmt_str->bytes[fmt_str_index++];
            }
            if (fmt_str_index == fmt_str->length) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "$fmt(): missing '}' in format string");
                return MAKE_NULL();
            }
            fmt_str_index++;
            fmt_spec_buffer[fmt_spec_count] = '\0';

            if (next_arg_index == arg_count) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "$fmt(): too few arguments for format string");
                return MAKE_NULL();
            }
            Value arg = args[next_arg_index++];

            ObjStr* formatted;
            if (fmt_spec_count == 0) {
                formatted = pyro_stringify_value(vm, arg);
            } else {
                formatted = pyro_format_value(vm, arg, fmt_spec_buffer);
            }
            if (vm->halt_flag) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                return MAKE_NULL();
            }

            if (out_count + formatted->length + 1 > out_capacity) {
                size_t new_capacity = out_count + formatted->length + 1;

                pyro_push(vm, MAKE_OBJ(formatted));
                char* new_array = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, new_capacity);
                pyro_pop(vm);

                if (!new_array) {
                    FREE_ARRAY(vm, char, out_buffer, out_capacity);
                    pyro_panic(vm, "$fmt(): out of memory");
                    return MAKE_NULL();
                }

                out_capacity = new_capacity;
                out_buffer = new_array;
            }

            memcpy(&out_buffer[out_count], formatted->bytes, formatted->length);
            out_count += formatted->length;
            fmt_spec_count = 0;
            continue;
        }

        out_buffer[out_count++] = fmt_str->bytes[fmt_str_index++];
    }

    if (out_capacity > out_count + 1) {
        out_buffer = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, out_count + 1);
        out_capacity = out_count + 1;
    }
    out_buffer[out_count] = '\0';

    ObjStr* string = ObjStr_take(out_buffer, out_count, vm);
    if (!string) {
        FREE_ARRAY(vm, char, out_buffer, out_capacity);
        pyro_panic(vm, "$fmt(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(string);
}


static Value fn_eprint(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$eprint(): expected 1 or more arguments, found 0");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_stderr_write_s(vm, string);
        if (result == -1) {
            pyro_panic(vm, "$eprint(): unable to write to the standard error stream");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, "$eprint(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$eprint(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_stderr_write_s(vm, AS_STR(formatted));
    if (result == -1) {
        pyro_panic(vm, "$eprint(): unable to write to the standard error stream");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, "$eprint(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_eprintln(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        int64_t result = pyro_stderr_write_n(vm, "\n", 1);
        if (result == -1) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, "$eprintln(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result1 = pyro_stderr_write_s(vm, string);
        if (result1 == -1) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return MAKE_NULL();
        } else if (result1 == -2) {
            pyro_panic(vm, "$eprintln(): out of memory");
            return MAKE_NULL();
        }
        int64_t result2 = pyro_stderr_write_n(vm, "\n", 1);
        if (result2 == -1) {
            pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
            return MAKE_NULL();
        } else if (result2 == -2) {
            pyro_panic(vm, "$eprintln(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result1 + result2);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$eprintln(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result1 = pyro_stderr_write_s(vm, AS_STR(formatted));
    if (result1 == -1) {
        pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
        return MAKE_NULL();
    } else if (result1 == -2) {
        pyro_panic(vm, "$eprintln(): out of memory");
        return MAKE_NULL();
    }
    int64_t result2 = pyro_stderr_write_n(vm, "\n", 1);
    if (result2 == -1) {
        pyro_panic(vm, "$eprintln(): unable to write to the standard error stream");
        return MAKE_NULL();
    } else if (result2 == -2) {
        pyro_panic(vm, "$eprintln(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_I64(result1 + result2);
}


static Value fn_print(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$print(): expected 1 or more arguments, found 0");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_stdout_write_s(vm, string);
        if (result == -1) {
            pyro_panic(vm, "$print(): unable to write to the standard output stream");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, "$print(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$print(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_stdout_write_s(vm, AS_STR(formatted));
    if (result == -1) {
        pyro_panic(vm, "$print(): unable to write to the standard output stream");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, "$print(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_println(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        int64_t result = pyro_stdout_write_n(vm, "\n", 1);
        if (result == -1) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, "$println(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result1 = pyro_stdout_write_s(vm, string);
        if (result1 == -1) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return MAKE_NULL();
        } else if (result1 == -2) {
            pyro_panic(vm, "$println(): out of memory");
            return MAKE_NULL();
        }
        int64_t result2 = pyro_stdout_write_n(vm, "\n", 1);
        if (result2 == -1) {
            pyro_panic(vm, "$println(): unable to write to the standard output stream");
            return MAKE_NULL();
        } else if (result2 == -2) {
            pyro_panic(vm, "$println(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_I64(result1 + result2);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$println(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result1 = pyro_stdout_write_s(vm, AS_STR(formatted));
    if (result1 == -1) {
        pyro_panic(vm, "$println(): unable to write to the standard output stream");
        return MAKE_NULL();
    } else if (result1 == -2) {
        pyro_panic(vm, "$println(): out of memory");
        return MAKE_NULL();
    }
    int64_t result2 = pyro_stdout_write_n(vm, "\n", 1);
    if (result2 == -1) {
        pyro_panic(vm, "$println(): unable to write to the standard output stream");
        return MAKE_NULL();
    } else if (result2 == -2) {
        pyro_panic(vm, "$println(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_I64(result1 + result2);
}


static Value fn_exit(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0])) {
        pyro_panic(vm, "$exit(): invalid argument [code], expected an integer");
        return MAKE_NULL();
    }
    vm->halt_flag = true;
    vm->exit_flag = true;
    vm->exit_code = args[0].as.i64;
    return MAKE_NULL();
}


static Value fn_panic(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "$panic(): expected 1 or more arguments, found 0");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        ObjStr* panic_message = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }

        ObjStr* escaped_panic_message = ObjStr_esc_percents(panic_message->bytes, panic_message->length, vm);
        if (!escaped_panic_message) {
            pyro_panic(vm, "$panic(): out of memory");
            return MAKE_NULL();
        }

        pyro_panic(vm, escaped_panic_message->bytes);
        return MAKE_NULL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$panic(): invalid argument [format_string], expected a string");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }
    ObjStr* panic_message = AS_STR(formatted);

    ObjStr* escaped_panic_message = ObjStr_esc_percents(panic_message->bytes, panic_message->length, vm);
    if (!escaped_panic_message) {
        pyro_panic(vm, "$panic(): out of memory");
        return MAKE_NULL();
    }

    pyro_panic(vm, escaped_panic_message->bytes);
    return MAKE_NULL();
}


static Value fn_is_mod(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_MOD(args[0]));
}


static Value fn_is_obj(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_OBJ(args[0]));
}


static Value fn_clock(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_F64((double)clock() / CLOCKS_PER_SEC);
}


static Value fn_is_nan(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_F64(args[0]) && isnan(args[0].as.f64));
}


static Value fn_is_inf(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_F64(args[0]) && isinf(args[0].as.f64));
}


static Value fn_f64(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[0].type) {
        case VAL_I64:
            return MAKE_F64((double)args[0].as.i64);

        case VAL_F64:
            return args[0];

        case VAL_CHAR:
            return MAKE_F64((double)args[0].as.u32);

        case VAL_OBJ: {
            if (IS_STR(args[0])) {
                ObjStr* string = AS_STR(args[0]);
                double value;
                if (pyro_parse_string_as_float(string->bytes, string->length, &value)) {
                    return MAKE_F64(value);
                }
                pyro_panic(vm, "$f64(): invalid argument, unable to parse string");
                return MAKE_NULL();
            }
            pyro_panic(vm, "$f64(): invalid argument");
            return MAKE_NULL();
        }

        default:
            pyro_panic(vm, "$f64(): invalid argument");
            return MAKE_NULL();
    }
}


static Value fn_is_f64(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_F64(args[0]));
}


static Value fn_i64(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[0].type) {
        case VAL_I64:
            return args[0];

        case VAL_CHAR:
            return MAKE_I64((int64_t)args[0].as.u32);

        case VAL_F64: {
            if (args[0].as.f64 >= -9223372036854775808.0    // -2^63 == I64_MIN
                && args[0].as.f64 < 9223372036854775808.0   // 2^63 == I64_MAX + 1
            ) {
                return MAKE_I64((int64_t)args[0].as.f64);
            }
            pyro_panic(vm, "$i64(): invalid argument, floating-point value is out-of-range");
            return MAKE_NULL();
        }

        case VAL_OBJ: {
            if (IS_STR(args[0])) {
                ObjStr* string = AS_STR(args[0]);
                int64_t value;
                if (pyro_parse_string_as_int(string->bytes, string->length, &value)) {
                    return MAKE_I64(value);
                }
                pyro_panic(vm, "$i64(): invalid argument, unable to parse string");
                return MAKE_NULL();
            }
            pyro_panic(vm, "$i64(): invalid argument");
            return MAKE_NULL();
        }

        default:
            pyro_panic(vm, "$i64(): invalid argument");
            return MAKE_NULL();
    }
}


static Value fn_is_i64(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_I64(args[0]));
}


static Value fn_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_is_truthy(args[0]));
}


static Value fn_is_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_BOOL(args[0]));
}


static Value fn_has_method(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, "$has_method(): invalid argument [method_name], expected a string");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_has_method(vm, args[0], AS_STR(args[1])));
}


static Value fn_has_field(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, "$has_field(): invalid argument [field_name], expected a string");
        return MAKE_NULL();
    }
    Value field_name = args[1];

    if (IS_INSTANCE(args[0])) {
        ObjMap* field_index_map = AS_INSTANCE(args[0])->obj.class->all_field_indexes;
        Value field_index;
        if (ObjMap_get(field_index_map, field_name, &field_index, vm)) {
            return MAKE_BOOL(true);
        }
    }

    return MAKE_BOOL(false);
}


static Value fn_is_instance(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_INSTANCE(args[0]));
}


static Value fn_is_instance_of(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_CLASS(args[1])) {
        pyro_panic(vm, "$is_instance_of(): invalid argument [class], expected a class object");
        return MAKE_NULL();
    }
    ObjClass* target_class = AS_CLASS(args[1]);

    if (!IS_INSTANCE(args[0])) {
        return MAKE_BOOL(false);
    }
    ObjClass* instance_class = AS_INSTANCE(args[0])->obj.class;

    while (instance_class != NULL) {
        if (instance_class == target_class) {
            return MAKE_BOOL(true);
        }
        instance_class = instance_class->superclass;
    }

    return MAKE_BOOL(false);
}


static Value fn_shell_shortcut(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* out_str;
    ObjStr* err_str;
    int exit_code;

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$(): invalid argument [cmd], expected a string");
        return MAKE_NULL();
    }

    if (!pyro_exec_shell_cmd(vm, AS_STR(args[0])->bytes, NULL, 0, &out_str, &err_str, &exit_code)) {
        return MAKE_NULL();
    }

    return MAKE_OBJ(out_str);
}


static Value fn_shell(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* out_str;
    ObjStr* err_str;
    int exit_code;

    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$shell(): invalid argument [cmd], expected a string");
            return MAKE_NULL();
        }
        if (!pyro_exec_shell_cmd(vm, AS_STR(args[0])->bytes, NULL, 0, &out_str, &err_str, &exit_code)) {
            return MAKE_NULL();
        }
    } else if (arg_count == 2) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$shell(): invalid argument [cmd], expected a string");
            return MAKE_NULL();
        }
        if (IS_STR(args[1])) {
            if (!pyro_exec_shell_cmd(
                vm,
                AS_STR(args[0])->bytes,
                (uint8_t*)AS_STR(args[1])->bytes,
                AS_STR(args[1])->length,
                &out_str,
                &err_str,
                &exit_code
            )) {
                return MAKE_NULL();
            }
        } else if (IS_BUF(args[1])) {
            if (!pyro_exec_shell_cmd(
                vm,
                AS_STR(args[0])->bytes,
                AS_BUF(args[1])->bytes,
                AS_BUF(args[1])->count,
                &out_str,
                &err_str,
                &exit_code
            )) {
                return MAKE_NULL();
            }
        } else {
            pyro_panic(vm, "$shell(): invalid argument [input], expected a string or buffer");
            return MAKE_NULL();
        }
    } else {
        pyro_panic(vm, "$shell(): expected 1 or 2 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    ObjTup* tup = ObjTup_new(3, vm);
    if (!tup) {
        pyro_panic(vm, "$shell(): out of memory");
        return MAKE_NULL();
    }

    tup->values[0] = MAKE_I64(exit_code);
    tup->values[1] = MAKE_OBJ(out_str);
    tup->values[2] = MAKE_OBJ(err_str);

    return MAKE_OBJ(tup);
}


static Value fn_debug(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* string = pyro_debugify_value(vm, args[0]);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }
    return MAKE_OBJ(string);
}


static Value fn_read_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$read_file(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "r");
    if (!stream) {
        pyro_panic(vm, "$read_file(): unable to open file '%s'", AS_STR(args[0])->bytes);
        return MAKE_NULL();
    }

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "$read_file(): out of memory");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                fclose(stream);
                return MAKE_NULL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(stream);

        if (c == EOF) {
            if (ferror(stream)) {
                pyro_panic(vm, "$read_file(): I/O read error");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                fclose(stream);
                return MAKE_NULL();
            }
            break;
        }

        array[count++] = c;
    }

    array[count] = '\0';
    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }

    ObjStr* string = ObjStr_take((char*)array, count, vm);
    if (!string) {
        pyro_panic(vm, "$read_file(): out of memory");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        fclose(stream);
        return MAKE_NULL();
    }

    fclose(stream);
    return MAKE_OBJ(string);
}


static Value fn_write_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$write_file(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    if (!IS_STR(args[1]) && !IS_BUF(args[1])) {
        pyro_panic(vm, "$write_file(): invalid argument [content], expected a string or buffer");
        return MAKE_NULL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "w");
    if (!stream) {
        pyro_panic(vm, "$write_file(): unable to open file '%s'", AS_STR(args[0])->bytes);
        return MAKE_NULL();
    }

    if (IS_BUF(args[1])) {
        ObjBuf* buf = AS_BUF(args[1]);
        size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, stream);
        if (n < buf->count) {
            pyro_panic(vm, "$write_file(): I/O write error");
            fclose(stream);
            return MAKE_NULL();
        }
        fclose(stream);
        return MAKE_I64((int64_t)n);
    }

    ObjStr* string = AS_STR(args[1]);
    size_t n = fwrite(string->bytes, sizeof(char), string->length, stream);
    if (n < string->length) {
        pyro_panic(vm, "$write_file(): I/O write error");
        fclose(stream);
        return MAKE_NULL();
    }
    fclose(stream);
    return MAKE_I64((int64_t)n);
}


static Value fn_hash(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_I64((int64_t)pyro_hash_value(vm, args[0]));
}


static Value fn_sleep(PyroVM* vm, size_t arg_count, Value* args) {
    double time_in_seconds;

    if (IS_I64(args[0]) && args[0].as.i64 >= 0) {
        time_in_seconds = (double)args[0].as.i64;
    } else if (IS_F64(args[0]) && args[0].as.f64 >= 0) {
        time_in_seconds = args[0].as.f64;
    } else {
        pyro_panic(vm, "$sleep(): invalid argument [time_in_seconds], expected a positive number");
        return MAKE_NULL();
    }

    if (pyro_sleep(time_in_seconds) != 0) {
        pyro_panic(vm, "$sleep(): OS error");
    }

    return MAKE_NULL();
}


static Value fn_is_callable(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_CLOSURE(args[0])) {
        return MAKE_BOOL(true);
    } else if (IS_NATIVE_FN(args[0])) {
        return MAKE_BOOL(true);
    } else if (IS_CLASS(args[0])) {
        return MAKE_BOOL(true);
    } else if (IS_BOUND_METHOD(args[0])) {
        return MAKE_BOOL(true);
    } else if (IS_INSTANCE(args[0]) && pyro_has_method(vm, args[0], vm->str_dollar_call)) {
        return MAKE_BOOL(true);
    }
    return MAKE_BOOL(false);
}


static Value fn_is_class(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_CLASS(args[0])) {
        return MAKE_BOOL(true);
    }
    return MAKE_BOOL(false);
}


static Value fn_is_pyro_func(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_CLOSURE(args[0])) {
        return MAKE_BOOL(true);
    }
    return MAKE_BOOL(false);
}


static Value fn_is_native_func(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_NATIVE_FN(args[0])) {
        return MAKE_BOOL(true);
    }
    return MAKE_BOOL(false);
}


static Value fn_is_iterable(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_has_method(vm, args[0], vm->str_dollar_iter));
}


static Value fn_is_iterator(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_has_method(vm, args[0], vm->str_dollar_next));
}


static Value fn_env(PyroVM* vm, size_t arg_count, Value* args) {
    // Note: getenv() is part of the C standard library.
    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$env(): invalid argument [name], expected a string");
            return MAKE_NULL();
        }

        ObjStr* name = AS_STR(args[0]);
        char* value = getenv(name->bytes);
        if (!value) {
            return MAKE_OBJ(vm->error);
        }

        ObjStr* string = STR(value);
        if (!string) {
            pyro_panic(vm, "$env(): out of memory");
            return MAKE_NULL();
        }

        return MAKE_OBJ(string);
    }

    // Note: setenv() is a POSIX function, not part of the C standard library.
    if (arg_count == 2) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$env(): invalid argument [name], expected a string");
            return MAKE_NULL();
        }

        ObjStr* name = AS_STR(args[0]);
        ObjStr* value = pyro_stringify_value(vm, args[1]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }

        if (!pyro_setenv(name->bytes, value->bytes)) {
            pyro_panic(vm, "$env(): failed to set environment variable '%s'", name->bytes);
            return MAKE_NULL();
        }

        return MAKE_NULL();
    }

    pyro_panic(vm, "$env(): expected 1 or 2 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value fn_is_null(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_NULL(args[0]));
}


static Value fn_input(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = (ObjFile*)vm->stdin_stream;

    ObjStr* string = ObjFile_read_line(file, vm);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    return string ? MAKE_OBJ(string) : MAKE_NULL();
}


static Value fn_exec(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$exec(): invalid argument [code], expected a string");
        return MAKE_NULL();
    }
    ObjStr* code = AS_STR(args[0]);

    ObjModule* module = ObjModule_new(vm);
    if (!module) {
        pyro_panic(vm, "$exec(): out of memory");
        return MAKE_NULL();
    }

    // Push the module onto the stack to keep it safe from the garbage collector.
    if (!pyro_push(vm, MAKE_OBJ(module))) { return MAKE_NULL(); }
    pyro_exec_code_as_module(vm, code->bytes, code->length, "<exec>", module);
    pyro_pop(vm);

    return MAKE_OBJ(module);
}


static Value fn_type(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[0].type) {
        case VAL_BOOL:
            return MAKE_OBJ(vm->str_bool);
        case VAL_NULL:
            return MAKE_OBJ(vm->str_null);
        case VAL_I64:
            return MAKE_OBJ(vm->str_i64);
        case VAL_F64:
            return MAKE_OBJ(vm->str_f64);
        case VAL_CHAR:
            return MAKE_OBJ(vm->str_char);
        case VAL_OBJ: {
            switch (AS_OBJ(args[0])->type) {
                case OBJ_BOUND_METHOD:
                    return MAKE_OBJ(vm->str_method);
                case OBJ_BUF:
                    return MAKE_OBJ(vm->str_buf);
                case OBJ_CLASS:
                    return MAKE_OBJ(vm->str_class);
                case OBJ_INSTANCE: {
                    ObjStr* class_name = AS_OBJ(args[0])->class->name;
                    if (class_name) {
                        return MAKE_OBJ(class_name);
                    }
                    return MAKE_OBJ(vm->str_instance);
                }
                case OBJ_CLOSURE:
                case OBJ_FN:
                case OBJ_NATIVE_FN:
                    return MAKE_OBJ(vm->str_fn);
                case OBJ_FILE:
                    return MAKE_OBJ(vm->str_file);
                case OBJ_ITER:
                    return MAKE_OBJ(vm->str_iter);
                case OBJ_MAP:
                    return MAKE_OBJ(vm->str_map);
                case OBJ_MAP_AS_SET:
                    return MAKE_OBJ(vm->str_set);
                case OBJ_VEC:
                    return MAKE_OBJ(vm->str_vec);
                case OBJ_VEC_AS_STACK:
                    return MAKE_OBJ(vm->str_stack);
                case OBJ_QUEUE:
                    return MAKE_OBJ(vm->str_queue);
                case OBJ_STR:
                    return MAKE_OBJ(vm->str_str);
                case OBJ_MODULE:
                    return MAKE_OBJ(vm->str_module);
                case OBJ_TUP:
                    return MAKE_OBJ(vm->str_tup);
                case OBJ_ERR:
                    return MAKE_OBJ(vm->str_err);
                default:
                    return MAKE_OBJ(vm->empty_string);
            }
        }
        default:
            return MAKE_OBJ(vm->empty_string);
    }
}


static Value fn_method(PyroVM* vm, size_t arg_count, Value* args) {
    Value obj = args[0];

    if (!IS_STR(args[1])) {
        pyro_panic(vm, "$method(): invalid argument [method_name], expected a string");
        return MAKE_NULL();
    }
    ObjStr* method_name = AS_STR(args[1]);

    Value method = pyro_get_method(vm, obj, method_name);
    if (IS_NULL(method)) {
        return MAKE_OBJ(vm->error);
    }

    ObjBoundMethod* bound_method = ObjBoundMethod_new(vm, obj, AS_OBJ(method));
    if (!bound_method) {
        pyro_panic(vm, "$method(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(bound_method);
}


static Value fn_methods(PyroVM* vm, size_t arg_count, Value* args) {
    ObjClass* class = pyro_get_class(vm, args[0]);
    if (!class) {
        ObjIter* iter = ObjIter_empty(vm);
        if (!iter) {
            pyro_panic(vm, "$methods(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(iter);
    }

    ObjIter* iter = ObjIter_new((Obj*)class->pub_instance_methods, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "$methods(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(iter);
}


static Value fn_field(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, "$field(): invalid argument [field_name], expected a string");
        return MAKE_NULL();
    }
    Value field_name = args[1];

    if (IS_INSTANCE(args[0])) {
        ObjMap* field_index_map = AS_INSTANCE(args[0])->obj.class->all_field_indexes;
        Value field_index;
        if (ObjMap_get(field_index_map, field_name, &field_index, vm)) {
            return AS_INSTANCE(args[0])->fields[field_index.as.i64];
        }
    }

    return MAKE_OBJ(vm->error);
}


static Value fn_fields(PyroVM* vm, size_t arg_count, Value* args) {
    ObjClass* class = pyro_get_class(vm, args[0]);
    if (!class) {
        ObjIter* iter = ObjIter_empty(vm);
        if (!iter) {
            pyro_panic(vm, "$fields(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(iter);
    }

    ObjIter* iter = ObjIter_new((Obj*)class->pub_field_indexes, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "$fields(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(iter);
}


static Value fn_is_func(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_CLOSURE(args[0]) || IS_NATIVE_FN(args[0]));
}


static Value fn_is_method(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_BOUND_METHOD(args[0]));
}


static Value fn_stdout(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        if (vm->stdout_stream) {
            return MAKE_OBJ(vm->stdout_stream);
        }
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        if (!(IS_FILE(args[0]) || IS_BUF(args[0]))) {
            pyro_panic(vm, "$stdout(): invalid argument, expected a file");
            return MAKE_NULL();
        }
        vm->stdout_stream = AS_OBJ(args[0]);
        return MAKE_NULL();
    }

    pyro_panic(vm, "$stdout(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value fn_stderr(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        if (vm->stderr_stream) {
            return MAKE_OBJ(vm->stderr_stream);
        }
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        if (!(IS_FILE(args[0]) || IS_BUF(args[0]))) {
            pyro_panic(vm, "$stderr(): invalid argument, expected a file");
            return MAKE_NULL();
        }
        vm->stderr_stream = AS_OBJ(args[0]);
        return MAKE_NULL();
    }

    pyro_panic(vm, "$stderr(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value fn_stdin(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        if (vm->stdin_stream) {
            return MAKE_OBJ(vm->stdin_stream);
        }
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        if (!IS_FILE(args[0])) {
            pyro_panic(vm, "$stdin(): invalid argument, expected a file");
            return MAKE_NULL();
        }
        vm->stdin_stream = AS_OBJ(args[0]);
        return MAKE_NULL();
    }

    pyro_panic(vm, "$stdin(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


/* -------- */
/*  Public  */
/* -------- */


Value pyro_fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    return fn_fmt(vm, arg_count, args);
}


void pyro_load_std_core(PyroVM* vm) {
    pyro_define_module_1(vm, "$std");
    pyro_define_global(vm, "$roots", MAKE_OBJ(vm->import_roots));

    ObjTup* args = ObjTup_new(0, vm);
    if (args) {
        pyro_define_global(vm, "$args", MAKE_OBJ(args));
    }

    pyro_define_global_fn(vm, "$", fn_shell_shortcut, 1);
    pyro_define_global_fn(vm, "$bool", fn_bool, 1);
    pyro_define_global_fn(vm, "$clock", fn_clock, 0);
    pyro_define_global_fn(vm, "$debug", fn_debug, 1);
    pyro_define_global_fn(vm, "$env", fn_env, -1);
    pyro_define_global_fn(vm, "$eprint", fn_eprint, -1);
    pyro_define_global_fn(vm, "$eprintln", fn_eprintln, -1);
    pyro_define_global_fn(vm, "$exec", fn_exec, 1);
    pyro_define_global_fn(vm, "$exit", fn_exit, 1);
    pyro_define_global_fn(vm, "$f64", fn_f64, 1);
    pyro_define_global_fn(vm, "$field", fn_field, 2);
    pyro_define_global_fn(vm, "$fields", fn_fields, 1);
    pyro_define_global_fn(vm, "$fmt", fn_fmt, -1);
    pyro_define_global_fn(vm, "$has_field", fn_has_field, 2);
    pyro_define_global_fn(vm, "$has_method", fn_has_method, 2);
    pyro_define_global_fn(vm, "$hash", fn_hash, 1);
    pyro_define_global_fn(vm, "$i64", fn_i64, 1);
    pyro_define_global_fn(vm, "$input", fn_input, 0);
    pyro_define_global_fn(vm, "$is_bool", fn_is_bool, 1);
    pyro_define_global_fn(vm, "$is_callable", fn_is_callable, 1);
    pyro_define_global_fn(vm, "$is_class", fn_is_class, 1);
    pyro_define_global_fn(vm, "$is_f64", fn_is_f64, 1);
    pyro_define_global_fn(vm, "$is_i64", fn_is_i64, 1);
    pyro_define_global_fn(vm, "$is_inf", fn_is_inf, 1);
    pyro_define_global_fn(vm, "$is_instance", fn_is_instance, 1);
    pyro_define_global_fn(vm, "$is_instance_of", fn_is_instance_of, 2);
    pyro_define_global_fn(vm, "$is_iterable", fn_is_iterable, 1);
    pyro_define_global_fn(vm, "$is_iterator", fn_is_iterator, 1);
    pyro_define_global_fn(vm, "$is_mod", fn_is_mod, 1);
    pyro_define_global_fn(vm, "$is_nan", fn_is_nan, 1);
    pyro_define_global_fn(vm, "$is_native_func", fn_is_native_func, 1);
    pyro_define_global_fn(vm, "$is_null", fn_is_null, 1);
    pyro_define_global_fn(vm, "$is_obj", fn_is_obj, 1);
    pyro_define_global_fn(vm, "$is_pyro_func", fn_is_pyro_func, 1);
    pyro_define_global_fn(vm, "$method", fn_method, 2);
    pyro_define_global_fn(vm, "$methods", fn_methods, 1);
    pyro_define_global_fn(vm, "$panic", fn_panic, -1);
    pyro_define_global_fn(vm, "$print", fn_print, -1);
    pyro_define_global_fn(vm, "$println", fn_println, -1);
    pyro_define_global_fn(vm, "$read_file", fn_read_file, 1);
    pyro_define_global_fn(vm, "$shell", fn_shell, -1);
    pyro_define_global_fn(vm, "$sleep", fn_sleep, 1);
    pyro_define_global_fn(vm, "$type", fn_type, 1);
    pyro_define_global_fn(vm, "$write_file", fn_write_file, 2);
    pyro_define_global_fn(vm, "$is_func", fn_is_func, 1);
    pyro_define_global_fn(vm, "$is_method", fn_is_method, 1);
    pyro_define_global_fn(vm, "$stdin", fn_stdin, -1);
    pyro_define_global_fn(vm, "$stdout", fn_stdout, -1);
    pyro_define_global_fn(vm, "$stderr", fn_stderr, -1);

}
