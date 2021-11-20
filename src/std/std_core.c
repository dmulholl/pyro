#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/os.h"


// --------------------- //
// Formatting & Printing //
// --------------------- //


static Value fn_fmt(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "Expected 2 or more arguments for $fmt(), found %d.", arg_count);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $fmt() should be a format string.");
        return NULL_VAL();
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
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
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
                    pyro_panic(vm, "Too many characters in format specifier.");
                    return NULL_VAL();
                }
                fmt_spec_buffer[fmt_spec_count++] = fmt_str->bytes[fmt_str_index++];
            }
            if (fmt_str_index == fmt_str->length) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "Missing '}' in format string.");
                return NULL_VAL();
            }
            fmt_str_index++;
            fmt_spec_buffer[fmt_spec_count] = '\0';

            if (next_arg_index == arg_count) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "Too few arguments for format string.");
                return NULL_VAL();
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
                return NULL_VAL();
            }

            if (!formatted) {
                FREE_ARRAY(vm, char, out_buffer, out_capacity);
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }

            if (out_count + formatted->length + 1 > out_capacity) {
                size_t new_capacity = out_count + formatted->length + 1;

                pyro_push(vm, OBJ_VAL(formatted));
                char* new_array = REALLOCATE_ARRAY(vm, char, out_buffer, out_capacity, new_capacity);
                pyro_pop(vm);

                if (!new_array) {
                    FREE_ARRAY(vm, char, out_buffer, out_capacity);
                    pyro_panic(vm, "Out of memory.");
                    return NULL_VAL();
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
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(string);
}


static Value fn_eprint(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        pyro_panic(vm, "Expected 1 or more arguments for $eprint(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (!string) {
            pyro_panic(vm, "Failed to format output string, insufficient memory.");
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
    if (vm->halt_flag) {
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
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (!string) {
            pyro_panic(vm, "Failed to format output string, insufficient memory.");
            return NULL_VAL();
        }
        pyro_err(vm, "%s", string->bytes);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to $print() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
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
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (!string) {
            pyro_panic(vm, "Failed to format output string, insufficient memory.");
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
    if (vm->halt_flag) {
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
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (!string) {
            pyro_panic(vm, "Failed to format output string, insufficient memory.");
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
    if (vm->halt_flag) {
        return NULL_VAL();
    }

    pyro_out(vm, "%s\n", AS_STR(formatted)->bytes);
    return NULL_VAL();
}


// ---- //
// Maps //
// ---- //


static Value fn_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(map);
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
    if (!ObjMap_set(map, args[0], args[1], vm)) {
        pyro_panic(vm, "Out of memory.");
    }
    return NULL_VAL();
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


static Value map_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap* copy = ObjMap_copy(map, vm);
    if (!copy) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(copy);
}


static Value map_keys(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_KEYS, vm);
    if (!iterator) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iterator);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_VALUES, vm);
    if (!iterator) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iterator);
}


static Value map_entries(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_ENTRIES, vm);
    if (!iterator) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
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
        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(vec);
    }

    else if (arg_count == 1) {
        Value iter_method = pyro_get_method(args[0], vm->str_iter);
        if (IS_NULL(iter_method)) {
            pyro_panic(vm, "Argument to $vec() is not iterable.");
            return NULL_VAL();
        }

        pyro_push(vm, args[0]);
        Value iterator = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        pyro_push(vm, iterator); // keep safe from the GC

        Value next_method = pyro_get_method(iterator, vm->str_next);
        if (IS_NULL(next_method)) {
            pyro_panic(vm, "Invalid iterator -- no :$next() method.");
            return NULL_VAL();
        }
        pyro_push(vm, next_method); // keep safe from the GC

        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_pop(vm); // next_method
            pyro_pop(vm); // iterator
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
        pyro_push(vm, OBJ_VAL(vec)); // keep safe from the GC

        while (true) {
            pyro_push(vm, iterator);
            Value next_value = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return NULL_VAL();
            }

            if (IS_ERR(next_value)) {
                break;
            }

            pyro_push(vm, next_value);
            if (!ObjVec_append(vec, next_value, vm)) {
                pyro_pop(vm); // next_value
                pyro_pop(vm); // vec
                pyro_pop(vm); // next_method
                pyro_pop(vm); // iterator
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm); // next_value
        }

        pyro_pop(vm); // vec
        pyro_pop(vm); // next_method
        pyro_pop(vm); // iterator
        return OBJ_VAL(vec);
    }

    else if (arg_count == 2) {
        if (IS_I64(args[0]) && args[0].as.i64 >= 0) {
            ObjVec* vec = ObjVec_new_with_cap_and_fill((size_t)args[0].as.i64, args[1], vm);
            if (!vec) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            return OBJ_VAL(vec);
        } else {
            pyro_panic(vm, "Invalid size argument for $vec().");
            return NULL_VAL();
        }
    }

    else {
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
    if (!ObjVec_append(vec, args[0], vm)) {
        pyro_panic(vm, "Out of memory.");
    }
    return NULL_VAL();
}


static Value vec_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < vec->count) {
            return vec->values[index];
        }
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value vec_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < vec->count) {
            vec->values[index] = args[1];
            return args[1];
        }
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value vec_reverse(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (vec->count < 2) {
        return OBJ_VAL(vec);
    }

    Value* low = &vec->values[0];
    Value* high = &vec->values[vec->count - 1];

    while (low < high) {
        Value temp = *low;
        *low = *high;
        *high = temp;
        low++;
        high--;
    }

    return OBJ_VAL(vec);
}


static Value vec_sort(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    Value compare_func;
    if (arg_count == 0) {
        compare_func = NULL_VAL();
    } else if (arg_count == 1) {
        compare_func = args[0];
    } else {
        pyro_panic(vm, "Expected 0 or 1 arguments to :sort(), found %d.", arg_count);
        return NULL_VAL();
    }

    if (!ObjVec_mergesort(vec, compare_func, vm)) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(vec);
}


static Value vec_shuffle(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    // Fisher-Yates/Durstenfeld algorithm: iterate over the array and at each index choose
    // randomly from the remaining unshuffled entries.
    for (size_t i = 0; i < vec->count; i++) {
        // Choose [j] from the half-open interval [i, vec->count).
        size_t j = i + pyro_mt64_gen_int(vm->mt64, vec->count - i);
        Value temp = vec->values[i];
        vec->values[i] = vec->values[j];
        vec->values[j] = temp;
    }

    return OBJ_VAL(vec);
}


static Value vec_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    ObjVec* copy = ObjVec_copy(vec, vm);
    if (!copy) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(vec);
}


static Value vec_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_check_equal(vec->values[i], args[0])) {
            return BOOL_VAL(true);
        }
    }

    return BOOL_VAL(false);
}


static Value vec_index_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_check_equal(vec->values[i], args[0])) {
            return I64_VAL((int64_t)i);
        }
    }

    return OBJ_VAL(vm->empty_error);
}


static Value vec_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjVec* new_vec = ObjVec_new_with_cap(vec->count, vm);
    if (!vec) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_vec));

    for (size_t i = 0; i < vec->count; i++) {
        pyro_push(vm, args[0]); // push the map function
        pyro_push(vm, vec->values[i]); // push the argument for the map function
        Value result = pyro_call_fn(vm, args[0], 1);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        new_vec->values[i] = result;
        new_vec->count++;
    }

    pyro_pop(vm);
    return OBJ_VAL(new_vec);
}


static Value vec_filter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjVec* new_vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_vec));

    for (size_t i = 0; i < vec->count; i++) {
        pyro_push(vm, args[0]); // push the filter function
        pyro_push(vm, vec->values[i]); // push the argument for the filter function
        Value result = pyro_call_fn(vm, args[0], 1);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (pyro_is_truthy(result)) {
            if (!ObjVec_append(new_vec, vec->values[i], vm)) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
        }
    }

    pyro_pop(vm);
    return OBJ_VAL(new_vec);
}


static Value vec_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    ObjVecIter* iterator = ObjVecIter_new(vec, vm);
    if (!iterator) {
        return NULL_VAL();
    }
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
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
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
    if (!iterator) {
        return NULL_VAL();
    }
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
    if (!tup) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
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
    ObjStr* string = pyro_stringify_value(vm, args[0]);
    if (!string) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(string);
}


static Value fn_is_str(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_STR(args[0]));
}


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
    if (!iterator) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
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
    if (!iterator) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
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

    return CHAR_VAL(cp.value);
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
            return CHAR_VAL(cp.value);
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

    char* array = ALLOCATE_ARRAY(vm, char, str->length + 1);
    if (!array) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    memcpy(array, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (array[i] >= 'a' && array[i] <= 'z') {
            array[i] -= 32;
        }
    }

    ObjStr* new_string = ObjStr_take(array, str->length, vm);
    if (!new_string) {
        FREE_ARRAY(vm, char, array, str->length + 1);
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_string);
}


static Value str_to_ascii_lower(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* array = ALLOCATE_ARRAY(vm, char, str->length + 1);
    if (!array) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    memcpy(array, str->bytes, str->length + 1);

    for (size_t i = 0; i < str->length; i++) {
        if (array[i] >= 'A' && array[i] <= 'Z') {
            array[i] += 32;
        }
    }

    ObjStr* new_string = ObjStr_take(array, str->length, vm);
    if (!new_string) {
        FREE_ARRAY(vm, char, array, str->length + 1);
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_string);
}


static Value str_starts_with(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :starts_with().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(str->bytes, target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_ends_with(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :ends_with().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(&str->bytes[str->length - target->length], target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_strip_prefix(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_prefix().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return OBJ_VAL(str);
    }

    if (memcmp(str->bytes, target->bytes, target->length) == 0) {
        ObjStr* new_str = ObjStr_copy_raw(&str->bytes[target->length], str->length - target->length, vm);
        if (!new_str) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(new_str);
    }

    return OBJ_VAL(str);
}


static Value str_strip_suffix(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_suffix().");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return OBJ_VAL(str);
    }

    if (memcmp(&str->bytes[str->length - target->length], target->bytes, target->length) == 0) {
        ObjStr* new_str = ObjStr_copy_raw(str->bytes, str->length - target->length, vm);
        if (!new_str) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(new_str);
    }

    return OBJ_VAL(str);
}


static Value str_strip_prefix_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_prefix_bytes().");
        return NULL_VAL();
    }

    ObjStr* prefix = AS_STR(args[0]);

    if (prefix->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(prefix->bytes, c, prefix->length) == NULL) {
            break;
        }
        start++;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_suffix_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_suffix_bytes().");
        return NULL_VAL();
    }

    ObjStr* suffix = AS_STR(args[0]);

    if (suffix->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *(end - 1);
        if (memchr(suffix->bytes, c, suffix->length) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_bytes(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(target->bytes, c, target->length) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(target->bytes, c, target->length) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_ascii_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        end--;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_utf8_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_is_unicode_whitespace(cp.value)) {
            break;
        }
        start += cp.length;
    }

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_is_unicode_whitespace(cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, "Argument to :strip_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        start += cp.length;
    }

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_suffix_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_suffix_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, "Argument to :strip_suffix_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_trailing_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        end -= cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip_prefix_chars(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :strip_prefix_chars(), expected a string.");
        return NULL_VAL();
    }

    ObjStr* target = AS_STR(args[0]);

    if (!pyro_is_valid_utf8(target->bytes, target->length)) {
        pyro_panic(vm, "Argument to :strip_prefix_chars() is not valid UTF-8.");
        return NULL_VAL();
    }

    if (target->length == 0 || str->length == 0) {
        return OBJ_VAL(str);
    }

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    Utf8CodePoint cp;

    while (start < end) {
        if (!pyro_read_utf8_codepoint((uint8_t*)start, end - start, &cp)) {
            break;
        }
        if (!pyro_contains_utf8_codepoint(target->bytes, target->length, cp.value)) {
            break;
        }
        start += cp.length;
    }

    ObjStr* new_str = ObjStr_copy_raw(start, end - start, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_strip(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        return str_strip_ascii_ws(vm, arg_count, args);
    } else if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "Invalid argument to :strip(), expected a string.");
            return NULL_VAL();
        }
        return str_strip_bytes(vm, arg_count, args);
    } else {
        pyro_panic(vm, "Expected 0 or 1 arguments for :strip(), found %d.", arg_count);
        return NULL_VAL();
    }
}


static Value str_match(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :match().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (!IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, "Invalid argument to :match().");
        return NULL_VAL();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target->length > str->length) {
        return BOOL_VAL(false);
    }

    if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
        return BOOL_VAL(true);
    }

    return BOOL_VAL(false);
}


static Value str_replace(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :replace().");
        return NULL_VAL();
    }
    ObjStr* old = AS_STR(args[0]);

    if (!IS_STR(args[1])) {
        pyro_panic(vm, "Invalid argument to :replace().");
        return NULL_VAL();
    }
    ObjStr* new = AS_STR(args[1]);

    if (old->length == 0 || old->length > str->length) {
        return OBJ_VAL(str);
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(buf)); // keep the buffer safe from the GC

    size_t index = 0;
    size_t last_possible_match_index = str->length - old->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], old->bytes, old->length) == 0) {
            if (!ObjBuf_append_bytes(buf, new->length, (uint8_t*)new->bytes, vm)) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            index += old->length;
        } else {
            if (!ObjBuf_append_byte(buf, str->bytes[index], vm)) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            index++;
        }
    }

    if (index < str->length) {
        if (!ObjBuf_append_bytes(buf, str->length - index, (uint8_t*)&str->bytes[index], vm)) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
    }

    ObjStr* new_str = ObjBuf_to_str(buf, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    pyro_pop(vm); // pop the buffer
    return OBJ_VAL(new_str);
}


static Value str_substr(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_I64(args[0]) && args[0].as.i64 > 0) {
        pyro_panic(vm, "Invalid argument to :substr().");
        return NULL_VAL();
    }
    size_t index = (size_t)args[0].as.i64;

    if (!IS_I64(args[1]) && args[1].as.i64 > 0) {
        pyro_panic(vm, "Invalid argument to :substr().");
        return NULL_VAL();
    }
    size_t length = (size_t)args[1].as.i64;

    if (index + length > str->length) {
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }

    ObjStr* new_str = ObjStr_copy_raw(&str->bytes[index], length, vm);
    if (!new_str) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(new_str);
}


static Value str_index_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :index_of().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (!IS_I64(args[1]) || args[1].as.i64 < 0) {
        pyro_panic(vm, "Invalid argument to :index_of().");
        return NULL_VAL();
    }
    size_t index = (size_t)args[1].as.i64;

    if (index + target->length > str->length) {
        return OBJ_VAL(vm->empty_error);
    }

    size_t last_possible_match_index = str->length - target->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
            return I64_VAL((int64_t)index);
        }
        index++;
    }

    return OBJ_VAL(vm->empty_error);
}


static Value str_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :contains().");
        return NULL_VAL();
    }
    ObjStr* target = AS_STR(args[0]);

    if (str->length < target->length) {
        return BOOL_VAL(false);
    }

    size_t index = 0;
    size_t last_possible_match_index = str->length - target->length;

    while (index <= last_possible_match_index) {
        if (memcmp(&str->bytes[index], target->bytes, target->length) == 0) {
            return BOOL_VAL(true);
        }
        index++;
    }

    return BOOL_VAL(false);
}


static Value str_split(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to :split().");
        return NULL_VAL();
    }
    ObjStr* sep = AS_STR(args[0]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(vec));

    if (str->length < sep->length || sep->length == 0) {
        if (!ObjVec_append(vec, OBJ_VAL(str), vm)) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
        pyro_pop(vm);
        return OBJ_VAL(vec);
    }

    size_t start = 0;
    size_t current = 0;
    size_t last_possible_match_index = str->length - sep->length;

    while (current <= last_possible_match_index) {
        if (memcmp(&str->bytes[current], sep->bytes, sep->length) == 0) {
            ObjStr* new_string = ObjStr_copy_raw(&str->bytes[start], current - start, vm);
            if (!new_string) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            pyro_push(vm, OBJ_VAL(new_string));
            if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm);
            current += sep->length;
            start = current;
        } else {
            current++;
        }
    }

    ObjStr* new_string = ObjStr_copy_raw(&str->bytes[start], str->length - start, vm);
    if (!new_string) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_string));
    if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_pop(vm);

    pyro_pop(vm); // pop the vector
    return OBJ_VAL(vec);
}


static Value str_split_on_ascii_ws(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(vec));

    const char* whitespace = " \t\r\n\v\f";

    char* start = str->bytes;
    char* end = str->bytes + str->length;

    while (start < end) {
        char c = *start;
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        start++;
    }

    while (start < end) {
        char c = *(end - 1);
        if (memchr(whitespace, c, 6) == NULL) {
            break;
        }
        end--;
    }

    char* current = start;

    while (current < end) {
        if (memchr(whitespace, *current, 6) != NULL) {
            ObjStr* new_string = ObjStr_copy_raw(start, current - start, vm);
            if (!new_string) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            pyro_push(vm, OBJ_VAL(new_string));
            if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
                pyro_panic(vm, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm);
            while (memchr(whitespace, *current, 6) != NULL) {
                current++;
            }
            start = current;
        } else {
            current++;
        }
    }

    ObjStr* new_string = ObjStr_copy_raw(start, current - start, vm);
    if (!new_string) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(new_string));
    if (!ObjVec_append(vec, OBJ_VAL(new_string), vm)) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_pop(vm);

    pyro_pop(vm); // pop the vector
    return OBJ_VAL(vec);
}


static Value str_to_hex(PyroVM* vm, size_t arg_count, Value* args) {
    ObjStr* str = AS_STR(args[-1]);
    if (str->length == 0) {
        return OBJ_VAL(str);
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(buf));

    for (size_t i = 0; i < str->length; i++) {
        if (!ObjBuf_append_hex_escaped_byte(buf, str->bytes[i], vm)) {
            pyro_panic(vm, "Out of memory.");
            return NULL_VAL();
        }
    }

    ObjStr* output_string = ObjBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    pyro_pop(vm);
    return OBJ_VAL(output_string);
}


// ------ //
// Ranges //
// ------ //


static Value fn_range(PyroVM* vm, size_t arg_count, Value* args) {
    int64_t start, stop, step;

    if (arg_count == 1) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "Invalid argument to $range().");
        }
        start = 0;
        stop = args[0].as.i64;
        step = 1;
    } else if (arg_count == 2) {
        if (!IS_I64(args[0]) || !IS_I64(args[1])) {
            pyro_panic(vm, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = 1;
    } else if (arg_count == 3) {
        if (!IS_I64(args[0]) || !IS_I64(args[1]) || !IS_I64(args[2])) {
            pyro_panic(vm, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = args[2].as.i64;
    } else {
        pyro_panic(vm, "Expected 1, 2, or 3 arguments for $range().");
        return NULL_VAL();
    }

    ObjRange* range = ObjRange_new(start, stop, step, vm);
    if (!range) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(range);
}


static Value fn_is_range(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_RANGE(args[0]));
}


static Value range_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value range_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjRange* range = AS_RANGE(args[-1]);

    if (range->step > 0) {
        if (range->next < range->stop) {
            int64_t next = range->next;
            range->next += range->step;
            return I64_VAL(next);
        }
        return OBJ_VAL(vm->empty_error);
    }

    if (range->step < 0) {
        if (range->next > range->stop) {
            int64_t next = range->next;
            range->next += range->step;
            return I64_VAL(next);
        }
        return OBJ_VAL(vm->empty_error);
    }

    return OBJ_VAL(vm->empty_error);
}


/* ------- */
/* Buffers */
/* ------- */


static Value fn_buf(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "Out of memory.");
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
        pyro_panic(vm, "Invalid argument to :write_byte().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > 255) {
        pyro_panic(vm, "Out-of-range argument (%d) to :write_byte().", value);
        return NULL_VAL();
    }

    if (!ObjBuf_append_byte(buf, value, vm)) {
        pyro_panic(vm, "Out of memory.");
    }

    return NULL_VAL();
}


static Value buf_write_be_u16(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "Invalid argument to :write_be_u16().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, "Out-of-range argument (%d) to :write_be_u16().", value);
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
        pyro_panic(vm, "Invalid argument to :write_be_u16().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > UINT16_MAX) {
        pyro_panic(vm, "Out-of-range argument (%d) to :write_be_u16().", value);
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
        pyro_panic(vm, "Out of memory.");
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
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
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
                pyro_panic(vm, "Byte value out of range.");
                return NULL_VAL();
            }
            pyro_panic(vm, "Invalid byte value type, expected an integer.");
            return NULL_VAL();
        }
        pyro_panic(vm, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, "Invalid index type, expected an integer.");
    return NULL_VAL();
}


static Value buf_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjBuf* buf = AS_BUF(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, "Expected 1 or more arguments for :write(), found 0.");
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
            pyro_panic(vm, "Out of memory.");
        }
        pyro_pop(vm);
        return NULL_VAL();
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to :write() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    ObjStr* string = AS_STR(formatted);

    pyro_push(vm, formatted);
    if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
        pyro_panic(vm, "Out of memory.");
    }
    pyro_pop(vm);

    return NULL_VAL();
}


/* ------- */
/*  Files  */
/* ------- */


static Value fn_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid filename argument, must be a string.");
        return NULL_VAL();
    }

    if (!IS_STR(args[1])) {
        pyro_panic(vm, "Invalid mode argument, must be a string.");
        return NULL_VAL();
    }

    ObjFile* file = ObjFile_new(vm);
    if (!file) {
        pyro_panic(vm, "Failed to allocate memory for file object.");
        return NULL_VAL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, AS_STR(args[1])->bytes);
    if (!stream) {
        pyro_panic(vm, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
        return NULL_VAL();
    }

    file->stream = stream;
    return OBJ_VAL(file);
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_FILE(args[0]));
}


static Value file_flush(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fflush(file->stream);
    }

    return NULL_VAL();
}


static Value file_close(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fclose(file->stream);
        file->stream = NULL;
    }

    return NULL_VAL();
}


static Value file_read(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "Insufficient memory available to read file.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            break;
        }

        array[count++] = c;
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "Insufficient memory available to read file.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    buf->count = count;
    buf->capacity = capacity;
    buf->bytes = array;

    return OBJ_VAL(buf);
}


static Value file_read_string(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "Insufficient memory available to read file.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
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
        pyro_panic(vm, "Insufficient memory available to read file.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    return OBJ_VAL(string);
}


// Attempts to read [n] bytes from the file into a byte buffer. May read less than [n] bytes if the
// end of the file is reached first. Returns the byte buffer or [NULL] if the end of the file had
// already been reached before the method was called. Panics if an I/O read error occurs, if the
// argument is invalid, or if memory cannot be allocated for the buffer.
static Value file_read_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "Invalid argument to :read_bytes(), must be a non-negative integer.");
        return NULL_VAL();
    }

    if (feof(file->stream)) {
        return NULL_VAL();
    }

    size_t num_bytes_to_read = args[0].as.i64;

    ObjBuf* buf = ObjBuf_new_with_cap(num_bytes_to_read, vm);
    if (!buf) {
        pyro_panic(vm, "Failed to allocate memory for buffer.");
        return NULL_VAL();
    }

    if (num_bytes_to_read == 0) {
        return OBJ_VAL(buf);
    }

    size_t n = fread(buf->bytes, sizeof(uint8_t), num_bytes_to_read, file->stream);
    buf->count = n;

    if (n < num_bytes_to_read) {
        if (ferror(file->stream)) {
            pyro_panic(vm, "I/O read error.");
            return NULL_VAL();
        }
        if (n == 0) {
            return NULL_VAL();
        }
    }

    return OBJ_VAL(buf);
}



static Value file_read_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (feof(file->stream)) {
        return NULL_VAL();
    }

    uint8_t byte;
    size_t n = fread(&byte, sizeof(uint8_t), 1, file->stream);

    if (n < 1) {
        if (ferror(file->stream)) {
            pyro_panic(vm, "I/O read error.");
            return NULL_VAL();
        }
        if (n == 0) {
            return NULL_VAL();
        }
    }

    return I64_VAL(byte);
}


static Value file_read_line(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, "Failed to allocate memory for string.");
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, "I/O read error.");
                return NULL_VAL();
            }
            break;
        }

        array[count++] = c;

        if (c == '\n') {
            break;
        }
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    while (count > 0 && (array[count - 1] == '\n' || array[count - 1] == '\r')) {
        count--;
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return OBJ_VAL(vm->empty_string);
    }

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }

    array[count] = '\0';

    ObjStr* string = ObjStr_take((char*)array, count, vm);
    if (!string) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        pyro_panic(vm, "Failed to allocate memory for string.");
        return NULL_VAL();
    }

    return OBJ_VAL(string);
}


static Value file_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, "Expected 1 or more arguments for :write(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        if (IS_BUF(args[0])) {
            ObjBuf* buf = AS_BUF(args[0]);
            size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, file->stream);
            if (n < buf->count) {
                pyro_panic(vm, "I/O write error.");
                return NULL_VAL();
            }
            return I64_VAL((int64_t)n);
        } else {
            ObjStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return NULL_VAL();
            }
            if (!string) {
                pyro_panic(vm, "Failed to allocate memory for string.");
                return NULL_VAL();
            }
            size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);
            if (n < string->length) {
                pyro_panic(vm, "I/O write error.");
                return NULL_VAL();
            }
            return I64_VAL((int64_t)n);
        }
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "First argument to :write() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    ObjStr* string = AS_STR(formatted);

    size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);

    if (n < string->length) {
        pyro_panic(vm, "I/O write error.");
        return NULL_VAL();
    }
    return I64_VAL((int64_t)n);
}


static Value file_write_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "Invalid argument type for :write_byte().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > 255) {
        pyro_panic(vm, "Argument to :write_byte() is out of range.");
        return NULL_VAL();
    }

    uint8_t byte = (uint8_t)value;
    size_t n = fwrite(&byte, sizeof(uint8_t), 1, file->stream);
    if (n < 1) {
        pyro_panic(vm, "I/O write error.");
        return NULL_VAL();
    }

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
        case VAL_CHAR:
            return F64_VAL((double)args[0].as.u32);
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
            // double literals below are powers of 2 so they're exactly representable.)
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
        if (arg >= 0 && arg <= UINT32_MAX) {
            return CHAR_VAL((uint32_t)arg);
        }
    }

    pyro_panic(vm, "Invalid argument to $char().");
    return NULL_VAL();
}


static Value fn_is_char(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_CHAR(args[0]));
}


static Value fn_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(pyro_is_truthy(args[0]));
}


static Value fn_is_bool(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_BOOL(args[0]));
}


static Value fn_has_method(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, "Invalid argument to $has_method(), method name must be a string.");
        return NULL_VAL();
    }
    return BOOL_VAL(pyro_has_method(args[0], AS_STR(args[1])));
}


static Value fn_has_field(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[1])) {
        pyro_panic(vm, "Invalid argument to $has_field(), field name must be a string.");
        return NULL_VAL();
    }
    Value field_name = args[1];

    if (IS_INSTANCE(args[0])) {
        ObjMap* field_index_map = AS_INSTANCE(args[0])->obj.class->field_indexes;
        Value field_index;
        if (ObjMap_get(field_index_map, field_name, &field_index)) {
            return BOOL_VAL(true);
        }
    }

    return BOOL_VAL(false);
}


static Value fn_is_instance(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_CLASS(args[1])) {
        pyro_panic(vm, "Invalid argument to $is_instance(), class must be a class object.");
        return NULL_VAL();
    }
    ObjClass* target_class = AS_CLASS(args[1]);

    if (!IS_INSTANCE(args[0])) {
        return BOOL_VAL(false);
    }
    ObjClass* instance_class = AS_INSTANCE(args[0])->obj.class;

    while (instance_class != NULL) {
        if (instance_class == target_class) {
            return BOOL_VAL(true);
        }
        instance_class = instance_class->superclass;
    }

    return BOOL_VAL(false);
}


static Value fn_shell(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to $shell(), must be a string.");
        return NULL_VAL();
    }

    CmdResult result;
    if (!pyro_run_shell_cmd(vm, AS_STR(args[0])->bytes, &result)) {
        // We've already panicked.
        return NULL_VAL();
    }

    return OBJ_VAL(result.output);
}


static Value fn_shell2(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid argument to $shell2(), must be a string.");
        return NULL_VAL();
    }

    CmdResult result;
    if (!pyro_run_shell_cmd(vm, AS_STR(args[0])->bytes, &result)) {
        // We've already panicked.
        return NULL_VAL();
    }

    Value output_string = OBJ_VAL(result.output);
    Value exit_code = I64_VAL(result.exit_code);

    pyro_push(vm, output_string);
    ObjTup* tup = ObjTup_new(2, vm);
    pyro_pop(vm);

    if (!tup) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    tup->values[0] = output_string;
    tup->values[1] = exit_code;

    return OBJ_VAL(tup);
}


static Value fn_debug(PyroVM* vm, size_t arg_count, Value* args) {
    Value method = pyro_get_method(args[0], vm->str_debug);
    if (!IS_NULL(method)) {
        pyro_push(vm, args[0]);
        Value debug_string = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (IS_STR(debug_string)) {
            return debug_string;
        }
        pyro_panic(vm, "Invalid type returned by $debug() method.");
        return NULL_VAL();
    }

    ObjStr* debug_string;

    if (IS_STR(args[0])) {
        debug_string = ObjStr_debug_str(AS_STR(args[0]), vm);
    } else if (IS_CHAR(args[0])) {
        debug_string = pyro_char_to_debug_str(vm, args[0]);
    } else {
        debug_string = pyro_stringify_value(vm, args[0]);
    }

    if (vm->halt_flag) {
        return NULL_VAL();
    }

    if (!debug_string) {
        pyro_panic(vm, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(debug_string);
}


static Value fn_read_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid path argument, must be a string.");
        return NULL_VAL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "r");
    if (!stream) {
        pyro_panic(vm, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
        return NULL_VAL();
    }

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "Insufficient memory available to read file.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                fclose(stream);
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(stream);

        if (c == EOF) {
            if (ferror(stream)) {
                pyro_panic(vm, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                fclose(stream);
                return NULL_VAL();
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
        pyro_panic(vm, "Insufficient memory available to read file.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        fclose(stream);
        return NULL_VAL();
    }

    fclose(stream);
    return OBJ_VAL(string);
}


static Value fn_write_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "Invalid path argument, must be a string.");
        return NULL_VAL();
    }

    if (!IS_STR(args[1]) && !IS_BUF(args[1])) {
        pyro_panic(vm, "Invalid content argument, must be a string or buffer.");
        return NULL_VAL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, "w");
    if (!stream) {
        pyro_panic(vm, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
        return NULL_VAL();
    }

    if (IS_BUF(args[1])) {
        ObjBuf* buf = AS_BUF(args[1]);
        size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, stream);
        if (n < buf->count) {
            pyro_panic(vm, "I/O write error.");
            fclose(stream);
            return NULL_VAL();
        }
        fclose(stream);
        return I64_VAL((int64_t)n);
    }

    ObjStr* string = AS_STR(args[1]);
    size_t n = fwrite(string->bytes, sizeof(char), string->length, stream);
    if (n < string->length) {
        pyro_panic(vm, "I/O write error.");
        fclose(stream);
        return NULL_VAL();
    }
    fclose(stream);
    return I64_VAL((int64_t)n);
}


// ------------ //
// Registration //
// ------------ //


void pyro_load_std_core(PyroVM* vm) {
    // Create the base standard library module.
    ObjModule* mod_std = pyro_define_module_1(vm, "$std");

    // Register [$std] as a global variable so it doesn't need to be explicitly imported.
    pyro_define_global(vm, "$std", OBJ_VAL(mod_std));

    ObjTup* args = ObjTup_new(0, vm);
    if (args) {
        pyro_push(vm, OBJ_VAL(args));
        pyro_define_global(vm, "$args", OBJ_VAL(args));
        pyro_pop(vm);
    }

    ObjFile* stdin_file = ObjFile_new(vm);
    if (stdin_file) {
        stdin_file->stream = stdin;
        pyro_push(vm, OBJ_VAL(stdin_file));
        pyro_define_global(vm, "$stdin", OBJ_VAL(stdin_file));
        pyro_pop(vm);
    }

    ObjFile* stdout_file = ObjFile_new(vm);
    if (stdout_file) {
        stdout_file->stream = stdout;
        pyro_push(vm, OBJ_VAL(stdout_file));
        pyro_define_global(vm, "$stdout", OBJ_VAL(stdout_file));
        pyro_pop(vm);
    }

    ObjFile* stderr_file = ObjFile_new(vm);
    if (stderr_file) {
        stderr_file->stream = stderr;
        pyro_push(vm, OBJ_VAL(stderr_file));
        pyro_define_global(vm, "$stderr", OBJ_VAL(stderr_file));
        pyro_pop(vm);
    }

    pyro_define_global_fn(vm, "$exit", fn_exit, 1);
    pyro_define_global_fn(vm, "$panic", fn_panic, 1);
    pyro_define_global_fn(vm, "$clock", fn_clock, 0);
    pyro_define_global_fn(vm, "$is_mod", fn_is_mod, 1);
    pyro_define_global_fn(vm, "$is_nan", fn_is_nan, 1);
    pyro_define_global_fn(vm, "$is_inf", fn_is_inf, 1);
    pyro_define_global_fn(vm, "$has_method", fn_has_method, 2);
    pyro_define_global_fn(vm, "$has_field", fn_has_field, 2);
    pyro_define_global_fn(vm, "$is_instance", fn_is_instance, 2);
    pyro_define_global_fn(vm, "$shell", fn_shell, 1);
    pyro_define_global_fn(vm, "$shell2", fn_shell2, 1);
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

    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);
    pyro_define_method(vm, vm->map_class, "count", map_count, 0);
    pyro_define_method(vm, vm->map_class, "get", map_get, 1);
    pyro_define_method(vm, vm->map_class, "$get_index", map_get, 1);
    pyro_define_method(vm, vm->map_class, "set", map_set, 2);
    pyro_define_method(vm, vm->map_class, "$set_index", map_set, 2);
    pyro_define_method(vm, vm->map_class, "del", map_del, 1);
    pyro_define_method(vm, vm->map_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->map_class, "copy", map_copy, 0);
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
    pyro_define_method(vm, vm->vec_class, "reverse", vec_reverse, 0);
    pyro_define_method(vm, vm->vec_class, "sort", vec_sort, -1);
    pyro_define_method(vm, vm->vec_class, "shuffle", vec_shuffle, 0);
    pyro_define_method(vm, vm->vec_class, "contains", vec_contains, 1);
    pyro_define_method(vm, vm->vec_class, "index_of", vec_index_of, 1);
    pyro_define_method(vm, vm->vec_class, "map", vec_map, 1);
    pyro_define_method(vm, vm->vec_class, "filter", vec_filter, 1);
    pyro_define_method(vm, vm->vec_class, "copy", vec_copy, 0);
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
    pyro_define_method(vm, vm->str_class, "starts_with", str_starts_with, 1);
    pyro_define_method(vm, vm->str_class, "ends_with", str_ends_with, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix", str_strip_prefix, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix", str_strip_suffix, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix_bytes", str_strip_prefix_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix_bytes", str_strip_suffix_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_bytes", str_strip_bytes, 1);
    pyro_define_method(vm, vm->str_class, "strip_chars", str_strip_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip_prefix_chars", str_strip_prefix_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip_suffix_chars", str_strip_suffix_chars, 1);
    pyro_define_method(vm, vm->str_class, "strip", str_strip, -1);
    pyro_define_method(vm, vm->str_class, "strip_ascii_ws", str_strip_ascii_ws, 0);
    pyro_define_method(vm, vm->str_class, "strip_utf8_ws", str_strip_utf8_ws, 0);
    pyro_define_method(vm, vm->str_class, "match", str_match, 2);
    pyro_define_method(vm, vm->str_class, "replace", str_replace, 2);
    pyro_define_method(vm, vm->str_class, "substr", str_substr, 2);
    pyro_define_method(vm, vm->str_class, "index_of", str_index_of, 2);
    pyro_define_method(vm, vm->str_class, "contains", str_contains, 1);
    pyro_define_method(vm, vm->str_class, "split", str_split, 1);
    pyro_define_method(vm, vm->str_class, "split_on_ascii_ws", str_split_on_ascii_ws, 0);
    pyro_define_method(vm, vm->str_class, "to_hex", str_to_hex, 0);

    pyro_define_global_fn(vm, "$range", fn_range, -1);
    pyro_define_global_fn(vm, "$is_range", fn_is_range, 1);
    pyro_define_method(vm, vm->range_class, "$iter", range_iter, 0);
    pyro_define_method(vm, vm->range_class, "$next", range_next, 0);

    pyro_define_global_fn(vm, "$err", fn_err, -1);
    pyro_define_global_fn(vm, "$is_err", fn_is_err, 1);

    pyro_define_global_fn(vm, "$buf", fn_buf, 0);
    pyro_define_global_fn(vm, "$is_buf", fn_is_buf, 1);
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

    pyro_define_global_fn(vm, "$file", fn_file, 2);
    pyro_define_global_fn(vm, "$is_file", fn_is_file, 1);
    pyro_define_method(vm, vm->file_class, "close", file_close, 0);
    pyro_define_method(vm, vm->file_class, "flush", file_flush, 0);
    pyro_define_method(vm, vm->file_class, "read", file_read, 0);
    pyro_define_method(vm, vm->file_class, "read_string", file_read_string, 0);
    pyro_define_method(vm, vm->file_class, "read_line", file_read_line, 0);
    pyro_define_method(vm, vm->file_class, "read_bytes", file_read_bytes, 1);
    pyro_define_method(vm, vm->file_class, "read_byte", file_read_byte, 0);
    pyro_define_method(vm, vm->file_class, "write", file_write, -1);
    pyro_define_method(vm, vm->file_class, "write_byte", file_write_byte, 1);
}
