#include "std_lib.h"

#include "../vm/vm.h"
#include "../vm/heap.h"
#include "../vm/setup.h"
#include "../vm/panics.h"


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_I64((int64_t)vm->bytes_allocated);
}


static Value fn_gc(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_collect_garbage(vm);
    return MAKE_NULL();
}


void pyro_load_std_pyro(PyroVM* vm) {
    ObjModule* mod_pyro = pyro_define_module_2(vm, "$std", "pyro");
    if (!mod_pyro) {
        return;
    }

    ObjTup* version_tuple = ObjTup_new(3, vm);
    if (!version_tuple) {
        return;
    }
    version_tuple->values[0] = MAKE_I64(PYRO_VERSION_MAJOR);
    version_tuple->values[1] = MAKE_I64(PYRO_VERSION_MINOR);
    version_tuple->values[2] = MAKE_I64(PYRO_VERSION_PATCH);
    pyro_define_member(vm, mod_pyro, "version", MAKE_OBJ(version_tuple));

    ObjStr* version_string = STR(PYRO_VERSION_STRING);
    if (!version_string) {
        return;
    }
    pyro_define_member(vm, mod_pyro, "version_string", MAKE_OBJ(version_string));

    pyro_define_member_fn(vm, mod_pyro, "memory", fn_memory, 0);
    pyro_define_member_fn(vm, mod_pyro, "gc", fn_memory, 0);
}
