#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/operators.h"
#include "../vm/setup.h"
#include "../vm/sorting.h"
#include "../vm/panics.h"
#include "../vm/exec.h"


static Value fn_vec(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return MAKE_NULL();
        }
        return MAKE_OBJ(vec);
    }

    else if (arg_count == 1) {
        // Does the object have an :$iter() method?
        Value iter_method = pyro_get_method(vm, args[0], vm->str_iter);
        if (IS_NULL(iter_method)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Argument to $vec() is not iterable.");
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
        Value next_method = pyro_get_method(vm, iterator, vm->str_next);
        if (IS_NULL(next_method)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid iterator -- no :$next() method.");
            return MAKE_NULL();
        }
        pyro_push(vm, next_method); // protect from GC

        ObjVec* vec = ObjVec_new(vm);
        if (!vec) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return MAKE_NULL();
            }
            return MAKE_OBJ(vec);
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid size argument for $vec().");
            return MAKE_NULL();
        }
    }

    else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 2 arguments to $vec(), found %i.", arg_count);
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
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

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :sort(), found %d.", arg_count);
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

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :mergesort(), found %d.", arg_count);
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

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :quicksort(), found %d.", arg_count);
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
        return MAKE_NULL();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }

    return ObjVec_remove_at_index(vec, args[0].as.i64, vm);
}


static Value vec_insert_at_index(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index type, expected an integer.");
        return MAKE_NULL();
    }

    if (args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
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
    pyro_panic(vm, ERR_VALUE_ERROR, "Vector is empty.");
    return MAKE_NULL();
}


static Value vec_last(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    if (vec->count > 0) {
        return vec->values[vec->count - 1];
    }
    pyro_panic(vm, ERR_VALUE_ERROR, "Vector is empty.");
    return MAKE_NULL();
}


static Value vec_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);
    return MAKE_BOOL(vec->count == 0);
}


static Value fn_stack(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = ObjVec_new_as_stack(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Cannot pop empty stack.");
        return MAKE_NULL();
    }
    vec->count--;
    return vec->values[vec->count];
}


static Value vec_slice(PyroVM* vm, size_t arg_count, Value* args) {
    ObjVec* vec = AS_VEC(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or 2 arguments for :slice(), found %i.", arg_count);
        return MAKE_NULL();
    }

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument type, start_index must be an integer.");
        return MAKE_NULL();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= vec->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= vec->count) {
        start_index = (size_t)((int64_t)vec->count + args[0].as.i64);
    } else {
        pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, start_index is out of range.");
        return MAKE_NULL();
    }

    size_t length = vec->count - start_index;
    if (arg_count == 2) {
        if (!IS_I64(args[1])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument type, length must be an integer.");
            return MAKE_NULL();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, length cannot be negative.");
            return MAKE_NULL();
        }
        if (start_index + (size_t)args[1].as.i64 > vec->count) {
            pyro_panic(vm, ERR_VALUE_ERROR, "Invalid argument value, length is out of range.");
            return MAKE_NULL();
        }
        length = (size_t)args[1].as.i64;
    }

    ObjVec* new_vec = ObjVec_new(vm);
    if (!new_vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }

    if (length == 0) {
        return MAKE_OBJ(new_vec);
    }

    pyro_push(vm, MAKE_OBJ(new_vec));
    Value* new_array = ALLOCATE_ARRAY(vm, Value, length);
    if (!new_array) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :is_sorted(), found %d.", arg_count);
    return MAKE_NULL();
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
    pyro_define_method(vm, vm->vec_class, "reverse", vec_reverse, 0);
    pyro_define_method(vm, vm->vec_class, "sort", vec_sort, -1);
    pyro_define_method(vm, vm->vec_class, "mergesort", vec_mergesort, -1);
    pyro_define_method(vm, vm->vec_class, "quicksort", vec_quicksort, -1);
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
    pyro_define_method(vm, vm->vec_class, "slice", vec_slice, -1);
    pyro_define_method(vm, vm->vec_class, "is_sorted", vec_is_sorted, -1);

    // Stack methods.
    pyro_define_method(vm, vm->stack_class, "count", vec_count, 0);
    pyro_define_method(vm, vm->stack_class, "is_empty", vec_is_empty, 0);
    pyro_define_method(vm, vm->stack_class, "push", vec_append, 1);
    pyro_define_method(vm, vm->stack_class, "pop", stack_pop, 0);
    pyro_define_method(vm, vm->stack_class, "$iter", vec_iter, 0);
}
