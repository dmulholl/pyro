#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/setup.h"
#include "../vm/stringify.h"
#include "../vm/panics.h"
#include "../vm/os.h"
#include "../vm/io.h"
#include "../vm/exec.h"


static Value fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 2 or more arguments for $fmt(), found %d.", arg_count);
        return MAKE_NULL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to $fmt() should be a format string.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                    pyro_panic(vm, ERR_VALUE_ERROR, "Too many characters in format specifier.");
                    return MAKE_NULL();
                }
                fmt_spec_buffer[fmt_spec_count++] = fmt_str->bytes[fmt_str_index++];
            }
            if (fmt_str_index == fmt_str->length) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, ERR_VALUE_ERROR, "Missing '}' in format string.");
                return MAKE_NULL();
            }
            fmt_str_index++;
            fmt_spec_buffer[fmt_spec_count] = '\0';

            if (next_arg_index == arg_count) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, ERR_ARGS_ERROR, "Too few arguments for format string.");
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
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }

    return MAKE_OBJ(string);
}


static Value fn_eprint(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or more arguments for $eprint(), found 0.");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_write_stderr(vm, "%s", string->bytes);
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard error stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to $eprint() should be a format string.");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_write_stderr(vm, "%s", AS_STR(formatted)->bytes);
    if (result == -1) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard error stream.");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_eprintln(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        int64_t result = pyro_write_stderr(vm, "\n");
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard error stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_write_stderr(vm, "%s\n", string->bytes);
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard error stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to $eprintln() should be a format string.");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_write_stderr(vm, "%s\n", AS_STR(formatted)->bytes);
    if (result == -1) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard error stream.");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_print(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or more arguments for $print(), found 0.");
        return MAKE_NULL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_write_stdout(vm, "%s", string->bytes);
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to $print() should be a format string.");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_write_stdout(vm, "%s", AS_STR(formatted)->bytes);
    if (result == -1) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_println(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        int64_t result = pyro_write_stdout(vm, "\n");
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        int64_t result = pyro_write_stdout(vm, "%s\n", string->bytes);
        if (result == -1) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
            return MAKE_NULL();
        } else if (result == -2) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_I64(result);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to $println() should be a format string.");
        return MAKE_NULL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    int64_t result = pyro_write_stdout(vm, "%s\n", AS_STR(formatted)->bytes);
    if (result == -1) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
        return MAKE_NULL();
    } else if (result == -2) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    return MAKE_I64(result);
}


static Value fn_exit(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $exit(), expected an integer.");
        return MAKE_NULL();
    }
    vm->exit_flag = true;
    vm->halt_flag = true;
    vm->status_code = args[0].as.i64;
    return MAKE_NULL();
}


static Value fn_panic(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $panic(), expected a string error message.");
            return MAKE_NULL();
        }
        pyro_panic(vm, 1, AS_STR(args[0])->bytes);
        return MAKE_NULL();
    } else if (arg_count == 2) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $panic(), expected a string error message.");
            return MAKE_NULL();
        }
        if (!IS_I64(args[1])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $panic(), expected an integer error code.");
            return MAKE_NULL();
        }
        pyro_panic(vm, args[1].as.i64, AS_STR(args[0])->bytes);
        return MAKE_NULL();
    } else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or 2 arguments for $panic(), found %d.", arg_count);
        return MAKE_NULL();
    }
}


static Value fn_is_mod(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_MOD(args[0]));
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
                double value;
                ObjStr* string = AS_STR(args[0]);
                if (pyro_parse_string_as_float(string->bytes, string->length, &value)) {
                    return MAKE_F64(value);
                }
                pyro_panic(vm, ERR_VALUE_ERROR, "Unable to parse string argument to $f64().");
                return MAKE_NULL();
            }
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $i64().");
            return MAKE_NULL();
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $f64().");
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
            pyro_panic(vm, ERR_VALUE_ERROR, "Floating-point value is out-of-range.");
            return MAKE_NULL();
        }

        case VAL_OBJ: {
            if (IS_STR(args[0])) {
                int64_t value;
                ObjStr* string = AS_STR(args[0]);
                if (pyro_parse_string_as_int(string->bytes, string->length, &value)) {
                    return MAKE_I64(value);
                }
                pyro_panic(vm, ERR_VALUE_ERROR, "Unable to parse string argument to $i64().");
                return MAKE_NULL();
            }
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $i64().");
            return MAKE_NULL();
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $i64().");
            return MAKE_NULL();
    }
}


static Value fn_is_i64(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_I64(args[0]));
}


static Value fn_char(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        int64_t arg = args[0].as.i64;
        if (arg >= 0 && arg <= UINT32_MAX) {
            return MAKE_CHAR((uint32_t)arg);
        }
    }

    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $char().");
    return MAKE_NULL();
}


static Value fn_is_char(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_CHAR(args[0]));
}


static Value fn_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_is_truthy(args[0]));
}


static Value fn_is_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_BOOL(args[0]));
}


static Value fn_has_method(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $has_method(), method name must be a string.");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_has_method(vm, args[0], AS_STR(args[1])));
}


static Value fn_has_field(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $has_field(), field name must be a string.");
        return MAKE_NULL();
    }
    Value field_name = args[1];

    if (IS_INSTANCE(args[0])) {
        ObjMap* field_index_map = AS_INSTANCE(args[0])->obj.class->field_indexes;
        Value field_index;
        if (ObjMap_get(field_index_map, field_name, &field_index, vm)) {
            return MAKE_BOOL(true);
        }
    }

    return MAKE_BOOL(false);
}


static Value fn_is_instance(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_CLASS(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $is_instance(), class must be a class object.");
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


static Value fn_shell(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $shell(), expected a string.");
        return MAKE_NULL();
    }

    ShellCmdResult result;
    if (!pyro_run_shell_cmd(vm, AS_STR(args[0])->bytes, &result)) {
        // We've already panicked.
        return MAKE_NULL();
    }

    Value output_string = MAKE_OBJ(result.output);
    Value exit_code = MAKE_I64(result.exit_code);

    pyro_push(vm, output_string);
    ObjTup* tup = ObjTup_new(2, vm);
    pyro_pop(vm);
    if (!tup) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }

    tup->values[0] = exit_code;
    tup->values[1] = output_string;

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
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid path argument, must be a string.");
        return MAKE_NULL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "r");
    if (!stream) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        fclose(stream);
        return MAKE_NULL();
    }

    fclose(stream);
    return MAKE_OBJ(string);
}


static Value fn_write_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid path argument, must be a string.");
        return MAKE_NULL();
    }

    if (!IS_STR(args[1]) && !IS_BUF(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid content argument, must be a string or buffer.");
        return MAKE_NULL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "w");
    if (!stream) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
        return MAKE_NULL();
    }

    if (IS_BUF(args[1])) {
        ObjBuf* buf = AS_BUF(args[1]);
        size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, stream);
        if (n < buf->count) {
            pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
            fclose(stream);
            return MAKE_NULL();
        }
        fclose(stream);
        return MAKE_I64((int64_t)n);
    }

    ObjStr* string = AS_STR(args[1]);
    size_t n = fwrite(string->bytes, sizeof(char), string->length, stream);
    if (n < string->length) {
        pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
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
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $sleep(), expected a positive number.");
        return MAKE_NULL();
    }

    if (pyro_sleep(time_in_seconds) != 0) {
        pyro_panic(vm, ERR_OS_ERROR, "OS error triggered by call to $sleep().");
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
    } else if (IS_INSTANCE(args[0]) && pyro_has_method(vm, args[0], vm->str_call)) {
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


static Value fn_is_method(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_BOUND_METHOD(args[0])) {
        return MAKE_BOOL(true);
    }
    return MAKE_BOOL(false);
}


static Value fn_is_iterable(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_has_method(vm, args[0], vm->str_iter));
}


static Value fn_is_iterator(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_has_method(vm, args[0], vm->str_next));
}


static Value fn_env(PyroVM* vm, size_t arg_count, Value* args) {
    // Note: getenv() is part of the C standard library.
    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid 'name' argument to $env(), expected a string.");
            return MAKE_NULL();
        }

        ObjStr* name = AS_STR(args[0]);
        char* value = getenv(name->bytes);
        if (!value) {
            return MAKE_OBJ(vm->empty_error);
        }

        ObjStr* string = STR(value);
        if (!string) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }

        return MAKE_OBJ(string);
    }

    // Note: setenv() is a POSIX function, not part of the C standard library.
    if (arg_count == 2) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid 'name' argument to $env(), expected a string.");
            return MAKE_NULL();
        }

        ObjStr* name = AS_STR(args[0]);
        ObjStr* value = pyro_stringify_value(vm, args[1]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }

        if (!pyro_setenv(name->bytes, value->bytes)) {
            pyro_panic(vm, ERR_OS_ERROR, "Failed to set environment variable '%s'.", name->bytes);
            return MAKE_NULL();
        }

        return MAKE_NULL();
    }

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or 2 arguments for $env(), found %zu.", arg_count);
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
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $exec(), expected a string.");
        return MAKE_NULL();
    }
    ObjStr* code = AS_STR(args[0]);

    ObjModule* module = ObjModule_new(vm);
    if (!module) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }

    // Push the module onto the stack to keep it safe from the garbage collector.
    if (!pyro_push(vm, MAKE_OBJ(module))) { return MAKE_NULL(); }
    pyro_exec_code_as_module(vm, code->bytes, code->length, "<exec>", module);
    pyro_pop(vm);

    return MAKE_OBJ(module->globals);
}


/* -------- */
/*  Public  */
/* -------- */


Value pyro_fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    return fn_fmt(vm, arg_count, args);
}


void pyro_load_std_core(PyroVM* vm) {
    // Create the base standard library module, $std.
    ObjModule* mod_std = pyro_define_module_1(vm, "$std");

    // Register $std as a global variable so it doesn't need to be explicitly imported.
    if (mod_std) {
        pyro_define_global(vm, "$std", MAKE_OBJ(mod_std));
    }

    pyro_define_global(vm, "$roots", MAKE_OBJ(vm->import_roots));

    ObjTup* args = ObjTup_new(0, vm);
    if (args) {
        pyro_push(vm, MAKE_OBJ(args));
        pyro_define_global(vm, "$args", MAKE_OBJ(args));
        pyro_pop(vm);
    }

    pyro_define_global_fn(vm, "$exit", fn_exit, 1);
    pyro_define_global_fn(vm, "$panic", fn_panic, -1);
    pyro_define_global_fn(vm, "$clock", fn_clock, 0);
    pyro_define_global_fn(vm, "$is_mod", fn_is_mod, 1);
    pyro_define_global_fn(vm, "$is_nan", fn_is_nan, 1);
    pyro_define_global_fn(vm, "$is_inf", fn_is_inf, 1);
    pyro_define_global_fn(vm, "$is_null", fn_is_null, 1);
    pyro_define_global_fn(vm, "$has_method", fn_has_method, 2);
    pyro_define_global_fn(vm, "$has_field", fn_has_field, 2);
    pyro_define_global_fn(vm, "$is_instance", fn_is_instance, 2);
    pyro_define_global_fn(vm, "$shell", fn_shell, 1);
    pyro_define_global_fn(vm, "$debug", fn_debug, 1);
    pyro_define_global_fn(vm, "$read_file", fn_read_file, 1);
    pyro_define_global_fn(vm, "$write_file", fn_write_file, 2);
    pyro_define_global_fn(vm, "$f64", fn_f64, 1);
    pyro_define_global_fn(vm, "$is_f64", fn_is_f64, 1);
    pyro_define_global_fn(vm, "$i64", fn_i64, 1);
    pyro_define_global_fn(vm, "$is_i64", fn_is_i64, 1);
    pyro_define_global_fn(vm, "$char", fn_char, 1);
    pyro_define_global_fn(vm, "$is_char", fn_is_char, 1);
    pyro_define_global_fn(vm, "$bool", fn_bool, 1);
    pyro_define_global_fn(vm, "$is_bool", fn_is_bool, 1);
    pyro_define_global_fn(vm, "$fmt", fn_fmt, -1);
    pyro_define_global_fn(vm, "$print", fn_print, -1);
    pyro_define_global_fn(vm, "$println", fn_println, -1);
    pyro_define_global_fn(vm, "$eprint", fn_eprint, -1);
    pyro_define_global_fn(vm, "$eprintln", fn_eprintln, -1);
    pyro_define_global_fn(vm, "$hash", fn_hash, 1);
    pyro_define_global_fn(vm, "$sleep", fn_sleep, 1);
    pyro_define_global_fn(vm, "$is_callable", fn_is_callable, 1);
    pyro_define_global_fn(vm, "$is_class", fn_is_class, 1);
    pyro_define_global_fn(vm, "$is_pyro_func", fn_is_pyro_func, 1);
    pyro_define_global_fn(vm, "$is_native_func", fn_is_native_func, 1);
    pyro_define_global_fn(vm, "$is_method", fn_is_method, 1);
    pyro_define_global_fn(vm, "$is_iterable", fn_is_iterable, 1);
    pyro_define_global_fn(vm, "$is_iterator", fn_is_iterator, 1);
    pyro_define_global_fn(vm, "$env", fn_env, -1);
    pyro_define_global_fn(vm, "$input", fn_input, 0);
    pyro_define_global_fn(vm, "$exec", fn_exec, 1);
}
