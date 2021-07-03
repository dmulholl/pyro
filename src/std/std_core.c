#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"


// ---- //
// Maps //
// ---- //


static Value fn_map(PyroVM* vm, size_t arg_count, Value* args) {
    return OBJ_VAL(ObjMap_new(vm));
}


static Value fn_is_map(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_MAP(args[0]));
}


static Value map_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return I64_VAL(map->count - map->tombstone_count);
}


static Value map_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return BOOL_VAL(ObjMap_set(map, args[0], args[1], vm));
}


static Value map_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    if (ObjMap_get(map, args[0], &value)) {
        return value;
    }
    return OBJ_VAL(vm->empty_error);
}


static Value map_del(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return BOOL_VAL(ObjMap_remove(map, args[0]));
}


static Value map_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    return BOOL_VAL(ObjMap_get(map, args[0], &value));
}


static Value map_keys(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_KEYS, vm);
    return OBJ_VAL(iterator);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_VALUES, vm);
    return OBJ_VAL(iterator);
}


static Value map_entries(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_ENTRIES, vm);
    return OBJ_VAL(iterator);
}


static Value map_iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value map_iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMapIter* iterator = AS_MAP_ITER(args[-1]);
    return ObjMapIter_next(iterator, vm);
}


// ------- //
// Vectors //
// ------- //


static Value fn_vec(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        return OBJ_VAL(ObjVec_new(vm));
    } else if (arg_count == 2) {
        if (IS_I64(args[0]) && args[0].as.i64 > 0) {
            return OBJ_VAL(ObjVec_new_with_cap_and_fill((size_t)args[0].as.i64, args[1], vm));
        } else {
            pyro_panic(vm, "Invalid size argument for $vec().");
            return NULL_VAL();
        }
    } else {
        pyro_panic(vm, "Expected 0 or 2 arguments to $vec(), found %i.", arg_count);
        return NULL_VAL();
    }
}


static Value fn_is_vec(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_VEC(args[0]));
}


static Value vec_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return I64_VAL(vec->count);
}


static Value vec_append(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    ObjVec_append(vec, args[0], vm);
    return NULL_VAL();
}


static Value vec_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        return ObjVec_get(vec, args[0].as.i64, vm);
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value vec_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        ObjVec_set(vec, args[0].as.i64, args[1], vm);
        return args[1];
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value vec_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    ObjVecIter* iterator = ObjVecIter_new(vec, vm);
    return OBJ_VAL(iterator);
}


static Value vec_iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVecIter* iterator = AS_VEC_ITER(args[-1]);

    if (iterator->next_index < iterator->vec->count) {
        iterator->next_index++;
        return iterator->vec->values[iterator->next_index - 1];
    }

    return OBJ_VAL(vm->empty_error);
}


// ------ //
// Tuples //
// ------ //


static Value fn_tup(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = ObjTup_new(arg_count, vm);
    if (tup == NULL) {
        return OBJ_VAL(vm->empty_error);
    }
    memcpy(tup->values, (void*)args, sizeof(Value) * arg_count);
    return OBJ_VAL(tup);
}


static Value fn_is_tup(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_TUP(args[0]));
}


static Value tup_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    return I64_VAL(tup->count);
}


static Value tup_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < tup->count) {
            return tup->values[index];
        }
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index, must be an integer.");
    return NULL_VAL();
}


static Value tup_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    ObjTupIter* iterator = ObjTupIter_new(tup, vm);
    return OBJ_VAL(iterator);
}


static Value tup_iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTupIter* iterator = AS_TUP_ITER(args[-1]);

    if (iterator->next_index < iterator->tup->count) {
        iterator->next_index++;
        return iterator->tup->values[iterator->next_index - 1];
    }

    return OBJ_VAL(vm->empty_error);
}


// ------ //
// Errors //
// ------ //


static Value fn_err(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        return OBJ_VAL(vm->empty_error);
    }
    ObjTup* tup = ObjTup_new_err(arg_count, vm);
    if (tup == NULL) {
        return OBJ_VAL(vm->empty_error);
    }
    memcpy(tup->values, (void*)args, sizeof(Value) * arg_count);
    return OBJ_VAL(tup);
}


static Value fn_is_err(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_ERR(args[0]));
}


// ------- //
// Strings //
// ------- //


static Value fn_str(PyroVM* vm, size_t arg_count, Value* args) {
    return OBJ_VAL(pyro_stringify_value(vm, args[0], true));
}


static Value fn_is_str(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_STR(args[0]));
}


// Returns the number of bytes in the string.
static Value str_byte_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    return I64_VAL(str->length);
}


// Returns the byte value at index [i] as an integer in the range [0,256).
static Value str_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < str->length) {
            return I64_VAL((uint8_t)str->bytes[index]);
        }
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index type.");
    return NULL_VAL();
}


static Value str_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    ObjStrIter* iterator = ObjStrIter_new(str, STR_ITER_BYTES, vm);
    return OBJ_VAL(iterator);
}


static Value str_is_ascii(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    for (size_t i = 0; i < str->length; i++) {
        if (str->bytes[i] & 0x80) {
            return BOOL_VAL(false);
        }
    }

    return BOOL_VAL(true);
}


static Value str_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    ObjStrIter* iterator = ObjStrIter_new(str, STR_ITER_CHARS, vm);
    return OBJ_VAL(iterator);
}


static Value str_char(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "Invalid index type.");
        return NULL_VAL();
    }

    int64_t target_index = args[0].as.i64;
    if (target_index < 0 || str->length == 0) {
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (char_count < (size_t)target_index + 1) {
        if (byte_index == str->length) {
            pyro_panic(vm, "Index out of range.");
            return NULL_VAL();
        }

        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, "String contains invalid utf-8 at byte index %zu.", byte_index);
            return NULL_VAL();
        }
    }

    return CHAR_VAL(cp.codepoint);
}


static Value str_char_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    size_t byte_index = 0;
    size_t char_count = 0;
    Utf8CodePoint cp;

    while (byte_index < str->length) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            char_count++;
            byte_index += cp.length;
        } else {
            pyro_panic(vm, "String contains invalid utf-8 at byte index %zu.", byte_index);
            return NULL_VAL();
        }
    }

    return I64_VAL(char_count);
}


static Value str_is_utf8(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < str->length) {
        uint8_t* src = (uint8_t*)&str->bytes[byte_index];
        size_t src_len = str->length - byte_index;

        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            byte_index += cp.length;
        } else {
            return BOOL_VAL(false);
        }
    }

    return BOOL_VAL(true);
}


static Value str_iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value str_iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStrIter* iter = AS_STR_ITER(args[-1]);
    ObjStr* str = iter->string;

    if (iter->iter_type == STR_ITER_BYTES) {
        if (iter->next_index < str->length) {
            int64_t byte_value = (uint8_t)str->bytes[iter->next_index];
            iter->next_index++;
            return I64_VAL(byte_value);
        }
        return OBJ_VAL(vm->empty_error);
    }

    if (iter->next_index < str->length) {
        uint8_t* src = (uint8_t*)&str->bytes[iter->next_index];
        size_t src_len = str->length - iter->next_index;
        Utf8CodePoint cp;
        if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
            iter->next_index += cp.length;
            return CHAR_VAL(cp.codepoint);
        }
        pyro_panic(vm, "String contains invalid utf-8 at byte index %zu.", iter->next_index);
    }

    return OBJ_VAL(vm->empty_error);
}


static Value str_to_ascii_upper(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* bytes = ALLOCATE_ARRAY(vm, char, str->length + 1);
    memcpy(bytes, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (bytes[i] >= 'a' && bytes[i] <= 'z') {
            bytes[i] -= 32;
        }
    }

    return OBJ_VAL(ObjStr_take(bytes, str->length, vm));
}


static Value str_to_ascii_lower(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* bytes = ALLOCATE_ARRAY(vm, char, str->length + 1);
    memcpy(bytes, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (bytes[i] >= 'A' && bytes[i] <= 'Z') {
            bytes[i] += 32;
        }
    }

    return OBJ_VAL(ObjStr_take(bytes, str->length, vm));
}


// --------------------- //
// Formatting & Printing //
// --------------------- //


static Value fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "Expected 2 or more arguments for $fmt(), found %d.", arg_count);
        return OBJ_VAL(vm->empty_string);
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $fmt() should be a format string.");
        return OBJ_VAL(vm->empty_string);
    }

    ObjStr* fmt_str = AS_STR(args[0]);

    char fmt_spec_buffer[16];
    size_t fmt_spec_count = 0;

    char* out_buffer = ALLOCATE_ARRAY(vm, char, 8);
    size_t out_capacity = 8;
    size_t out_count = 0;

    size_t fmt_str_index = 0;
    size_t next_arg_index = 1;

    while (fmt_str_index < fmt_str->length) {
        if (out_count + 1 == out_capacity) {
            size_t new_capacity = out_capacity * 2;
            out_buffer = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, new_capacity);
            out_capacity = new_capacity;
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
                    pyro_panic(vm, "Too many characters in format specifier.");
                    FREE_ARRAY(vm, char, out_buffer, out_capacity);
                    return OBJ_VAL(vm->empty_string);
                }
                fmt_spec_buffer[fmt_spec_count++] = fmt_str->bytes[fmt_str_index++];
            }
            if (fmt_str_index == fmt_str->length) {
                pyro_panic(vm, "Missing '}' in format string.");
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                return OBJ_VAL(vm->empty_string);
            }
            fmt_str_index++;
            fmt_spec_buffer[fmt_spec_count] = '\0';

            if (next_arg_index == arg_count) {
                pyro_panic(vm, "Too few arguments for format string.");
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                return OBJ_VAL(vm->empty_string);
            }
            Value arg = args[next_arg_index++];

            ObjStr* formatted;
            if (fmt_spec_count == 0) {
                formatted = pyro_stringify_value(vm, arg, true);
            } else {
                formatted = pyro_format_value(vm, arg, fmt_spec_buffer);
            }
            if (vm->exit_flag || vm->panic_flag) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                return OBJ_VAL(vm->empty_string);
            }

            if (out_capacity - out_count - 1 < formatted->length) {
                int new_capacity = out_capacity + formatted->length + 1;
                pyro_push(vm, OBJ_VAL(formatted));
                out_buffer = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, new_capacity);
                out_capacity = new_capacity;
                pyro_pop(vm);
            }

            memcpy(&out_buffer[out_count], formatted->bytes, formatted->length);
            out_count += formatted->length;
            fmt_spec_count = 0;
            continue;
        }

        out_buffer[out_count++] = fmt_str->bytes[fmt_str_index++];
    }

    out_buffer = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, out_count + 1);
    out_buffer[out_count] = '\0';
    return OBJ_VAL(ObjStr_take(out_buffer, out_count, vm));
}


static Value fn_eprint(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "Expected 1 or more arguments for $eprint(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0], true);
        if (vm->exit_flag || vm->panic_flag) {
            return NULL_VAL();
        }
        pyro_err(vm, "%s", string->bytes);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $eprint() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->exit_flag || vm->panic_flag) {
        return NULL_VAL();
    }
    pyro_err(vm, "%s", AS_STR(formatted)->bytes);
    return NULL_VAL();
}


static Value fn_print(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "Expected 1 or more arguments for $print(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0], true);
        if (vm->exit_flag || vm->panic_flag) {
            return NULL_VAL();
        }
        pyro_out(vm, "%s", string->bytes);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $print() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->exit_flag || vm->panic_flag) {
        return NULL_VAL();
    }
    pyro_out(vm, "%s", AS_STR(formatted)->bytes);
    return NULL_VAL();
}


static Value fn_eprintln(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_err(vm, "\n");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0], true);
        if (vm->exit_flag || vm->panic_flag) {
            return NULL_VAL();
        }
        pyro_err(vm, "%s\n", string->bytes);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $eprintln() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->exit_flag || vm->panic_flag) {
        return NULL_VAL();
    }
    pyro_err(vm, "%s\n", AS_STR(formatted)->bytes);
    return NULL_VAL();
}


static Value fn_println(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_out(vm, "\n");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0], true);
        if (vm->exit_flag || vm->panic_flag) {
            return NULL_VAL();
        }
        pyro_out(vm, "%s\n", string->bytes);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $println() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->exit_flag || vm->panic_flag) {
        return NULL_VAL();
    }
    pyro_out(vm, "%s\n", AS_STR(formatted)->bytes);
    return NULL_VAL();
}


// ----------- //
// Miscellanea //
// ----------- //


static Value fn_exit(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0])) {
        pyro_panic(vm, "$exit() requires an integer argument.");
        return NULL_VAL();
    }
    vm->exit_flag = true;
    vm->halt_flag = true;
    vm->exit_code = args[0].as.i64;
    return NULL_VAL();
}


static Value fn_panic(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$panic() requires a string argument.");
        return NULL_VAL();
    }
    pyro_panic(vm, AS_STR(args[0])->bytes);
    return NULL_VAL();
}


static Value fn_is_mod(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_MOD(args[0]));
}


static Value fn_clock(PyroVM* vm, size_t arg_count, Value* args) {
    return F64_VAL((double)clock() / CLOCKS_PER_SEC);
}


static Value fn_is_nan(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_F64(args[0]) && isnan(args[0].as.f64));
}


static Value fn_is_inf(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_F64(args[0]) && isinf(args[0].as.f64));
}


static Value fn_f64(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[0].type) {
        case VAL_I64:
            return F64_VAL((double)args[0].as.i64);
        case VAL_F64:
            return args[0];
        default:
            pyro_panic(vm, "Invalid argument to $f64().");
            return NULL_VAL();
    }
}


static Value fn_is_f64(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_F64(args[0]));
}


static Value fn_i64(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[0].type) {
        case VAL_I64:
            return args[0];
        case VAL_F64: {
            // Check that the value is inside the range -2^63 to 2^63 - 1. (Note that both
            // double literals are powers of 2 so they're exactly representable.)
            if (args[0].as.f64 >= -9223372036854775808.0 && args[0].as.f64 < 9223372036854775808.0) {
                return I64_VAL((int64_t)args[0].as.f64);
            }
            pyro_panic(vm, "Floating-point value is out-of-range for $i64().");
            return NULL_VAL();
        }
        case VAL_CHAR:
            return I64_VAL((int64_t)args[0].as.u32);
        default:
            pyro_panic(vm, "Invalid argument to $i64().");
            return NULL_VAL();
    }
}


static Value fn_is_i64(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_I64(args[0]));
}


static Value fn_char(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        int64_t arg = args[0].as.i64;
        if (arg > 0 && arg <= UINT32_MAX) {
            return CHAR_VAL((uint32_t)arg);
        }
    }

    pyro_panic(vm, "Invalid argument to $char().");
    return NULL_VAL();
}


static Value fn_is_char(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_CHAR(args[0]));
}


// ------------ //
// Registration //
// ------------ //


void pyro_load_std_core(PyroVM* vm) {
    // Create the base standard library module.
    ObjModule* mod_std = pyro_define_module_1(vm, "$std");

    // Register [$std] as a global variable so it doesn't need to be explicitly imported.
    pyro_define_global(vm, "$std", OBJ_VAL(mod_std));

    ObjTup* tup = ObjTup_new(0, vm);
    if (tup) {
        pyro_push(vm, OBJ_VAL(tup));
        pyro_define_global(vm, "$args", OBJ_VAL(tup));
        pyro_pop(vm);
    }

    pyro_define_global_fn(vm, "$exit", fn_exit, 1);
    pyro_define_global_fn(vm, "$panic", fn_panic, 1);
    pyro_define_global_fn(vm, "$clock", fn_clock, 0);
    pyro_define_global_fn(vm, "$is_mod", fn_is_mod, 1);
    pyro_define_global_fn(vm, "$is_nan", fn_is_nan, 1);
    pyro_define_global_fn(vm, "$is_inf", fn_is_inf, 1);

    pyro_define_global_fn(vm, "$f64", fn_f64, 1);
    pyro_define_global_fn(vm, "$is_f64", fn_is_f64, 1);

    pyro_define_global_fn(vm, "$i64", fn_i64, 1);
    pyro_define_global_fn(vm, "$is_i64", fn_is_i64, 1);

    pyro_define_global_fn(vm, "$char", fn_char, 1);
    pyro_define_global_fn(vm, "$is_char", fn_is_char, 1);

    pyro_define_global_fn(vm, "$fmt", fn_fmt, -1);
    pyro_define_global_fn(vm, "$print", fn_print, -1);
    pyro_define_global_fn(vm, "$println", fn_println, -1);
    pyro_define_global_fn(vm, "$eprint", fn_eprint, -1);
    pyro_define_global_fn(vm, "$eprintln", fn_eprintln, -1);

    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);
    pyro_define_method(vm, vm->map_class, "count", map_count, 0);
    pyro_define_method(vm, vm->map_class, "get", map_get, 1);
    pyro_define_method(vm, vm->map_class, "$get_index", map_get, 1);
    pyro_define_method(vm, vm->map_class, "set", map_set, 2);
    pyro_define_method(vm, vm->map_class, "$set_index", map_set, 2);
    pyro_define_method(vm, vm->map_class, "del", map_del, 1);
    pyro_define_method(vm, vm->map_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->map_class, "keys", map_keys, 0);
    pyro_define_method(vm, vm->map_class, "values", map_values, 0);
    pyro_define_method(vm, vm->map_class, "entries", map_entries, 0);
    pyro_define_method(vm, vm->map_iter_class, "$iter", map_iter_iter, 0);
    pyro_define_method(vm, vm->map_iter_class, "$next", map_iter_next, 0);

    pyro_define_global_fn(vm, "$vec", fn_vec, -1);
    pyro_define_global_fn(vm, "$is_vec", fn_is_vec, 1);
    pyro_define_method(vm, vm->vec_class, "count", vec_count, 0);
    pyro_define_method(vm, vm->vec_class, "append", vec_append, 1);
    pyro_define_method(vm, vm->vec_class, "get", vec_get, 1);
    pyro_define_method(vm, vm->vec_class, "set", vec_set, 2);
    pyro_define_method(vm, vm->vec_class, "$get_index", vec_get, 1);
    pyro_define_method(vm, vm->vec_class, "$set_index", vec_set, 2);
    pyro_define_method(vm, vm->vec_class, "$iter", vec_iter, 0);
    pyro_define_method(vm, vm->vec_iter_class, "$next", vec_iter_next, 0);

    pyro_define_global_fn(vm, "$tup", fn_tup, -1);
    pyro_define_global_fn(vm, "$is_tup", fn_is_tup, 1);
    pyro_define_method(vm, vm->tup_class, "count", tup_count, 0);
    pyro_define_method(vm, vm->tup_class, "get", tup_get, 1);
    pyro_define_method(vm, vm->tup_class, "$get_index", tup_get, 1);
    pyro_define_method(vm, vm->tup_class, "$iter", tup_iter, 0);
    pyro_define_method(vm, vm->tup_iter_class, "$next", tup_iter_next, 0);

    pyro_define_global_fn(vm, "$str", fn_str, 1);
    pyro_define_global_fn(vm, "$is_str", fn_is_str, 1);
    pyro_define_method(vm, vm->str_class, "is_utf8", str_is_utf8, 0);
    pyro_define_method(vm, vm->str_class, "is_ascii", str_is_ascii, 0);
    pyro_define_method(vm, vm->str_class, "byte", str_byte, 1);
    pyro_define_method(vm, vm->str_class, "bytes", str_bytes, 0);
    pyro_define_method(vm, vm->str_class, "byte_count", str_byte_count, 0);
    pyro_define_method(vm, vm->str_class, "char", str_char, 1);
    pyro_define_method(vm, vm->str_class, "chars", str_chars, 0);
    pyro_define_method(vm, vm->str_class, "char_count", str_char_count, 0);
    pyro_define_method(vm, vm->str_iter_class, "$iter", str_iter_iter, 0);
    pyro_define_method(vm, vm->str_iter_class, "$next", str_iter_next, 0);
    pyro_define_method(vm, vm->str_class, "to_ascii_upper", str_to_ascii_upper, 0);
    pyro_define_method(vm, vm->str_class, "to_ascii_lower", str_to_ascii_lower, 0);

    pyro_define_global_fn(vm, "$err", fn_err, -1);
    pyro_define_global_fn(vm, "$is_err", fn_is_err, 1);
}

