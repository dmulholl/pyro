#include "../../inc/pyro.h"


static PyroValue fn_exists(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "exists(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_exists(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_file(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_file(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_dir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_dir(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_dir(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_is_symlink(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "is_symlink(): invalid argument [path], expected a string");
        return pyro_null();
    }
    return pyro_bool(pyro_is_symlink(PYRO_AS_STR(args[0])->bytes));
}


static PyroValue fn_dirname(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "dirname(): invalid argument [path], expected a string");
        return pyro_null();
    }
    char* path = PYRO_AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "dirname(): out of memory");
        return pyro_null();
    }

    char* result = pyro_dirname(path_copy);
    if (!result) {
        pyro_panic(vm, "dirname(): out of memory");
        free(path_copy);
        return pyro_null();
    }

    PyroStr* output = PyroStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "dirname(): out of memory");
        free(path_copy);
        return pyro_null();
    }

    free(path_copy);
    return pyro_obj(output);
}


static PyroValue fn_basename(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "basename(): invalid argument [path], expected a string");
        return pyro_null();
    }
    char* path = PYRO_AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "basename(): out of memory");
        return pyro_null();
    }

    char* result = pyro_basename(path_copy);
    if (!result) {
        pyro_panic(vm, "basename(): out of memory");
        free(path_copy);
        return pyro_null();
    }

    PyroStr* output = PyroStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "basename(): out of memory");
        free(path_copy);
        return pyro_null();
    }

    free(path_copy);
    return pyro_obj(output);
}


static PyroValue fn_join(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "join(): expected at least 2 arguments");
        return pyro_null();
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!PYRO_IS_STR(args[i])) {
            pyro_panic(vm, "join(): invalid argument (%zu), expected a string", i + 1);
            return pyro_null();
        }
    }

    char* array = NULL;
    size_t array_count = 0;
    size_t array_capacity = 0;

    for (size_t i = 0; i < arg_count; i++) {
        PyroStr* arg = PYRO_AS_STR(args[i]);
        if (arg->length == 0) {
            continue;
        }

        // Do we need to add a '/'?
        bool needs_sep = array_count > 0 && array[array_count - 1] != '/' && arg->bytes[0] != '/';

        // Always add 1 to allow for a terminating '\0'.
        size_t new_capacity = needs_sep ? array_count + arg->length + 2 : array_count + arg->length + 1;

        char* new_array = PYRO_REALLOCATE_ARRAY(vm, char, array, array_capacity, new_capacity);
        if (!new_array) {
            if (array) {
                PYRO_FREE_ARRAY(vm, char, array, array_capacity);
            }
            pyro_panic(vm, "join(): out of memory");
            return pyro_null();
        }
        array = new_array;
        array_capacity = new_capacity;

        if (needs_sep) {
            array[array_count] = '/';
            array_count++;
        }

        memcpy(array + array_count, arg->bytes, arg->length);
        array_count += arg->length;
    }

    if (!array) {
        return pyro_obj(vm->empty_string);
    }

    array[array_count] = '\0';

    PyroStr* output = PyroStr_take(array, array_count, vm);
    if (!output) {
        PYRO_FREE_ARRAY(vm, char, array, array_capacity);
        pyro_panic(vm, "join(): out of memory");
        return pyro_null();
    }

    return pyro_obj(output);
}


static PyroValue fn_remove(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "rm(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);

    if (pyro_remove(path->bytes) != 0) {
        pyro_panic(vm, "rm(): unable to delete '%s'", path->bytes);
    }

    return pyro_null();
}


static PyroValue fn_getcwd(PyroVM* vm, size_t arg_count, PyroValue* args) {
    char* cwd = pyro_getcwd();
    if (!cwd) {
        pyro_panic(vm, "getcwd(): out of memory");
        return pyro_null();
    }

    PyroStr* string = PyroStr_copy_raw(cwd, strlen(cwd), vm);
    free(cwd);

    if (!string) {
        pyro_panic(vm, "getcwd(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_listdir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "listdir(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroVec* vec = pyro_listdir(vm, PYRO_AS_STR(args[0])->bytes);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(vec);
}


static PyroValue fn_realpath(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "realpath(): invalid argument [path], expected a string");
        return pyro_null();
    }

    char* result = pyro_realpath(PYRO_AS_STR(args[0])->bytes);
    if (!result) {
        PyroStr* message = pyro_sprintf_to_obj(vm,
            "realpath(): invalid path '%s'",
            PYRO_AS_STR(args[0])->bytes
        );
        if (!message) {
            return pyro_null();
        }

        PyroErr* err = PyroErr_new(vm);
        if (!err) {
            return pyro_null();
        }

        err->message = message;
        return pyro_obj(err);
    }

    PyroStr* string = PyroStr_copy_raw(result, strlen(result), vm);
    free(result);
    if (!string) {
        pyro_panic(vm, "realpath(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_chdir(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "chdir(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    if (!pyro_chdir(path->bytes)) {
        pyro_panic(vm, "chdir(): failed to change the current working directory to '%s'", path->bytes);
        return pyro_null();
    }

    return pyro_null();
}


static PyroValue fn_chroot(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "chroot(): invalid argument [path], expected a string");
        return pyro_null();
    }

    PyroStr* path = PYRO_AS_STR(args[0]);
    if (!pyro_chroot(path->bytes)) {
        pyro_panic(vm, "chroot(): failed to change root directory to '%s'", path->bytes);
        return pyro_null();
    }

    return pyro_null();
}


void pyro_load_std_mod_path(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "exists", fn_exists, 1);
    pyro_define_pub_member_fn(vm, module, "is_file", fn_is_file, 1);
    pyro_define_pub_member_fn(vm, module, "is_dir", fn_is_dir, 1);
    pyro_define_pub_member_fn(vm, module, "is_symlink", fn_is_symlink, 1);
    pyro_define_pub_member_fn(vm, module, "dirname", fn_dirname, 1);
    pyro_define_pub_member_fn(vm, module, "basename", fn_basename, 1);
    pyro_define_pub_member_fn(vm, module, "join", fn_join, -1);
    pyro_define_pub_member_fn(vm, module, "remove", fn_remove, 1);
    pyro_define_pub_member_fn(vm, module, "listdir", fn_listdir, 1);
    pyro_define_pub_member_fn(vm, module, "realpath", fn_realpath, 1);
    pyro_define_pub_member_fn(vm, module, "chdir", fn_chdir, 1);
    pyro_define_pub_member_fn(vm, module, "chroot", fn_chroot, 1);
    pyro_define_pub_member_fn(vm, module, "getcwd", fn_getcwd, 0);

    // Deprecated.
    pyro_define_pub_member_fn(vm, module, "cd", fn_chdir, 1);
    pyro_define_pub_member_fn(vm, module, "cwd", fn_getcwd, 0);
    pyro_define_pub_member_fn(vm, module, "rm", fn_remove, 1);
}
