#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/os.h"
#include "../../inc/stringify.h"


static Value fn_exists(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "exists(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    return pyro_make_bool(pyro_exists(AS_STR(args[0])->bytes));
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "is_file(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    return pyro_make_bool(pyro_is_file(AS_STR(args[0])->bytes));
}


static Value fn_is_dir(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "is_dir(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    return pyro_make_bool(pyro_is_dir(AS_STR(args[0])->bytes));
}


static Value fn_is_symlink(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "is_symlink(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    return pyro_make_bool(pyro_is_symlink(AS_STR(args[0])->bytes));
}


static Value fn_dirname(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "dirname(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "dirname(): out of memory");
        return pyro_make_null();
    }

    char* result = pyro_dirname(path_copy);
    if (!result) {
        pyro_panic(vm, "dirname(): out of memory");
        free(path_copy);
        return pyro_make_null();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "dirname(): out of memory");
        free(path_copy);
        return pyro_make_null();
    }

    free(path_copy);
    return MAKE_OBJ(output);
}


static Value fn_basename(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "basename(): invalid argument [path], expected a string");
        return pyro_make_null();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "basename(): out of memory");
        return pyro_make_null();
    }

    char* result = pyro_basename(path_copy);
    if (!result) {
        pyro_panic(vm, "basename(): out of memory");
        free(path_copy);
        return pyro_make_null();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "basename(): out of memory");
        free(path_copy);
        return pyro_make_null();
    }

    free(path_copy);
    return MAKE_OBJ(output);
}


static Value fn_join(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "join(): expected at least 2 arguments");
        return pyro_make_null();
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!IS_STR(args[i])) {
            pyro_panic(vm, "join(): invalid argument (%zu), expected a string", i + 1);
            return pyro_make_null();
        }
    }

    char* array = NULL;
    size_t array_count = 0;
    size_t array_capacity = 0;

    for (size_t i = 0; i < arg_count; i++) {
        ObjStr* arg = AS_STR(args[i]);
        if (arg->length == 0) {
            continue;
        }

        // Do we need to add a '/'?
        bool needs_sep = array_count > 0 && array[array_count - 1] != '/' && arg->bytes[0] != '/';

        // Always add 1 to allow for a terminating '\0'.
        size_t new_capacity = needs_sep ? array_count + arg->length + 2 : array_count + arg->length + 1;

        char* new_array = REALLOCATE_ARRAY(vm, char, array, array_capacity, new_capacity);
        if (!new_array) {
            if (array) {
                FREE_ARRAY(vm, char, array, array_capacity);
            }
            pyro_panic(vm, "join(): out of memory");
            return pyro_make_null();
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
        return MAKE_OBJ(vm->empty_string);
    }

    array[array_count] = '\0';

    ObjStr* output = ObjStr_take(array, array_count, vm);
    if (!output) {
        FREE_ARRAY(vm, char, array, array_capacity);
        pyro_panic(vm, "join(): out of memory");
        return pyro_make_null();
    }

    return MAKE_OBJ(output);
}


static Value fn_rm(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "rm(): invalid argument [path], expected a string");
        return pyro_make_null();
    }

    ObjStr* path = AS_STR(args[0]);

    if (pyro_rmrf(path->bytes) != 0) {
        pyro_panic(vm, "rm(): unable to delete '%s'", path->bytes);
    }

    return pyro_make_null();
}


static Value fn_cwd(PyroVM* vm, size_t arg_count, Value* args) {
    char* cwd = pyro_getcwd();
    if (!cwd) {
        pyro_panic(vm, "cwd(): out of memory");
        return pyro_make_null();
    }

    ObjStr* string = ObjStr_copy_raw(cwd, strlen(cwd), vm);
    free(cwd);

    if (!string) {
        pyro_panic(vm, "cwd(): out of memory");
        return pyro_make_null();
    }

    return MAKE_OBJ(string);
}


static Value fn_listdir(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "listdir(): invalid argument [path], expected a string");
        return pyro_make_null();
    }

    ObjVec* vec = pyro_listdir(vm, AS_STR(args[0])->bytes);
    if (vm->halt_flag) {
        return pyro_make_null();
    }

    return MAKE_OBJ(vec);
}


static Value fn_realpath(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "realpath(): invalid argument [path], expected a string");
        return pyro_make_null();
    }

    char* result = pyro_realpath(AS_STR(args[0])->bytes);
    if (!result) {
        ObjStr* message = pyro_sprintf_to_obj(vm,
            "realpath(): invalid path '%s'",
            AS_STR(args[0])->bytes
        );
        if (!message) {
            return pyro_make_null();
        }

        ObjErr* err = ObjErr_new(vm);
        if (!err) {
            return pyro_make_null();
        }

        err->message = message;
        return MAKE_OBJ(err);
    }

    ObjStr* string = ObjStr_copy_raw(result, strlen(result), vm);
    free(result);
    if (!string) {
        pyro_panic(vm, "realpath(): out of memory");
        return pyro_make_null();
    }

    return MAKE_OBJ(string);
}


static Value fn_cd(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "cd(): invalid argument [path], expected a string");
        return pyro_make_null();
    }

    ObjStr* path = AS_STR(args[0]);
    if (!pyro_cd(path->bytes)) {
        pyro_panic(vm, "cd(): failed to change current working directory to '%s'", path->bytes);
        return pyro_make_null();
    }

    return pyro_make_null();
}


void pyro_load_std_mod_path(PyroVM* vm, ObjModule* module) {
    pyro_define_pub_member_fn(vm, module, "exists", fn_exists, 1);
    pyro_define_pub_member_fn(vm, module, "is_file", fn_is_file, 1);
    pyro_define_pub_member_fn(vm, module, "is_dir", fn_is_dir, 1);
    pyro_define_pub_member_fn(vm, module, "is_symlink", fn_is_symlink, 1);
    pyro_define_pub_member_fn(vm, module, "dirname", fn_dirname, 1);
    pyro_define_pub_member_fn(vm, module, "basename", fn_basename, 1);
    pyro_define_pub_member_fn(vm, module, "join", fn_join, -1);
    pyro_define_pub_member_fn(vm, module, "rm", fn_rm, 1);
    pyro_define_pub_member_fn(vm, module, "cwd", fn_cwd, 0);
    pyro_define_pub_member_fn(vm, module, "listdir", fn_listdir, 1);
    pyro_define_pub_member_fn(vm, module, "realpath", fn_realpath, 1);
    pyro_define_pub_member_fn(vm, module, "cd", fn_cd, 1);
}
