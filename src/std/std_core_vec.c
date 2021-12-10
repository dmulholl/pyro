#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/errors.h"
#include "../vm/operators.h"


static Value fn_vec(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(vec);
    }

    else if (arg_count == 1) {
        // Does the object have an :$iter() method?
        Value iter_method = pyro_get_method(vm, args[0], vm->str_iter);
        if (IS_NULL(iter_method)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Argument to $vec() is not iterable.");
            return NULL_VAL();
        }

        // Call the object's :$iter() method to get an iterator.
        pyro_push(vm, args[0]); // receiver for the $iter() method call
        Value iterator = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        pyro_push(vm, iterator); // protect from GC

        // Get the iterator's :$next() method.
        Value next_method = pyro_get_method(vm, iterator, vm->str_next);
        if (IS_NULL(next_method)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid iterator -- no :$next() method.");
            return NULL_VAL();
        }
        pyro_push(vm, next_method); // protect from GC

        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        pyro_push(vm, OBJ_VAL(vec)); // protect from GC

        while (true) {
            pyro_push(vm, iterator); // receiver for the :$next() method call
            Value next_value = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return NULL_VAL();
            }
            if (IS_ERR(next_value)) {
                break;
            }
            pyro_push(vm, next_value); // protect from GC
            if (!ObjVec_append(vec, next_value, vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            return OBJ_VAL(vec);
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid size argument for $vec().");
            return NULL_VAL();
        }
    }

    else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 2 arguments to $vec(), found %i.", arg_count);
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
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
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments to :sort(), found %d.", arg_count);
        return NULL_VAL();
    }

    if (!ObjVec_mergesort(vec, compare_func, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(vec);
}


static Value vec_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_op_compare_eq(vm, vec->values[i], args[0])) {
            return BOOL_VAL(true);
        }
    }

    return BOOL_VAL(false);
}


static Value vec_index_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    for (size_t i = 0; i < vec->count; i++) {
        if (pyro_op_compare_eq(vm, vec->values[i], args[0])) {
            return I64_VAL((int64_t)i);
        }
    }

    return OBJ_VAL(vm->empty_error);
}


static Value vec_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjVec* new_vec = ObjVec_new_with_cap(vec->count, vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
        }
    }

    pyro_pop(vm);
    return OBJ_VAL(new_vec);
}


static Value vec_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)vec, ITER_VEC, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(iter);
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
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
        return NULL_VAL();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }

    return ObjVec_remove_at_index(vec, args[0].as.i64, vm);
}


static Value vec_insert_at_index(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
        return NULL_VAL();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
    }

    ObjVec_insert_at_index(vec, args[0].as.i64, args[1], vm);
    return NULL_VAL();
}


static Value vec_first(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[0];
    }
    pyro_panic(vm, ERR_VALUE_ERROR, "Vector is empty.");
    return NULL_VAL();
}


static Value vec_last(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[vec->count - 1];
    }
    pyro_panic(vm, ERR_VALUE_ERROR, "Vector is empty.");
    return NULL_VAL();
}


static Value vec_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return BOOL_VAL(vec->count == 0);
}


static Value fn_stack(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = ObjVec_new_as_stack(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(vec);
}


static Value fn_is_stack(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_STACK(args[0]));
}


static Value stack_pop(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count == 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Cannot pop empty stack.");
        return NULL_VAL();
    }
    vec->count--;
    return vec->values[vec->count];
}


void pyro_load_std_core_vec(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$vec", fn_vec, -1);
    pyro_define_global_fn(vm, "$is_vec", fn_is_vec, 1);
    pyro_define_global_fn(vm, "$stack", fn_stack, 0);
    pyro_define_global_fn(vm, "$is_stack", fn_is_stack, 1);

    // Vector methods.
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
    pyro_define_method(vm, vm->vec_class, "remove_last", vec_remove_last, 0);
    pyro_define_method(vm, vm->vec_class, "remove_first", vec_remove_first, 0);
    pyro_define_method(vm, vm->vec_class, "remove_at", vec_remove_at_index, 1);
    pyro_define_method(vm, vm->vec_class, "insert_at", vec_insert_at_index, 2);
    pyro_define_method(vm, vm->vec_class, "$iter", vec_iter, 0);
    pyro_define_method(vm, vm->vec_class, "first", vec_first, 0);
    pyro_define_method(vm, vm->vec_class, "last", vec_last, 0);
    pyro_define_method(vm, vm->vec_class, "is_empty", vec_is_empty, 0);

    // Stack methods.
    pyro_define_method(vm, vm->stack_class, "count", vec_count, 0);
    pyro_define_method(vm, vm->stack_class, "is_empty", vec_is_empty, 0);
    pyro_define_method(vm, vm->stack_class, "push", vec_append, 1);
    pyro_define_method(vm, vm->stack_class, "pop", stack_pop, 0);
    pyro_define_method(vm, vm->stack_class, "$iter", vec_iter, 0);
}
