#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/errors.h"


static Value fn_exists(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::exists(), expected a string.");
        return NULL_VAL();
    }
    return BOOL_VAL(pyro_exists(AS_STR(args[0])->bytes));
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::is_file(), expected a string.");
        return NULL_VAL();
    }
    return BOOL_VAL(pyro_is_file(AS_STR(args[0])->bytes));
}


static Value fn_is_dir(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::is_dir(), expected a string.");
        return NULL_VAL();
    }
    return BOOL_VAL(pyro_is_dir(AS_STR(args[0])->bytes));
}


static Value fn_is_symlink(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::is_symlink(), expected a string.");
        return NULL_VAL();
    }
    return BOOL_VAL(pyro_is_symlink(AS_STR(args[0])->bytes));
}


static Value fn_dirname(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::dirname(), expected a string.");
        return NULL_VAL();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = strdup(path);
    if (!path_copy) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    char* result = pyro_dirname(path_copy);
    if (!result) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(path_copy);
        return NULL_VAL();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(path_copy);
        return NULL_VAL();
    }

    free(path_copy);
    return OBJ_VAL(output);
}


static Value fn_basename(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::basename(), expected a string.");
        return NULL_VAL();
    }
    char* path = AS_STR(args[0])->bytes;

    char* path_copy = strdup(path);
    if (!path_copy) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    char* result = pyro_basename(path_copy);
    if (!result) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(path_copy);
        return NULL_VAL();
    }

    ObjStr* output = ObjStr_copy_raw(result, strlen(result), vm);
    if (!output) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(path_copy);
        return NULL_VAL();
    }

    free(path_copy);
    return OBJ_VAL(output);
}


static Value fn_join(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count < 2) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected at least 2 arguments for $std::path::join().");
        return NULL_VAL();
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!IS_STR(args[i])) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::path::join(), expected a string.");
            return NULL_VAL();
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
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
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
        return OBJ_VAL(vm->empty_string);
    }

    array[array_count] = '\0';

    ObjStr* output = ObjStr_take(array, array_count, vm);
    if (!output) {
        FREE_ARRAY(vm, char, array, array_capacity);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(output);
}


void pyro_load_std_path(PyroVM* vm) {
    ObjModule* mod_path = pyro_define_module_2(vm, "$std", "path");
    if (!mod_path) {
        return;
    }

    pyro_define_member_fn(vm, mod_path, "exists", fn_exists, 1);
    pyro_define_member_fn(vm, mod_path, "is_file", fn_is_file, 1);
    pyro_define_member_fn(vm, mod_path, "is_dir", fn_is_dir, 1);
    pyro_define_member_fn(vm, mod_path, "is_symlink", fn_is_symlink, 1);
    pyro_define_member_fn(vm, mod_path, "dirname", fn_dirname, 1);
    pyro_define_member_fn(vm, mod_path, "basename", fn_basename, 1);
    pyro_define_member_fn(vm, mod_path, "join", fn_join, -1);
}
