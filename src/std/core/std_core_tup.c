#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/operators.h"


static Value fn_tup(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = ObjTup_new(arg_count, vm);
    if (tup == NULL) {
        pyro_panic(vm, "$tup(): out of memory");
        return pyro_make_null();
    }
    memcpy(tup->values, (void*)args, sizeof(Value) * arg_count);
    return MAKE_OBJ(tup);
}


static Value fn_is_tup(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_TUP(args[0]));
}


static Value tup_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    return MAKE_I64(tup->count);
}


static Value tup_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    if (IS_I64(args[0])) {
        int64_t index = args[0].as.i64;
        if (index >= 0 && (size_t)index < tup->count) {
            return tup->values[index];
        }
        pyro_panic(vm, "get(): invalid argument [index], integer is out of range");
        return pyro_make_null();
    }
    pyro_panic(vm, "get(): invalid argument [index], expected an integer");
    return pyro_make_null();
}


static Value tup_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)tup, ITER_TUP, vm);
    if (!iter) {
        return pyro_make_null();
    }
    return MAKE_OBJ(iter);
}


static Value tup_slice(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);

    if (!(arg_count == 1 || arg_count == 2)) {
        pyro_panic(vm, "slice(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_make_null();
    }

    if (!IS_I64(args[0])) {
        pyro_panic(vm, "slice(): invalid argument [start_index], expected an integer");
        return pyro_make_null();
    }

    size_t start_index;
    if (args[0].as.i64 >= 0 && (size_t)args[0].as.i64 <= tup->count) {
        start_index = (size_t)args[0].as.i64;
    } else if (args[0].as.i64 < 0 && (size_t)(args[0].as.i64 * -1) <= tup->count) {
        start_index = (size_t)((int64_t)tup->count + args[0].as.i64);
    } else {
        pyro_panic(vm, "slice(): invalid argument [start_index], integer (%d) is out of range", args[0].as.i64);
        return pyro_make_null();
    }

    size_t length = tup->count - start_index;
    if (arg_count == 2) {
        if (!IS_I64(args[1])) {
            pyro_panic(vm, "slice(): invalid argument [length], expected an integer");
            return pyro_make_null();
        }
        if (args[1].as.i64 < 0) {
            pyro_panic(vm, "slice(): invalid argument [length], expected a positive integer");
            return pyro_make_null();
        }
        if (start_index + (size_t)args[1].as.i64 > tup->count) {
            pyro_panic(vm, "slice(): invalid argument [length], integer (%d) is out of range", args[1].as.i64);
            return pyro_make_null();
        }
        length = (size_t)args[1].as.i64;
    }

    ObjTup* new_tup = ObjTup_new(length, vm);
    if (!new_tup) {
        pyro_panic(vm, "slice(): out of memory");
        return pyro_make_null();
    }

    if (length == 0) {
        return MAKE_OBJ(new_tup);
    }

    memcpy(new_tup->values, &tup->values[start_index], sizeof(Value) * length);
    return MAKE_OBJ(new_tup);
}


static Value tup_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);

    for (size_t i = 0; i < tup->count; i++) {
        if (pyro_op_compare_eq(vm, tup->values[i], args[0])) {
            return pyro_make_bool(true);
        }
    }

    return pyro_make_bool(false);
}


void pyro_load_std_core_tup(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$tup", fn_tup, -1);
    pyro_define_global_fn(vm, "$is_tup", fn_is_tup, 1);

    // Methods.
    pyro_define_pri_method(vm, vm->class_tup, "$contains", tup_contains, 1);
    pyro_define_pri_method(vm, vm->class_tup, "$iter", tup_iter, 0);
    pyro_define_pub_method(vm, vm->class_tup, "count", tup_count, 0);
    pyro_define_pub_method(vm, vm->class_tup, "slice", tup_slice, -1);
    pyro_define_pub_method(vm, vm->class_tup, "get", tup_get, 1);
    pyro_define_pub_method(vm, vm->class_tup, "iter", tup_iter, 0);
    pyro_define_pub_method(vm, vm->class_tup, "contains", tup_contains, 1);
}
