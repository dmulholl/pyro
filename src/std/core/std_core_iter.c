#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value fn_is_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_ITER(args[0]));
}


static Value fn_iter(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_ITER(args[0])) {
        return args[0];
    }

    // If the argument is an iterator, wrap it in an ObjIter instance.
    if (IS_OBJ(args[0]) && pyro_has_method(vm, args[0], vm->str_dollar_next)) {
        ObjIter* iter = ObjIter_new(AS_OBJ(args[0]), ITER_GENERIC, vm);
        if (!iter) {
            pyro_panic(vm, "$iter(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(iter);
    }

    // If the argument is iterable, call its :iter() method.
    Value iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (!IS_NULL(iter_method)) {
        pyro_push(vm, args[0]);
        Value result = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }

        if (IS_ITER(result)) {
            return result;
        }

        if (IS_OBJ(result) && pyro_has_method(vm, result, vm->str_dollar_next)) {
            pyro_push(vm, result);
            ObjIter* iter = ObjIter_new(AS_OBJ(result), ITER_GENERIC, vm);
            if (!iter) {
                pyro_panic(vm, "$iter(): out of memory");
                return MAKE_NULL();
            }
            pyro_pop(vm);
            return MAKE_OBJ(iter);
        }
    }

    pyro_panic(vm, "$iter(): invalid argument [arg], expected an iterator or an iterable object");
    return MAKE_NULL();
}


static Value iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);
    return ObjIter_next(iter, vm);
}


static Value iter_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, "map(): invalid argument [callback], expected a callable");
        return MAKE_NULL();
    }

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_FUNC_MAP, vm);
    if (!new_iter) {
        pyro_panic(vm, "map(): out of memory");
        return MAKE_NULL();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return MAKE_OBJ(new_iter);
}


static Value iter_filter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, "filter(): invalid argument [callback], expected a callable");
        return MAKE_NULL();
    }

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_FUNC_FILTER, vm);
    if (!new_iter) {
        pyro_panic(vm, "filter(): out of memory");
        return MAKE_NULL();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return MAKE_OBJ(new_iter);
}


static Value iter_to_vec(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "to_vec(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(vec));

    while (true) {
        Value next_value = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        if (IS_ERR(next_value)) {
            break;
        }

        pyro_push(vm, next_value);
        if (!ObjVec_append(vec, next_value, vm)) {
            pyro_panic(vm, "to_vec(): out of memory");
            return MAKE_NULL();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // vec
    return MAKE_OBJ(vec);
}


static Value iter_join(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    if (arg_count == 0) {
        ObjStr* result = ObjIter_join(iter, "", 0, vm);
        return vm->halt_flag ? MAKE_NULL() : MAKE_OBJ(result);
    }

    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "join(): invalid argument [sep], expected a string");
            return MAKE_NULL();
        }
        ObjStr* sep = AS_STR(args[0]);
        ObjStr* result = ObjIter_join(iter, sep->bytes, sep->length, vm);
        return vm->halt_flag ? MAKE_NULL() : MAKE_OBJ(result);
    }

    pyro_panic(vm, "join(): expected 0 or 1 arguments, found %zu", arg_count);
    return MAKE_NULL();
}


static Value iter_to_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    ObjMap* map = ObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, "to_set(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(map));

    while (true) {
        Value next_value = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        if (IS_ERR(next_value)) {
            break;
        }

        pyro_push(vm, next_value);
        if (ObjMap_set(map, next_value, MAKE_NULL(), vm) == 0) {
            pyro_panic(vm, "to_set(): out of memory");
            return MAKE_NULL();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    return MAKE_OBJ(map);
}


static Value iter_enumerate(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_ENUM, vm);
    if (!new_iter) {
        pyro_panic(vm, "enumerate(): out of memory");
        return MAKE_NULL();
    }

    if (arg_count == 0) {
        new_iter->next_enum = 0;
    } else if (arg_count == 1) {
        if (IS_I64(args[0])) {
            new_iter->next_enum = args[0].as.i64;
        } else {
            pyro_panic(vm, "enumerate(): invalid argument [start_index], expected an integer");
            return MAKE_NULL();
        }
    } else {
        pyro_panic(vm, "enumerate(): expected 0 or 1 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    return MAKE_OBJ(new_iter);
}


static Value fn_range(PyroVM* vm, size_t arg_count, Value* args) {
    int64_t start, stop, step;

    if (arg_count == 1) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return MAKE_NULL();
        }
        start = 0;
        stop = args[0].as.i64;
        step = 1;
    } else if (arg_count == 2) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [start], expected an integer");
            return MAKE_NULL();
        }
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return MAKE_NULL();
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = 1;
    } else if (arg_count == 3) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [start], expected an integer");
            return MAKE_NULL();
        }
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return MAKE_NULL();
        }
        if (!IS_I64(args[2])) {
            pyro_panic(vm, "$range(): invalid argument [step], expected an integer");
            return MAKE_NULL();
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = args[2].as.i64;
    } else {
        pyro_panic(vm, "$range(): expected 1, 2, or 3 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    ObjIter* iter = ObjIter_new(NULL, ITER_RANGE, vm);
    if (!iter) {
        pyro_panic(vm, "$range(): out of memory");
        return MAKE_NULL();
    }

    iter->range_next = start;
    iter->range_stop = stop;
    iter->range_step = step;

    return MAKE_OBJ(iter);
}


static Value iter_skip_first(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "skip_first(): invalid argument [n], expected an integer");
        return MAKE_NULL();
    }

    int64_t num_to_skip = args[0].as.i64;
    if (num_to_skip == 0) {
        return MAKE_OBJ(iter);
    } else if (num_to_skip < 0) {
        pyro_panic(vm, "skip_first(): invalid argument [n], expected a positive integer");
        return MAKE_NULL();
    }

    int64_t num_skipped = 0;

    while (num_skipped < num_to_skip) {
        Value result = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return MAKE_NULL();
        } else if (IS_ERR(result)) {
            pyro_panic(
                vm,
                "skip_first(): failed to skip first %d items, iterator exhausted after %d items",
                num_to_skip,
                num_skipped
            );
            return MAKE_NULL();
        }
        num_skipped++;
    }

    return MAKE_OBJ(iter);
}


static Value iter_skip_last(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "skip_last(): invalid argument [n], expected an integer");
        return MAKE_NULL();
    }

    int64_t num_to_skip = args[0].as.i64;
    if (num_to_skip == 0) {
        return MAKE_OBJ(iter);
    } else if (num_to_skip < 0) {
        pyro_panic(vm, "skip_last(): invalid argument [n], expected a positive integer");
        return MAKE_NULL();
    }

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "skip_last(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(vec));

    while (true) {
        Value value = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        if (IS_ERR(value)) {
            break;
        }

        pyro_push(vm, value);
        if (!ObjVec_append(vec, value, vm)) {
            pyro_panic(vm, "skip_last(): out of memory");
            return MAKE_NULL();
        }
        pyro_pop(vm); // value
    }

    if (vec->count < (size_t)num_to_skip) {
        pyro_panic(
            vm,
            "skip_last(): failed to skip last %d items, iterator exhausted after %d",
            num_to_skip,
            vec->count
        );
        return MAKE_NULL();
    }

    vec->count -= num_to_skip;

    ObjIter* new_iter = ObjIter_new((Obj*)vec, ITER_VEC, vm);
    if (!new_iter) {
        pyro_panic(vm, "skip_last(): out of memory");
        return MAKE_NULL();
    }

    pyro_pop(vm); // vec
    return MAKE_OBJ(new_iter);
}


static Value iter_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);
    int64_t count = 0;

    while (true) {
        Value result = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return MAKE_NULL();
        } else if (IS_ERR(result)) {
            break;
        }
        count++;
    }

    return MAKE_I64(count);
}


void pyro_load_std_core_iter(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$iter", fn_iter, 1);
    pyro_define_global_fn(vm, "$is_iter", fn_is_iter, 1);
    pyro_define_global_fn(vm, "$range", fn_range, -1);

    // Methods.
    pyro_define_method(vm, vm->class_iter, "$iter", iter_iter, 0);
    pyro_define_method(vm, vm->class_iter, "$next", iter_next, 0);
    pyro_define_method(vm, vm->class_iter, "map", iter_map, 1);
    pyro_define_method(vm, vm->class_iter, "filter", iter_filter, 1);
    pyro_define_method(vm, vm->class_iter, "to_vec", iter_to_vec, 0);
    pyro_define_method(vm, vm->class_iter, "to_set", iter_to_set, 0);
    pyro_define_method(vm, vm->class_iter, "enumerate", iter_enumerate, -1);
    pyro_define_method(vm, vm->class_iter, "skip_first", iter_skip_first, 1);
    pyro_define_method(vm, vm->class_iter, "skip_last", iter_skip_last, 1);
    pyro_define_method(vm, vm->class_iter, "join", iter_join, -1);
    pyro_define_method(vm, vm->class_iter, "count", iter_count, 0);
    pyro_define_method(vm, vm->class_iter, "next", iter_next, 0);
}
