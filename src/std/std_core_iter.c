#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/errors.h"



static Value fn_is_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_ITER(args[0]));
}


static Value fn_iter(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_ITER(args[0])) {
        return args[0];
    }

    // If the argument is an iterator, wrap it in an ObjIter instance.
    if (IS_OBJ(args[0]) && pyro_has_method(args[0], vm->str_next)) {
        ObjIter* iter = ObjIter_new(AS_OBJ(args[0]), ITER_GENERIC, vm);
        if (!iter) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(iter);
    }

    // If the argument is iterable, call its :iter() method.
    Value iter_method = pyro_get_method(args[0], vm->str_iter);
    if (!IS_NULL(iter_method)) {
        pyro_push(vm, args[0]);
        Value result = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }

        if (IS_ITER(result)) {
            return result;
        }

        if (IS_OBJ(result) && pyro_has_method(result, vm->str_next)) {
            pyro_push(vm, result);
            ObjIter* iter = ObjIter_new(AS_OBJ(result), ITER_GENERIC, vm);
            if (!iter) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            pyro_pop(vm);
            return OBJ_VAL(iter);
        }
    }

    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $iter().");
    return NULL_VAL();
}


static Value iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);
    Value result = ObjIter_next(iter, vm);
    return result;
}


static Value iter_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :map(), expected a callable.");
        return NULL_VAL();
    }

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_MAP_FUNC, vm);
    if (!new_iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return OBJ_VAL(new_iter);
}


static Value iter_filter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :filter(), expected a callable.");
        return NULL_VAL();
    }

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_FILTER_FUNC, vm);
    if (!new_iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return OBJ_VAL(new_iter);
}


static Value iter_to_vec(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);

    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(vec));

    while (true) {
        Value value = ObjIter_next(iter, vm);
        if (IS_ERR(value)) {
            break;
        }

        pyro_push(vm, value);
        if (!ObjVec_append(vec, value, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        pyro_pop(vm); // value
    }

    pyro_pop(vm); // vec
    return OBJ_VAL(vec);
}


static Value iter_enum(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* src_iter = AS_ITER(args[-1]);

    ObjIter* new_iter = ObjIter_new((Obj*)src_iter, ITER_ENUM, vm);
    if (!new_iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    if (arg_count == 0) {
        new_iter->next_enum = 0;
    } else if (arg_count == 1) {
        if (IS_I64(args[0])) {
            new_iter->next_enum = args[0].as.i64;
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument for :enum(), expected an integer.");
            return NULL_VAL();
        }
    } else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for :enum().");
        return NULL_VAL();
    }

    return OBJ_VAL(new_iter);
}


static Value fn_range(PyroVM* vm, size_t arg_count, Value* args) {
    int64_t start, stop, step;

    if (arg_count == 1) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = 0;
        stop = args[0].as.i64;
        step = 1;
    } else if (arg_count == 2) {
        if (!IS_I64(args[0]) || !IS_I64(args[1])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = 1;
    } else if (arg_count == 3) {
        if (!IS_I64(args[0]) || !IS_I64(args[1]) || !IS_I64(args[2])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $range().");
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = args[2].as.i64;
    } else {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1, 2, or 3 arguments for $range().");
        return NULL_VAL();
    }

    ObjIter* iter = ObjIter_new(NULL, ITER_RANGE, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    iter->range_next = start;
    iter->range_stop = stop;
    iter->range_step = step;

    return OBJ_VAL(iter);
}


void pyro_load_std_core_iter(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$iter", fn_iter, 1);
    pyro_define_global_fn(vm, "$is_iter", fn_is_iter, 1);
    pyro_define_global_fn(vm, "$range", fn_range, -1);

    // Methods.
    pyro_define_method(vm, vm->iter_class, "$iter", iter_iter, 0);
    pyro_define_method(vm, vm->iter_class, "$next", iter_next, 0);
    pyro_define_method(vm, vm->iter_class, "map", iter_map, 1);
    pyro_define_method(vm, vm->iter_class, "filter", iter_filter, 1);
    pyro_define_method(vm, vm->iter_class, "to_vec", iter_to_vec, 0);
    pyro_define_method(vm, vm->iter_class, "enum", iter_enum, -1);
}
