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


static PyroValue fn_err(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjErr* err = ObjErr_new(vm);
    if (err == NULL) {
        pyro_panic(vm, "$err(): out of memory");
        return pyro_null();
    }

    if (arg_count == 0) {
        return pyro_obj(err);
    } else if (arg_count == 1) {
        ObjStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        err->message = string;
        return pyro_obj(err);
    } else {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$err(): invalid argument [format_string], expected a string");
            return pyro_null();
        }
        PyroValue formatted = pyro_fn_fmt(vm, arg_count, args);
        if (vm->halt_flag) {
            return pyro_null();
        }
        err->message = AS_STR(formatted);
        return pyro_obj(err);
    }
}


static PyroValue fn_is_err(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(IS_ERR(args[0]));
}


static PyroValue err_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjErr* err = AS_ERR(args[-1]);
    if (ObjMap_set(err->details, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "err:set(): out of memory");
    }
    return pyro_null();
}


static PyroValue err_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjErr* err = AS_ERR(args[-1]);
    PyroValue value;
    if (ObjMap_get(err->details, args[0], &value, vm)) {
        return value;
    }
    return pyro_obj(vm->error);
}


static PyroValue err_message(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjErr* err = AS_ERR(args[-1]);
    return pyro_obj(err->message);
}


static PyroValue err_details(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjErr* err = AS_ERR(args[-1]);
    return pyro_obj(err->details);
}


void pyro_load_std_core_err(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$err", fn_err, -1);
    pyro_define_global_fn(vm, "$is_err", fn_is_err, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_err, "$get_index", err_get, 1);
    pyro_define_pri_method(vm, vm->class_err, "$set_index", err_set, 2);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_err, "message", err_message, 0);
    pyro_define_pub_method(vm, vm->class_err, "details", err_details, 0);
}
