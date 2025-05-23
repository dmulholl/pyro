#include "../includes/pyro.h"


static PyroValue fn_vec(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        PyroVec* vec = PyroVec_new(vm);
        if (!vec) {
            pyro_panic(vm, "out of memory");
            return pyro_null();
        }
        return pyro_obj(vec);
    }

    if (arg_count == 1) {
        // Does the object have an :$iter() method?
        PyroValue iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
        if (PYRO_IS_NULL(iter_method)) {
            pyro_panic(vm, "$vec(): invalid argument [arg], expected an iterable object");
            return pyro_null();
        }

        // Call the object's :$iter() method to get an iterator.
        if (!pyro_push(vm, args[0])) return pyro_null();
        PyroValue iterator = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (!pyro_push(vm, iterator)) return pyro_null(); // protect from GC

        // Get the iterator's :$next() method.
        PyroValue next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
        if (PYRO_IS_NULL(next_method)) {
            pyro_panic(vm, "$vec(): invalid argument [arg], $iter() method does not return an iterator");
            return pyro_null();
        }

        PyroVec* vec = PyroVec_new(vm);
        if (!vec) {
            pyro_panic(vm, "out of memory");
            return pyro_null();
        }
        if (!pyro_push(vm, pyro_obj(vec))) return pyro_null(); // protect from GC

        while (true) {
            if (!pyro_push(vm, iterator)) return pyro_null();
            PyroValue next_value = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return pyro_null();
            }
            if (PYRO_IS_ERR(next_value)) {
                break;
            }
            if (!PyroVec_append(vec, next_value, vm)) {
                pyro_panic(vm, "out of memory");
                return pyro_null();
            }
        }

        pyro_pop(vm); // vec
        pyro_pop(vm); // iterator
        return pyro_obj(vec);
    }

    if (arg_count == 2) {
        if (PYRO_IS_I64(args[0]) && args[0].as.i64 >= 0) {
            size_t size = args[0].as.i64;
            PyroVec* vec = PyroVec_new_with_capacity(size, vm);
            if (!vec) {
                pyro_panic(vm, "out of memory");
                return pyro_null();
            }

            PyroValue fill_value = args[1];
            for (size_t i = 0; i < size; i++) {
                vec->values[i] = fill_value;
            }
            vec->count = size;

            return pyro_obj(vec);
        }
        pyro_panic(vm, "$vec(): invalid argument [size], expected a positive integer");
        return pyro_null();
    }

    pyro_panic(vm, "$vec(): expected 0, 1, or 2 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_is_vec(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_VEC(args[0]));
}


static PyroValue vec_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    return pyro_i64(vec->count);
}


static PyroValue vec_capacity(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    return pyro_i64(vec->capacity);
}


static PyroValue vec_append(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    // Experimentally, if we're appending a single value, PyroVec_append() is faster.
    if (arg_count == 1) {
        if (!PyroVec_append(vec, args[0], vm)) {
            pyro_panic(vm, "out of memory");
        }
        return pyro_null();
    }

    if (!PyroVec_append_values(vec, args, arg_count, vm)) {
        pyro_panic(vm, "out of memory");
    }

    return pyro_null();
}


static PyroValue vec_append_values(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (PYRO_IS_VEC(args[0])) {
        if (!PyroVec_append_values(vec, PYRO_AS_VEC(args[0])->values, PYRO_AS_VEC(args[0])->count, vm)) {
            pyro_panic(vm, "out of memory");
        }
        return pyro_null();
    }

    if (PYRO_IS_TUP(args[0])) {
        if (!PyroVec_append_values(vec, PYRO_AS_TUP(args[0])->values, PYRO_AS_TUP(args[0])->count, vm)) {
            pyro_panic(vm, "out of memory");
        }
        return pyro_null();
    }

    PyroValue iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (PYRO_IS_NULL(iter_method)) {
        pyro_panic(vm, "append_values(): argument must be iterable");
        return pyro_null();
    }

    if (!pyro_push(vm, args[0])) return pyro_null();
    PyroValue iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return pyro_null();
    }
    if (!pyro_push(vm, iterator)) return pyro_null(); // protect from GC

    PyroValue next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
    if (PYRO_IS_NULL(next_method)) {
        pyro_panic(vm, "append_values(): invalid argument, $iter() method does not return an iterator");
        return pyro_null();
    }

    while (true) {
        if (!pyro_push(vm, iterator)) return pyro_null();
        PyroValue next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (PYRO_IS_ERR(next_value)) {
            break;
        }
        if (!PyroVec_append(vec, next_value, vm)) {
            pyro_panic(vm, "out of memory");
            return pyro_null();
        }
    }

    pyro_pop(vm); // iterator
    return pyro_null();
}

static PyroValue vec_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "get(): invalid argument [index], type '%s', expected 'i64'",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    int64_t index = args[0].as.i64;
    if (index < 0) {
        index += vec->count;
    }

    if (index < 0 || (size_t)index >= vec->count) {
        pyro_panic(vm, "get(): index %" PRId64 " is out of range", index);
        return pyro_null();
    }

    return vec->values[index];
}


static PyroValue vec_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "set(): invalid argument [index], type '%s', expected 'i64'",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    int64_t index = args[0].as.i64;
    if (index < 0) {
        index += vec->count;
    }

    if (index < 0 || (size_t)index >= vec->count) {
        pyro_panic(vm, "set(): index %" PRId64 " is out of range", index);
        return pyro_null();
    }

    vec->version++;
    vec->values[index] = args[1];
    return args[1];
}


static PyroValue vec_reverse(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (vec->count < 2) {
        return pyro_obj(vec);
    }

    PyroValue* low = &vec->values[0];
    PyroValue* high = &vec->values[vec->count - 1];

    while (low < high) {
        PyroValue temp = *low;
        *low = *high;
        *high = temp;
        low++;
        high--;
    }

    return pyro_obj(vec);
}


static PyroValue vec_sort(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (arg_count == 0) {
        pyro_mergesort(vm, vec->values, vec->count);
        return pyro_obj(vec);
    }

    if (arg_count == 1) {
        pyro_mergesort_with_callback(vm, vec->values, vec->count, args[0]);
        return pyro_obj(vec);
    }

    pyro_panic(vm, "sort(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue vec_mergesort(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (arg_count == 0) {
        pyro_mergesort(vm, vec->values, vec->count);
        return pyro_obj(vec);
    }

    if (arg_count == 1) {
        pyro_mergesort_with_callback(vm, vec->values, vec->count, args[0]);
        return pyro_obj(vec);
    }

    pyro_panic(vm, "mergesort(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue vec_quicksort(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (arg_count == 0) {
        pyro_quicksort(vm, vec->values, vec->count);
        return pyro_obj(vec);
    }

    if (arg_count == 1) {
        pyro_quicksort_with_callback(vm, vec->values, vec->count, args[0]);
        return pyro_obj(vec);
    }

    pyro_panic(vm, "quicksort(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue vec_shuffle(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;
    pyro_shuffle(vm, vec->values, vec->count);
    return pyro_obj(vec);
}


static PyroValue vec_copy(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    PyroVec* copy = PyroVec_copy(vec, vm);
    if (!copy) {
        pyro_panic(vm, "copy(): out of memory");
        return pyro_null();
    }
    return pyro_obj(copy);
}


static PyroValue vec_contains(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    PyroValue target = args[0];

    for (size_t i = 0; i < vec->count; i++) {
        bool found = pyro_op_compare_eq(vm, vec->values[i], target);
        if (vm->halt_flag) {
            return pyro_bool(false);
        }
        if (found) {
            return pyro_bool(true);
        }
    }

    return pyro_bool(false);
}


static PyroValue vec_index_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    PyroValue target = args[0];

    for (size_t i = 0; i < vec->count; i++) {
        bool found = pyro_op_compare_eq(vm, vec->values[i], target);
        if (vm->halt_flag) {
            return pyro_obj(vm->empty_error);
        }
        if (found) {
            return pyro_i64((int64_t)i);
        }
    }

    return pyro_obj(vm->empty_error);
}


static PyroValue vec_map(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    PyroValue map_func = args[0];

    PyroVec* new_vec = PyroVec_new_with_capacity(vec->count, vm);
    if (!vec) {
        pyro_panic(vm, "map(): out of memory");
        return pyro_null();
    }
    if (!pyro_push(vm, pyro_obj(new_vec))) return pyro_null();

    for (size_t i = 0; i < vec->count; i++) {
        if (!pyro_push(vm, map_func)) return pyro_null();
        if (!pyro_push(vm, vec->values[i])) return pyro_null();
        PyroValue result = pyro_call_function(vm, 1);
        if (vm->halt_flag) {
            return pyro_null();
        }
        new_vec->values[i] = result;
        new_vec->count++;
    }

    pyro_pop(vm);
    return pyro_obj(new_vec);
}


static PyroValue vec_filter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    PyroValue filter_func = args[0];

    PyroVec* new_vec = PyroVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "filter(): out of memory");
        return pyro_null();
    }
    if (!pyro_push(vm, pyro_obj(new_vec))) return pyro_null();

    for (size_t i = 0; i < vec->count; i++) {
        if (!pyro_push(vm, filter_func)) return pyro_null();
        if (!pyro_push(vm, vec->values[i])) return pyro_null();
        PyroValue result = pyro_call_function(vm, 1);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (pyro_is_truthy(result)) {
            if (!PyroVec_append(new_vec, vec->values[i], vm)) {
                pyro_panic(vm, "filter(): out of memory");
                return pyro_null();
            }
        }
    }

    pyro_pop(vm);
    return pyro_obj(new_vec);
}


static PyroValue vec_values(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)vec, PYRO_ITER_VEC, vm);
    if (!iter) {
        pyro_panic(vm, "values(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue vec_remove_last(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;
    return PyroVec_remove_last(vec, vm);
}


static PyroValue vec_remove_first(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;
    return PyroVec_remove_first(vec, vm);
}


static PyroValue vec_remove_at_index(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "remove_at_index(): invalid argument [index], expected an integer");
        return pyro_null();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, "remove_at_index(): invalid argument [index], out of range");
        return pyro_null();
    }

    return PyroVec_remove_at_index(vec, args[0].as.i64, vm);
}


static PyroValue vec_remove_random(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (vec->count == 0) {
        pyro_panic(vm, "remove_random(): vector is empty");
        return pyro_null();
    }

    size_t index = pyro_xoshiro256ss_next_in_range(&vm->prng_state, vec->count);
    return PyroVec_remove_at_index(vec, index, vm);
}


static PyroValue vec_random(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (vec->count == 0) {
        pyro_panic(vm, "random(): vector is empty");
        return pyro_null();
    }

    size_t index = pyro_xoshiro256ss_next_in_range(&vm->prng_state, vec->count);
    return vec->values[index];
}


static PyroValue vec_insert_at_index(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "insert_at_index(): invalid argument [index], expected an integer");
        return pyro_null();
    }

    if (args[0].as.i64 < 0 || (size_t)args[0].as.i64 > vec->count) {
        pyro_panic(vm, "insert_at_index(): invalid argument [index], out of range");
        return pyro_null();
    }

    PyroVec_insert_at_index(vec, args[0].as.i64, args[1], vm);
    return pyro_null();
}


static PyroValue vec_first(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[0];
    }
    pyro_panic(vm, "first(): vector is empty");
    return pyro_null();
}


static PyroValue vec_last(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[vec->count - 1];
    }
    pyro_panic(vm, "last(): vector is empty");
    return pyro_null();
}


static PyroValue vec_is_empty(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    return pyro_bool(vec->count == 0);
}


static PyroValue fn_stack(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PyroVec_new_as_stack(vm);
    if (!vec) {
        pyro_panic(vm, "$stack(): out of memory");
        return pyro_null();
    }
    return pyro_obj(vec);
}


static PyroValue fn_is_stack(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_STACK(args[0]));
}


static PyroValue stack_pop(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;

    if (vec->count == 0) {
        return pyro_obj(vm->empty_error);
    }

    vec->count--;
    return vec->values[vec->count];
}


static PyroValue stack_peek(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    if (vec->count == 0) {
        return pyro_obj(vm->empty_error);
    }
    return vec->values[vec->count - 1];
}


static PyroValue vec_slice(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, "slice(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "slice(): invalid argument [start_index], expected an integer");
        return pyro_null();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= vec->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= vec->count) {
        start_index = (size_t)((int64_t)vec->count + args[0].as.i64);
    } else {
        pyro_panic(vm, "slice(): invalid argument [start_index], integer (%d) is out of range", args[0].as.i64);
        return pyro_null();
    }

    size_t length = vec->count - start_index;
    if (arg_count == 2) {
        if (!PYRO_IS_I64(args[1])) {
            pyro_panic(vm, "slice(): invalid argument [length], expected an integer");
            return pyro_null();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, "slice(): invalid argument [length], expected a positive integer");
            return pyro_null();
        }
        if (start_index + (size_t)args[1].as.i64 > vec->count) {
            pyro_panic(vm, "slice(): invalid argument [length], integer (%d) is out of range", args[1].as.i64);
            return pyro_null();
        }
        length = (size_t)args[1].as.i64;
    }

    PyroVec* new_vec = PyroVec_new(vm);
    if (!new_vec) {
        pyro_panic(vm, "slice(): out of memory");
        return pyro_null();
    }

    if (length == 0) {
        return pyro_obj(new_vec);
    }

    PyroValue* new_array = PYRO_ALLOCATE_ARRAY(vm, PyroValue, length);
    if (!new_array) {
        pyro_panic(vm, "slice(): out of memory");
        return pyro_null();
    }

    memcpy(new_array, &vec->values[start_index], sizeof(PyroValue) * length);
    new_vec->values = new_array;
    new_vec->capacity = length;
    new_vec->count = length;

    return pyro_obj(new_vec);
}


static PyroValue vec_is_sorted(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    if (arg_count == 0) {
        return pyro_bool(pyro_is_sorted(vm, vec->values, vec->count));
    }

    if (arg_count == 1) {
        return pyro_bool(pyro_is_sorted_with_callback(vm, vec->values, vec->count, args[0]));
    }

    pyro_panic(vm, "is_sorted(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue vec_join(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)vec, PYRO_ITER_VEC, vm);
    if (!iter) {
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }

    if (arg_count == 0) {
        if (!pyro_push(vm, pyro_obj(iter))) return pyro_null();
        PyroStr* result = PyroIter_join(iter, "", 0, vm);
        pyro_pop(vm);
        return vm->halt_flag ? pyro_null() : pyro_obj(result);
    }

    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "join(): invalid argument [sep], expected a string");
            return pyro_null();
        }
        PyroStr* sep = PYRO_AS_STR(args[0]);
        if (!pyro_push(vm, pyro_obj(iter))) return pyro_null();
        PyroStr* result = PyroIter_join(iter, sep->bytes, sep->count, vm);
        pyro_pop(vm);
        return vm->halt_flag ? pyro_null() : pyro_obj(result);
    }

    pyro_panic(vm, "join(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue vec_clear(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);
    vec->version++;
    PyroVec_clear(vec, vm);
    return pyro_null();
}


static PyroValue stack_values(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroVec* vec = PYRO_AS_VEC(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)vec, PYRO_ITER_VEC_REVERSE_ORDER, vm);
    if (!iter) {
        pyro_panic(vm, "values(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


void pyro_load_builtin_type_vec(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$vec", fn_vec, -1);
    pyro_define_superglobal_fn(vm, "$is_vec", fn_is_vec, 1);
    pyro_define_superglobal_fn(vm, "$stack", fn_stack, 0);
    pyro_define_superglobal_fn(vm, "$is_stack", fn_is_stack, 1);

    // Vector methods -- private.
    pyro_define_pri_method(vm, vm->class_vec, "$iter", vec_values, 0);
    pyro_define_pri_method(vm, vm->class_vec, "$contains", vec_contains, 1);

    // Vector methods -- public.
    pyro_define_pub_method(vm, vm->class_vec, "count", vec_count, 0);
    pyro_define_pub_method(vm, vm->class_vec, "capacity", vec_capacity, 0);
    pyro_define_pub_method(vm, vm->class_vec, "append", vec_append, -1);
    pyro_define_pub_method(vm, vm->class_vec, "append_values", vec_append_values, 1);
    pyro_define_pub_method(vm, vm->class_vec, "get", vec_get, 1);
    pyro_define_pub_method(vm, vm->class_vec, "set", vec_set, 2);
    pyro_define_pub_method(vm, vm->class_vec, "reverse", vec_reverse, 0);
    pyro_define_pub_method(vm, vm->class_vec, "sort", vec_sort, -1);
    pyro_define_pub_method(vm, vm->class_vec, "mergesort", vec_mergesort, -1);
    pyro_define_pub_method(vm, vm->class_vec, "quicksort", vec_quicksort, -1);
    pyro_define_pub_method(vm, vm->class_vec, "shuffle", vec_shuffle, 0);
    pyro_define_pub_method(vm, vm->class_vec, "contains", vec_contains, 1);
    pyro_define_pub_method(vm, vm->class_vec, "index_of", vec_index_of, 1);
    pyro_define_pub_method(vm, vm->class_vec, "map", vec_map, 1);
    pyro_define_pub_method(vm, vm->class_vec, "filter", vec_filter, 1);
    pyro_define_pub_method(vm, vm->class_vec, "copy", vec_copy, 0);
    pyro_define_pub_method(vm, vm->class_vec, "remove_last", vec_remove_last, 0);
    pyro_define_pub_method(vm, vm->class_vec, "remove_first", vec_remove_first, 0);
    pyro_define_pub_method(vm, vm->class_vec, "remove_at", vec_remove_at_index, 1);
    pyro_define_pub_method(vm, vm->class_vec, "insert_at", vec_insert_at_index, 2);
    pyro_define_pub_method(vm, vm->class_vec, "first", vec_first, 0);
    pyro_define_pub_method(vm, vm->class_vec, "last", vec_last, 0);
    pyro_define_pub_method(vm, vm->class_vec, "is_empty", vec_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_vec, "slice", vec_slice, -1);
    pyro_define_pub_method(vm, vm->class_vec, "is_sorted", vec_is_sorted, -1);
    pyro_define_pub_method(vm, vm->class_vec, "join", vec_join, -1);
    pyro_define_pub_method(vm, vm->class_vec, "values", vec_values, 0);
    pyro_define_pub_method(vm, vm->class_vec, "remove_random", vec_remove_random, 0);
    pyro_define_pub_method(vm, vm->class_vec, "random", vec_random, 0);
    pyro_define_pub_method(vm, vm->class_vec, "clear", vec_clear, 0);

    // Stack methods -- private.
    pyro_define_pri_method(vm, vm->class_stack, "$iter", stack_values, 0);
    pyro_define_pri_method(vm, vm->class_stack, "$contains", vec_contains, 1);

    // Stack methods -- public.
    pyro_define_pub_method(vm, vm->class_stack, "count", vec_count, 0);
    pyro_define_pub_method(vm, vm->class_stack, "is_empty", vec_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_stack, "push", vec_append, 1);
    pyro_define_pub_method(vm, vm->class_stack, "pop", stack_pop, 0);
    pyro_define_pub_method(vm, vm->class_stack, "peek", stack_peek, 0);
    pyro_define_pub_method(vm, vm->class_stack, "clear", vec_clear, 0);
    pyro_define_pub_method(vm, vm->class_stack, "contains", vec_contains, 1);
    pyro_define_pub_method(vm, vm->class_stack, "values", stack_values, 0);

    // Deprecated.
    pyro_define_pub_method(vm, vm->class_vec, "iter", vec_values, 0);
    pyro_define_pub_method(vm, vm->class_stack, "iter", stack_values, 0);
}
