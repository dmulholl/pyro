#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"
#include "../../inc/operators.h"


static PyroValue fn_is_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(IS_ITER(args[0]));
}


static PyroValue fn_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (IS_ITER(args[0])) {
        return args[0];
    }

    // If the argument is an iterator, wrap it in an PyroObjIter instance.
    if (IS_OBJ(args[0]) && pyro_has_method(vm, args[0], vm->str_dollar_next)) {
        PyroObjIter* iter = PyroObjIter_new(AS_OBJ(args[0]), PYRO_ITER_GENERIC, vm);
        if (!iter) {
            pyro_panic(vm, "$iter(): out of memory");
            return pyro_null();
        }
        return pyro_obj(iter);
    }

    // If the argument is iterable, call its :iter() method.
    PyroValue iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (!IS_NULL(iter_method)) {
        pyro_push(vm, args[0]);
        PyroValue result = pyro_call_method(vm, iter_method, 0);
        if (vm->halt_flag) {
            return pyro_null();
        }

        if (IS_ITER(result)) {
            return result;
        }

        if (IS_OBJ(result) && pyro_has_method(vm, result, vm->str_dollar_next)) {
            pyro_push(vm, result);
            PyroObjIter* iter = PyroObjIter_new(AS_OBJ(result), PYRO_ITER_GENERIC, vm);
            if (!iter) {
                pyro_panic(vm, "$iter(): out of memory");
                return pyro_null();
            }
            pyro_pop(vm);
            return pyro_obj(iter);
        }
    }

    pyro_panic(vm, "$iter(): invalid argument [arg], expected an iterator or an iterable object");
    return pyro_null();
}


static PyroValue iter_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return args[-1];
}


static PyroValue iter_next(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);
    return PyroObjIter_next(iter, vm);
}


static PyroValue iter_map(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, "map(): invalid argument [callback], expected a callable");
        return pyro_null();
    }

    PyroObjIter* new_iter = PyroObjIter_new((PyroObj*)src_iter, PYRO_ITER_FUNC_MAP, vm);
    if (!new_iter) {
        pyro_panic(vm, "map(): out of memory");
        return pyro_null();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return pyro_obj(new_iter);
}


static PyroValue iter_filter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* src_iter = AS_ITER(args[-1]);

    if (!IS_OBJ(args[0])) {
        pyro_panic(vm, "filter(): invalid argument [callback], expected a callable");
        return pyro_null();
    }

    PyroObjIter* new_iter = PyroObjIter_new((PyroObj*)src_iter, PYRO_ITER_FUNC_FILTER, vm);
    if (!new_iter) {
        pyro_panic(vm, "filter(): out of memory");
        return pyro_null();
    }

    new_iter->callback = AS_OBJ(args[0]);
    return pyro_obj(new_iter);
}


static PyroValue iter_to_vec(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);

    PyroObjVec* vec = PyroObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "to_vec(): out of memory");
        return pyro_null();
    }
    pyro_push(vm, pyro_obj(vec));

    while (true) {
        PyroValue next_value = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (IS_ERR(next_value)) {
            break;
        }

        pyro_push(vm, next_value);
        if (!PyroObjVec_append(vec, next_value, vm)) {
            pyro_panic(vm, "to_vec(): out of memory");
            return pyro_null();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // vec
    return pyro_obj(vec);
}


static PyroValue iter_join(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);

    if (arg_count == 0) {
        PyroObjStr* result = PyroObjIter_join(iter, "", 0, vm);
        return vm->halt_flag ? pyro_null() : pyro_obj(result);
    }

    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "join(): invalid argument [sep], expected a string");
            return pyro_null();
        }
        PyroObjStr* sep = AS_STR(args[0]);
        PyroObjStr* result = PyroObjIter_join(iter, sep->bytes, sep->length, vm);
        return vm->halt_flag ? pyro_null() : pyro_obj(result);
    }

    pyro_panic(vm, "join(): expected 0 or 1 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue iter_to_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);

    PyroObjMap* map = PyroObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, "to_set(): out of memory");
        return pyro_null();
    }
    pyro_push(vm, pyro_obj(map));

    while (true) {
        PyroValue next_value = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (IS_ERR(next_value)) {
            break;
        }

        pyro_push(vm, next_value);
        if (PyroObjMap_set(map, next_value, pyro_null(), vm) == 0) {
            pyro_panic(vm, "to_set(): out of memory");
            return pyro_null();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    return pyro_obj(map);
}


static PyroValue iter_enumerate(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* src_iter = AS_ITER(args[-1]);

    PyroObjIter* new_iter = PyroObjIter_new((PyroObj*)src_iter, PYRO_ITER_ENUM, vm);
    if (!new_iter) {
        pyro_panic(vm, "enumerate(): out of memory");
        return pyro_null();
    }

    if (arg_count == 0) {
        new_iter->next_enum = 0;
    } else if (arg_count == 1) {
        if (IS_I64(args[0])) {
            new_iter->next_enum = args[0].as.i64;
        } else {
            pyro_panic(vm, "enumerate(): invalid argument [start_index], expected an integer");
            return pyro_null();
        }
    } else {
        pyro_panic(vm, "enumerate(): expected 0 or 1 arguments, found %zu", arg_count);
        return pyro_null();
    }

    return pyro_obj(new_iter);
}


static PyroValue fn_range(PyroVM* vm, size_t arg_count, PyroValue* args) {
    int64_t start, stop, step;

    if (arg_count == 1) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return pyro_null();
        }
        start = 0;
        stop = args[0].as.i64;
        step = 1;
    } else if (arg_count == 2) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [start], expected an integer");
            return pyro_null();
        }
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return pyro_null();
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = 1;
    } else if (arg_count == 3) {
        if (!IS_I64(args[0])) {
            pyro_panic(vm, "$range(): invalid argument [start], expected an integer");
            return pyro_null();
        }
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "$range(): invalid argument [stop], expected an integer");
            return pyro_null();
        }
        if (!IS_I64(args[2])) {
            pyro_panic(vm, "$range(): invalid argument [step], expected an integer");
            return pyro_null();
        }
        start = args[0].as.i64;
        stop = args[1].as.i64;
        step = args[2].as.i64;
    } else {
        pyro_panic(vm, "$range(): expected 1, 2, or 3 arguments, found %zu", arg_count);
        return pyro_null();
    }

    PyroObjIter* iter = PyroObjIter_new(NULL, PYRO_ITER_RANGE, vm);
    if (!iter) {
        pyro_panic(vm, "$range(): out of memory");
        return pyro_null();
    }

    iter->range_next = start;
    iter->range_stop = stop;
    iter->range_step = step;

    return pyro_obj(iter);
}


static PyroValue iter_skip_first(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "skip_first(): invalid argument [n], expected an integer");
        return pyro_null();
    }

    int64_t num_to_skip = args[0].as.i64;
    if (num_to_skip == 0) {
        return pyro_obj(iter);
    } else if (num_to_skip < 0) {
        pyro_panic(vm, "skip_first(): invalid argument [n], expected a positive integer");
        return pyro_null();
    }

    int64_t num_skipped = 0;

    while (num_skipped < num_to_skip) {
        PyroValue result = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        } else if (IS_ERR(result)) {
            pyro_panic(
                vm,
                "skip_first(): failed to skip first %d items, iterator exhausted after %d items",
                num_to_skip,
                num_skipped
            );
            return pyro_null();
        }
        num_skipped++;
    }

    return pyro_obj(iter);
}


static PyroValue iter_skip_last(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "skip_last(): invalid argument [n], expected an integer");
        return pyro_null();
    }

    int64_t num_to_skip = args[0].as.i64;
    if (num_to_skip == 0) {
        return pyro_obj(iter);
    } else if (num_to_skip < 0) {
        pyro_panic(vm, "skip_last(): invalid argument [n], expected a positive integer");
        return pyro_null();
    }

    PyroObjVec* vec = PyroObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, "skip_last(): out of memory");
        return pyro_null();
    }
    pyro_push(vm, pyro_obj(vec));

    while (true) {
        PyroValue value = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (IS_ERR(value)) {
            break;
        }

        pyro_push(vm, value);
        if (!PyroObjVec_append(vec, value, vm)) {
            pyro_panic(vm, "skip_last(): out of memory");
            return pyro_null();
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
        return pyro_null();
    }

    vec->count -= num_to_skip;

    PyroObjIter* new_iter = PyroObjIter_new((PyroObj*)vec, PYRO_ITER_VEC, vm);
    if (!new_iter) {
        pyro_panic(vm, "skip_last(): out of memory");
        return pyro_null();
    }

    pyro_pop(vm); // vec
    return pyro_obj(new_iter);
}


static PyroValue iter_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);
    int64_t count = 0;

    while (true) {
        PyroValue result = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        } else if (IS_ERR(result)) {
            break;
        }
        count++;
    }

    return pyro_i64(count);
}


static PyroValue iter_sum(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);
    bool is_first_item = true;
    PyroValue sum = pyro_null();

    while (true) {
        PyroValue item = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        } else if (IS_ERR(item)) {
            break;
        }

        if (is_first_item) {
            sum = item;
        } else {
            sum = pyro_op_binary_plus(vm, sum, item);
            if (vm->halt_flag) {
                return pyro_null();
            }
        }

        is_first_item = false;
    }

    return sum;
}


static PyroValue iter_reduce(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjIter* iter = AS_ITER(args[-1]);
    PyroValue callback = args[0];
    PyroValue accumulator = args[1];

    while (true) {
        PyroValue item = PyroObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return pyro_null();
        } else if (IS_ERR(item)) {
            break;
        }

        pyro_push(vm, callback);
        pyro_push(vm, accumulator);
        pyro_push(vm, item);
        accumulator = pyro_call_function(vm, 2);
        if (vm->halt_flag) {
            return pyro_null();
        }
    }

    return accumulator;
}


void pyro_load_std_core_iter(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$iter", fn_iter, 1);
    pyro_define_global_fn(vm, "$is_iter", fn_is_iter, 1);
    pyro_define_global_fn(vm, "$range", fn_range, -1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_iter, "$iter", iter_iter, 0);
    pyro_define_pri_method(vm, vm->class_iter, "$next", iter_next, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_iter, "map", iter_map, 1);
    pyro_define_pub_method(vm, vm->class_iter, "filter", iter_filter, 1);
    pyro_define_pub_method(vm, vm->class_iter, "to_vec", iter_to_vec, 0);
    pyro_define_pub_method(vm, vm->class_iter, "to_set", iter_to_set, 0);
    pyro_define_pub_method(vm, vm->class_iter, "enumerate", iter_enumerate, -1);
    pyro_define_pub_method(vm, vm->class_iter, "skip_first", iter_skip_first, 1);
    pyro_define_pub_method(vm, vm->class_iter, "skip_last", iter_skip_last, 1);
    pyro_define_pub_method(vm, vm->class_iter, "join", iter_join, -1);
    pyro_define_pub_method(vm, vm->class_iter, "count", iter_count, 0);
    pyro_define_pub_method(vm, vm->class_iter, "next", iter_next, 0);
    pyro_define_pub_method(vm, vm->class_iter, "sum", iter_sum, 0);
    pyro_define_pub_method(vm, vm->class_iter, "reduce", iter_reduce, 2);
}
