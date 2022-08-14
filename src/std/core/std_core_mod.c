#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value mod_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);
    Value value;
    if (ObjMap_get(mod->globals, args[0], &value, vm)) {
        return value;
    }
    return MAKE_OBJ(vm->empty_error);
}


static Value mod_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);
    Value value;
    return MAKE_BOOL(ObjMap_get(mod->globals, args[0], &value, vm));
}


static Value mod_globals(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);
    ObjMap* copy = ObjMap_copy(mod->globals, vm);
    if (!copy) {
        pyro_panic(vm, "globals(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(copy);
}


void pyro_load_std_core_mod(PyroVM* vm) {
    // Module methods.
    pyro_define_pri_method(vm, vm->class_module, "$contains", mod_contains, 1);
    pyro_define_pub_method(vm, vm->class_module, "get", mod_get, 1);
    pyro_define_pub_method(vm, vm->class_module, "contains", mod_contains, 1);
    pyro_define_pub_method(vm, vm->class_module, "globals", mod_globals, 0);
}
