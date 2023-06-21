#include "../../inc/pyro.h"


static PyroValue fn_err(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroErr* err = PyroErr_new(vm);
    if (err == NULL) {
        pyro_panic(vm, "$err(): out of memory");
        return pyro_null();
    }

    if (arg_count == 0) {
        return pyro_obj(err);
    }

    if (arg_count == 1) {
        PyroStr* string = pyro_stringify_value(vm, args[0]);
        if (vm->halt_flag) {
            return pyro_null();
        }
        err->message = string;
        return pyro_obj(err);
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "$err(): invalid argument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$err()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    err->message = string;
    return pyro_obj(err);
}


static PyroValue fn_is_err(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_ERR(args[0]));
}


static PyroValue err_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroErr* err = PYRO_AS_ERR(args[-1]);
    if (PyroMap_set(err->details, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "err:set(): out of memory");
    }
    return pyro_null();
}


static PyroValue err_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroErr* err = PYRO_AS_ERR(args[-1]);
    PyroValue value;
    if (PyroMap_get(err->details, args[0], &value, vm)) {
        return value;
    }
    return pyro_obj(vm->error);
}


static PyroValue err_message(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroErr* err = PYRO_AS_ERR(args[-1]);
    return pyro_obj(err->message);
}


static PyroValue err_details(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroErr* err = PYRO_AS_ERR(args[-1]);
    return pyro_obj(err->details);
}


void pyro_load_std_builtins_err(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$err", fn_err, -1);
    pyro_define_superglobal_fn(vm, "$is_err", fn_is_err, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_err, "$get", err_get, 1);
    pyro_define_pri_method(vm, vm->class_err, "$set", err_set, 2);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_err, "message", err_message, 0);
    pyro_define_pub_method(vm, vm->class_err, "details", err_details, 0);
}
