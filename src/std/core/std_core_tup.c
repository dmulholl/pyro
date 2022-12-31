#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/operators.h"


static PyroValue fn_tup(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = PyroObjTup_new(arg_count, vm);
    if (tup == NULL) {
        pyro_panic(vm, "$tup(): out of memory");
        return pyro_null();
    }
    memcpy(tup->values, (void*)args, sizeof(PyroValue) * arg_count);
    return pyro_obj(tup);
}


static PyroValue fn_is_tup(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_TUP(args[0]));
}


static PyroValue tup_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = AS_TUP(args[-1]);
    return pyro_i64(tup->count);
}


static PyroValue tup_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = AS_TUP(args[-1]);
    if (PYRO_IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < tup->count) {
            return tup->values[index];
        }
        pyro_panic(vm, "get(): invalid argument [index], integer is out of range");
        return pyro_null();
    }
    pyro_panic(vm, "get(): invalid argument [index], expected an integer");
    return pyro_null();
}


static PyroValue tup_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = AS_TUP(args[-1]);
    PyroObjIter* iter = PyroObjIter_new((PyroObj*)tup, PYRO_ITER_TUP, vm);
    if (!iter) {
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue tup_slice(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = AS_TUP(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, "slice(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "slice(): invalid argument [start_index], expected an integer");
        return pyro_null();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= tup->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= tup->count) {
        start_index = (size_t)((int64_t)tup->count + args[0].as.i64);
    } else {
        pyro_panic(vm, "slice(): invalid argument [start_index], integer (%d) is out of range", args[0].as.i64);
        return pyro_null();
    }

    size_t length = tup->count - start_index;
    if (arg_count == 2) {
        if (!PYRO_IS_I64(args[1])) {
            pyro_panic(vm, "slice(): invalid argument [length], expected an integer");
            return pyro_null();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, "slice(): invalid argument [length], expected a positive integer");
            return pyro_null();
        }
        if (start_index + (size_t)args[1].as.i64 > tup->count) {
            pyro_panic(vm, "slice(): invalid argument [length], integer (%d) is out of range", args[1].as.i64);
            return pyro_null();
        }
        length = (size_t)args[1].as.i64;
    }

    PyroObjTup* new_tup = PyroObjTup_new(length, vm);
    if (!new_tup) {
        pyro_panic(vm, "slice(): out of memory");
        return pyro_null();
    }

    if (length == 0) {
        return pyro_obj(new_tup);
    }

    memcpy(new_tup->values, &tup->values[start_index], sizeof(PyroValue) * length);
    return pyro_obj(new_tup);
}


static PyroValue tup_contains(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjTup* tup = AS_TUP(args[-1]);

    for (size_t i = 0; i < tup->count; i++) {
        if (pyro_op_compare_eq(vm, tup->values[i], args[0])) {
            return pyro_bool(true);
        }
    }

    return pyro_bool(false);
}


void pyro_load_std_core_tup(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$tup", fn_tup, -1);
    pyro_define_global_fn(vm, "$is_tup", fn_is_tup, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_tup, "$contains", tup_contains, 1);
    pyro_define_pri_method(vm, vm->class_tup, "$iter", tup_iter, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_tup, "count", tup_count, 0);
    pyro_define_pub_method(vm, vm->class_tup, "slice", tup_slice, -1);
    pyro_define_pub_method(vm, vm->class_tup, "get", tup_get, 1);
    pyro_define_pub_method(vm, vm->class_tup, "iter", tup_iter, 0);
    pyro_define_pub_method(vm, vm->class_tup, "contains", tup_contains, 1);
}
