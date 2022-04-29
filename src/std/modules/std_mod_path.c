#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../vm/utils.h"
#include "../../vm/heap.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"
#include "../../vm/os.h"


static Value fn_exists(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::exists(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_exists(AS_STR(args[0])->bytes));
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::is_file(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_is_file(AS_STR(args[0])->bytes));
}


static Value fn_is_dir(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::is_dir(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_is_dir(AS_STR(args[0])->bytes));
}


static Value fn_is_symlink(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::is_symlink(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    return MAKE_BOOL(pyro_is_symlink(AS_STR(args[0])->bytes));
}


static Value fn_dirname(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::dirname(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "$std::path::dirname(): out of memory");
        return MAKE_NULL();
    }

    char* result = pyro_dirname(path_copy);
    if (!result) {
        pyro_panic(vm, "$std::path::dirname(): out of memory");
        free(path_copy);
        return MAKE_NULL();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "$std::path::dirname(): out of memory");
        free(path_copy);
        return MAKE_NULL();
    }

    free(path_copy);
    return MAKE_OBJ(output);
}


static Value fn_basename(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::basename(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = pyro_strdup(path);
    if (!path_copy) {
        pyro_panic(vm, "$std::path::basename(): out of memory");
        return MAKE_NULL();
    }

    char* result = pyro_basename(path_copy);
    if (!result) {
        pyro_panic(vm, "$std::path::basename(): out of memory");
        free(path_copy);
        return MAKE_NULL();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, "$std::path::basename(): out of memory");
        free(path_copy);
        return MAKE_NULL();
    }

    free(path_copy);
    return MAKE_OBJ(output);
}


static Value fn_join(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, "$std::path::join(): expected at least 2 arguments");
        return MAKE_NULL();
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!IS_STR(args[i])) {
            pyro_panic(vm, "$std::path::join(): invalid argument (%zu), expected a string", i + 1);
            return MAKE_NULL();
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
            pyro_panic(vm, "$std::path::join(): out of memory");
            return MAKE_NULL();
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
        pyro_panic(vm, "$std::path::join(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(output);
}


static Value fn_rm(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::rm(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    ObjStr* path = AS_STR(args[0]);

    if (pyro_rmrf(path->bytes) != 0) {
        pyro_panic(vm, "$std::path::rm(): unable to delete '%s'", path->bytes);
    }

    return MAKE_NULL();
}


static Value fn_cwd(PyroVM* vm, size_t arg_count, Value* args) {
    char* cwd = pyro_getcwd();
    if (!cwd) {
        pyro_panic(vm, "$std::path::cwd(): out of memory");
        return MAKE_NULL();
    }

    ObjStr* string = ObjStr_copy_raw(cwd, strlen(cwd), vm);
    free(cwd);

    if (!string) {
        pyro_panic(vm, "$std::path::cwd(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(string);
}


static Value fn_listdir(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::listdir(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    ObjVec* vec = pyro_listdir(vm, AS_STR(args[0])->bytes);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }

    return MAKE_OBJ(vec);
}


static Value fn_realpath(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::realpath(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    char* realpath = pyro_realpath(AS_STR(args[0])->bytes);
    if (!realpath) {
        pyro_panic(vm, "$std::path::realpath(): unable to resolve path '%s'", AS_STR(args[0])->bytes);
        return MAKE_NULL();
    }

    ObjStr* string = ObjStr_copy_raw(realpath, strlen(realpath), vm);
    free(realpath);

    if (!string) {
        pyro_panic(vm, "$std::path::realpath(): out of memory");
        return MAKE_NULL();
    }

    return MAKE_OBJ(string);
}


static Value fn_cd(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, "$std::path::cd(): invalid argument [path], expected a string");
        return MAKE_NULL();
    }

    ObjStr* path = AS_STR(args[0]);
    if (!pyro_cd(path->bytes)) {
        pyro_panic(vm, "$std::path::cd(): failed to change current working directory to '%s'", path->bytes);
        return MAKE_NULL();
    }

    return MAKE_NULL();
}


void pyro_load_std_mod_path(PyroVM* vm, ObjModule* module) {
    pyro_define_member_fn(vm, module, "exists", fn_exists, 1);
    pyro_define_member_fn(vm, module, "is_file", fn_is_file, 1);
    pyro_define_member_fn(vm, module, "is_dir", fn_is_dir, 1);
    pyro_define_member_fn(vm, module, "is_symlink", fn_is_symlink, 1);
    pyro_define_member_fn(vm, module, "dirname", fn_dirname, 1);
    pyro_define_member_fn(vm, module, "basename", fn_basename, 1);
    pyro_define_member_fn(vm, module, "join", fn_join, -1);
    pyro_define_member_fn(vm, module, "rm", fn_rm, 1);
    pyro_define_member_fn(vm, module, "cwd", fn_cwd, 0);
    pyro_define_member_fn(vm, module, "listdir", fn_listdir, 1);
    pyro_define_member_fn(vm, module, "realpath", fn_realpath, 1);
    pyro_define_member_fn(vm, module, "cd", fn_cd, 1);
}
