#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../vm/utils.h"
#include "../../vm/heap.h"
#include "../../vm/utf8.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"


static Value fn_tup(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = ObjTup_new(arg_count, vm);
    if (tup == NULL) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    memcpy(tup->values, (void*)args, sizeof(Value) * arg_count);
    return MAKE_OBJ(tup);
}


static Value fn_is_tup(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_TUP(args[0]));
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return MAKE_NULL();
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid index, must be an integer.");
    return MAKE_NULL();
}


static Value tup_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjTup* tup = AS_TUP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)tup, ITER_TUP, vm);
    if (!iter) {
        return MAKE_NULL();
    }
    return MAKE_OBJ(iter);
}


static Value fn_err(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        return MAKE_OBJ(vm->empty_error);
    }
    ObjTup* tup = ObjTup_new_err(arg_count, vm);
    if (!tup) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    memcpy(tup->values, (void*)args, sizeof(Value) * arg_count);
    return MAKE_OBJ(tup);
}


static Value fn_is_err(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_ERR(args[0]));
}


void pyro_load_std_core_tup(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$tup", fn_tup, -1);
    pyro_define_global_fn(vm, "$is_tup", fn_is_tup, 1);
    pyro_define_global_fn(vm, "$err", fn_err, -1);
    pyro_define_global_fn(vm, "$is_err", fn_is_err, 1);

    // Methods.
    pyro_define_method(vm, vm->tup_class, "count", tup_count, 0);
    pyro_define_method(vm, vm->tup_class, "get", tup_get, 1);
    pyro_define_method(vm, vm->tup_class, "$iter", tup_iter, 0);
}