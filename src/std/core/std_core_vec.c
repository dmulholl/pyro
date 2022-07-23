#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/operators.h"
#include "../../inc/setup.h"
#include "../../inc/sorting.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value fn_vec(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, "$vec(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(vec);
    }

    else if (arg_count == 1) {
        // Does the object have an :$iter() method?
        Value iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
        if (IS_NULL(iter_method)) {
            pyro_panic(vm, "$vec(): invalid argument [arg], expected an iterable object");
            return MAKE_NULL();
        }

        // Call the object's :$iter() method to get an iterator.
        pyro_push(vm, args[0]); // receiver for the $iter() method call
        Value iterator = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        pyro_push(vm, iterator); // protect from GC

        // Get the iterator's :$next() method.
        Value next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
        if (IS_NULL(next_method)) {
            pyro_panic(vm, "$vec(): invalid argument [arg], iterator has no :$next() method");
            return MAKE_NULL();
        }
        pyro_push(vm, next_method); // protect from GC

        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, "$vec(): out of memory");
            return MAKE_NULL();
        }
        pyro_push(vm, MAKE_OBJ(vec)); // protect from GC

        while (true) {
            pyro_push(vm, iterator); // receiver for the :$next() method call
            Value next_value = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return MAKE_NULL();
            }
            if (IS_ERR(next_value)) {
                break;
            }
            pyro_push(vm, next_value); // protect from GC
            if (!ObjVec_append(vec, next_value, vm)) {
                pyro_panic(vm, "$vec(): out of memory");
                return MAKE_NULL();
            }
            pyro_pop(vm); // next_value
        }

        pyro_pop(vm); // vec
        pyro_pop(vm); // next_method
        pyro_pop(vm); // iterator
        return MAKE_OBJ(vec);
    }

    else if (arg_count == 2) {
        if (IS_I64(args[0]) && args[0].as.i64 >= 0) {
            ObjVec* vec = ObjVec_new_with_cap_and_fill((size_t)args[0].as.i64, args[1], vm);
            if (!vec) {
                pyro_panic(vm, "$vec(): out of memory");
                return MAKE_NULL();
            }
            return MAKE_OBJ(vec);
        } else {
            pyro_panic(vm, "$vec(): invalid argument [size], expected a positive integer");
            return MAKE_NULL();
        }
    }

    else {
        pyro_panic(vm, "$vec(): expected 0 or 2 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }
}


static Value fn_is_vec(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_VEC(args[0]));
}


static Value vec_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return MAKE_I64(vec->count);
}


static Value vec_append(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (!ObjVec_append(vec, args[0], vm)) {
        pyro_panic(vm, "append(): out of memory");
    }
    return MAKE_NULL();
}


static Value vec_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < vec->count) {
            return vec->values[index];
        }
        pyro_panic(vm, "get(): invalid argument [index], out of range");
        return MAKE_NULL();
    }
    pyro_panic(vm, "get(): invalid argument [index], expected an integer");
    return MAKE_NULL();
}


static Value vec_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < vec->count) {
            vec->values[index] = args[1];
            return args[1];
        }
        pyro_panic(vm, "set(): invalid argument [index], out of range");
        return MAKE_NULL();
    }
    pyro_panic(vm, "set(): invalid argument [index], expected an integer");
    return MAKE_NULL();
}


static Value vec_reverse(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (vec->count < 2) {
        return MAKE_OBJ(vec);
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

    return MAKE_OBJ(vec);
}


static Value vec_sort(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (arg_count == 0) {
        pyro_mergesort(vm, vec->values, vec->count);
        return MAKE_OBJ(vec);
    }

    if (arg_count == 1) {
        pyro_mergesort_with_callback(vm, vec->values, vec->count, args[0]);
        return MAKE_OBJ(vec);
    }

    pyro_panic(vm, "sort(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value vec_mergesort(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (arg_count == 0) {
        pyro_mergesort(vm, vec->values, vec->count);
        return MAKE_OBJ(vec);
    }

    if (arg_count == 1) {
        pyro_mergesort_with_callback(vm, vec->values, vec->count, args[0]);
        return MAKE_OBJ(vec);
    }

    pyro_panic(vm, "mergesort(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value vec_quicksort(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (arg_count == 0) {
        pyro_quicksort(vm, vec->values, vec->count);
        return MAKE_OBJ(vec);
    }

    if (arg_count == 1) {
        pyro_quicksort_with_callback(vm, vec->values, vec->count, args[0]);
        return MAKE_OBJ(vec);
    }

    pyro_panic(vm, "quicksort(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value vec_shuffle(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    pyro_shuffle(vm, vec->values, vec->count);
    return MAKE_OBJ(vec);
}


static Value vec_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    ObjVec* copy = ObjVec_copy(vec, vm);
    if (!copy) {
        pyro_panic(vm, "copy(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(vec);
}


static Value vec_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_op_compare_eq(vm, vec->values[i], args[0])) {
            return MAKE_BOOL(true);
        }
    }

    return MAKE_BOOL(false);
}


static Value vec_index_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_op_compare_eq(vm, vec->values[i], args[0])) {
            return MAKE_I64((int64_t)i);
        }
    }

    return MAKE_OBJ(vm->empty_error);
}


static Value vec_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjVec* new_vec = ObjVec_new_with_cap(vec->count, vm);
    if (!vec) {
        pyro_panic(vm, "map(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(new_vec));

    for (size_t i = 0; i < vec->count; i++) {
        pyro_push(vm, args[0]); // push the map function
        pyro_push(vm, vec->values[i]); // push the argument for the map function
        Value result = pyro_call_function(vm, 1);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        new_vec->values[i] = result;
        new_vec->count++;
    }

    pyro_pop(vm);
    return MAKE_OBJ(new_vec);
}


static Value vec_filter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjVec* new_vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "filter(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(new_vec));

    for (size_t i = 0; i < vec->count; i++) {
        pyro_push(vm, args[0]); // push the filter function
        pyro_push(vm, vec->values[i]); // push the argument for the filter function
        Value result = pyro_call_function(vm, 1);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        if (pyro_is_truthy(result)) {
            if (!ObjVec_append(new_vec, vec->values[i], vm)) {
                pyro_panic(vm, "filter(): out of memory");
                return MAKE_NULL();
            }
        }
    }

    pyro_pop(vm);
    return MAKE_OBJ(new_vec);
}


static Value vec_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)vec, ITER_VEC, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(iter);
}


static Value vec_remove_last(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return ObjVec_remove_last(vec, vm);
}


static Value vec_remove_first(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return ObjVec_remove_first(vec, vm);
}


static Value vec_remove_at_index(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "remove_at_index(): invalid argument [index], expected an integer");
        return MAKE_NULL();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, "remove_at_index(): invalid argument [index], out of range");
        return MAKE_NULL();
    }

    return ObjVec_remove_at_index(vec, args[0].as.i64, vm);
}


static Value vec_remove_random(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (vec->count == 0) {
        pyro_panic(vm, "remove_random(): vector is empty");
        return MAKE_NULL();
    }

    size_t index = pyro_mt64_gen_int(vm->mt64, vec->count);
    return ObjVec_remove_at_index(vec, index, vm);
}


static Value vec_random(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (vec->count == 0) {
        pyro_panic(vm, "random(): vector is empty");
        return MAKE_NULL();
    }

    size_t index = pyro_mt64_gen_int(vm->mt64, vec->count);
    return vec->values[index];
}


static Value vec_insert_at_index(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "insert_at_index(): invalid argument [index], expected an integer");
        return MAKE_NULL();
    }

    if (args[0].as.i64 < 0 || (size_t)args[0].as.i64 > vec->count) {
        pyro_panic(vm, "insert_at_index(): invalid argument [index], out of range");
        return MAKE_NULL();
    }

    ObjVec_insert_at_index(vec, args[0].as.i64, args[1], vm);
    return MAKE_NULL();
}


static Value vec_first(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[0];
    }
    pyro_panic(vm, "first(): vector is empty");
    return MAKE_NULL();
}


static Value vec_last(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[vec->count - 1];
    }
    pyro_panic(vm, "last(): vector is empty");
    return MAKE_NULL();
}


static Value vec_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return MAKE_BOOL(vec->count == 0);
}


static Value fn_stack(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = ObjVec_new_as_stack(vm);
    if (!vec) {
        pyro_panic(vm, "$stack(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(vec);
}


static Value fn_is_stack(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_STACK(args[0]));
}


static Value stack_pop(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count == 0) {
        pyro_panic(vm, "pop(): stack is empty");
        return MAKE_NULL();
    }
    vec->count--;
    return vec->values[vec->count];
}


static Value vec_slice(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, "slice(): expected 1 or 2 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "slice(): invalid argument [start_index], expected an integer");
        return MAKE_NULL();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= vec->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= vec->count) {
        start_index = (size_t)((int64_t)vec->count + args[0].as.i64);
    } else {
        pyro_panic(vm, "slice(): invalid argument [start_index], integer (%d) is out of range", args[0].as.i64);
        return MAKE_NULL();
    }

    size_t length = vec->count - start_index;
    if (arg_count == 2) {
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "slice(): invalid argument [length], expected an integer");
            return MAKE_NULL();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, "slice(): invalid argument [length], expected a positive integer");
            return MAKE_NULL();
        }
        if (start_index + (size_t)args[1].as.i64 > vec->count) {
            pyro_panic(vm, "slice(): invalid argument [length], integer (%d) is out of range", args[1].as.i64);
            return MAKE_NULL();
        }
        length = (size_t)args[1].as.i64;
    }

    ObjVec* new_vec = ObjVec_new(vm);
    if (!new_vec) {
        pyro_panic(vm, "slice(): out of memory");
        return MAKE_NULL();
    }

    if (length == 0) {
        return MAKE_OBJ(new_vec);
    }

    pyro_push(vm, MAKE_OBJ(new_vec));
    Value* new_array = ALLOCATE_ARRAY(vm, Value, length);
    if (!new_array) {
        pyro_panic(vm, "slice(): out of memory");
        return MAKE_NULL();
    }
    pyro_pop(vm); // new_vec

    memcpy(new_array, &vec->values[start_index], sizeof(Value) * length);
    new_vec->values = new_array;
    new_vec->capacity = length;
    new_vec->count = length;

    return MAKE_OBJ(new_vec);
}


static Value vec_is_sorted(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (arg_count == 0) {
        return MAKE_BOOL(pyro_is_sorted(vm, vec->values, vec->count));
    }

    if (arg_count == 1) {
        return MAKE_BOOL(pyro_is_sorted_with_callback(vm, vec->values, vec->count, args[0]));
    }

    pyro_panic(vm, "is_sorted(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value vec_join(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)vec, ITER_VEC, vm);
    if (!iter) {
        pyro_panic(vm, "join(): out of memory");
        return MAKE_NULL();
    }

    if (arg_count == 0) {
        pyro_push(vm, MAKE_OBJ(iter));
        ObjStr* result = ObjIter_join(iter, "", 0, vm);
        pyro_pop(vm);
        return vm->halt_flag ? MAKE_NULL() : MAKE_OBJ(result);
    }

    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "join(): invalid argument [sep], expected a string");
            return MAKE_NULL();
        }
        ObjStr* sep = AS_STR(args[0]);
        pyro_push(vm, MAKE_OBJ(iter));
        ObjStr* result = ObjIter_join(iter, sep->bytes, sep->length, vm);
        pyro_pop(vm);
        return vm->halt_flag ? MAKE_NULL() : MAKE_OBJ(result);
    }

    pyro_panic(vm, "join(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


void pyro_load_std_core_vec(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$vec", fn_vec, -1);
    pyro_define_global_fn(vm, "$is_vec", fn_is_vec, 1);
    pyro_define_global_fn(vm, "$stack", fn_stack, 0);
    pyro_define_global_fn(vm, "$is_stack", fn_is_stack, 1);

    // Vector methods.
    pyro_define_method(vm, vm->class_vec, "count", vec_count, 0);
    pyro_define_method(vm, vm->class_vec, "append", vec_append, 1);
    pyro_define_method(vm, vm->class_vec, "get", vec_get, 1);
    pyro_define_method(vm, vm->class_vec, "set", vec_set, 2);
    pyro_define_method(vm, vm->class_vec, "reverse", vec_reverse, 0);
    pyro_define_method(vm, vm->class_vec, "sort", vec_sort, -1);
    pyro_define_method(vm, vm->class_vec, "mergesort", vec_mergesort, -1);
    pyro_define_method(vm, vm->class_vec, "quicksort", vec_quicksort, -1);
    pyro_define_method(vm, vm->class_vec, "shuffle", vec_shuffle, 0);
    pyro_define_method(vm, vm->class_vec, "contains", vec_contains, 1);
    pyro_define_method(vm, vm->class_vec, "$contains", vec_contains, 1);
    pyro_define_method(vm, vm->class_vec, "index_of", vec_index_of, 1);
    pyro_define_method(vm, vm->class_vec, "map", vec_map, 1);
    pyro_define_method(vm, vm->class_vec, "filter", vec_filter, 1);
    pyro_define_method(vm, vm->class_vec, "copy", vec_copy, 0);
    pyro_define_method(vm, vm->class_vec, "remove_last", vec_remove_last, 0);
    pyro_define_method(vm, vm->class_vec, "remove_first", vec_remove_first, 0);
    pyro_define_method(vm, vm->class_vec, "remove_at", vec_remove_at_index, 1);
    pyro_define_method(vm, vm->class_vec, "insert_at", vec_insert_at_index, 2);
    pyro_define_method(vm, vm->class_vec, "first", vec_first, 0);
    pyro_define_method(vm, vm->class_vec, "last", vec_last, 0);
    pyro_define_method(vm, vm->class_vec, "is_empty", vec_is_empty, 0);
    pyro_define_method(vm, vm->class_vec, "slice", vec_slice, -1);
    pyro_define_method(vm, vm->class_vec, "is_sorted", vec_is_sorted, -1);
    pyro_define_method(vm, vm->class_vec, "join", vec_join, -1);
    pyro_define_method(vm, vm->class_vec, "$iter", vec_iter, 0);
    pyro_define_method(vm, vm->class_vec, "iter", vec_iter, 0);
    pyro_define_method(vm, vm->class_vec, "remove_random", vec_remove_random, 0);
    pyro_define_method(vm, vm->class_vec, "random", vec_random, 0);

    // Stack methods.
    pyro_define_method(vm, vm->class_stack, "count", vec_count, 0);
    pyro_define_method(vm, vm->class_stack, "is_empty", vec_is_empty, 0);
    pyro_define_method(vm, vm->class_stack, "push", vec_append, 1);
    pyro_define_method(vm, vm->class_stack, "pop", stack_pop, 0);
    pyro_define_method(vm, vm->class_stack, "$iter", vec_iter, 0);
}
