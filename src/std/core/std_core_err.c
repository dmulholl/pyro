#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/operators.h"
#include "../../inc/stringify.h"


static Value fn_err(PyroVM* vm, size_t arg_count, Value* args) {
    ObjErr* err = ObjErr_new(vm);
    if (err == NULL) {
        pyro_panic(vm, "$err(): out of memory");
        return MAKE_NULL();
    }

    if (arg_count == 0) {
        return MAKE_OBJ(err);
    } else if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        err->message = string;
        return MAKE_OBJ(err);
    } else {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$err(): invalid argument [format_string], expected a string");
            return MAKE_NULL();
        }
        Value formatted = pyro_fn_fmt(vm, arg_count, args);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        err->message = AS_STR(formatted);
        return MAKE_OBJ(err);
    }
}


static Value fn_is_err(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_ERR(args[0]));
}


static Value err_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjErr* err = AS_ERR(args[-1]);
    if (ObjMap_set(err->details, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "err:set(): out of memory");
    }
    return MAKE_NULL();
}


static Value err_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjErr* err = AS_ERR(args[-1]);
    Value value;
    if (ObjMap_get(err->details, args[0], &value, vm)) {
        return value;
    }
    return MAKE_OBJ(vm->empty_error);
}


static Value err_message(PyroVM* vm, size_t arg_count, Value* args) {
    ObjErr* err = AS_ERR(args[-1]);
    return MAKE_OBJ(err->message);
}


static Value err_details(PyroVM* vm, size_t arg_count, Value* args) {
    ObjErr* err = AS_ERR(args[-1]);
    return MAKE_OBJ(err->details);
}


void pyro_load_std_core_err(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$err", fn_err, -1);
    pyro_define_global_fn(vm, "$is_err", fn_is_err, 1);

    // Methods.
    pyro_define_method(vm, vm->class_err, "$get_index", err_get, 1);
    pyro_define_method(vm, vm->class_err, "$set_index", err_set, 2);
    pyro_define_method(vm, vm->class_err, "message", err_message, 0);
    pyro_define_method(vm, vm->class_err, "details", err_details, 0);
}
