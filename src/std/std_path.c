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



void pyro_load_std_path(PyroVM* vm) {
    ObjModule* mod_path = pyro_define_module_2(vm, "$std", "path");
    if (!mod_path) {
        return;
    }

    pyro_define_member_fn(vm, mod_path, "exists", fn_exists, 1);
    pyro_define_member_fn(vm, mod_path, "is_file", fn_is_file, 1);
    pyro_define_member_fn(vm, mod_path, "is_dir", fn_is_dir, 1);
    pyro_define_member_fn(vm, mod_path, "is_symlink", fn_is_symlink, 1);
}
