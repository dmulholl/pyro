#include "std_pyro.h"

#include "../vm/vm.h"
#include "../vm/heap.h"
#include "../vm/setup.h"


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return I64_VAL((int64_t)vm->bytes_allocated);
}


static Value fn_gc(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_collect_garbage(vm);
    return NULL_VAL();
}


void pyro_load_std_pyro(PyroVM* vm) {
    ObjModule* mod_pyro = pyro_define_module_2(vm, "$std", "pyro");
    if (!mod_pyro) {
        return;
    }

    ObjTup* version = ObjTup_new(3, vm);
    if (version) {
        version->values[0] = I64_VAL(PYRO_VERSION_MAJOR);
        version->values[1] = I64_VAL(PYRO_VERSION_MINOR);
        version->values[2] = I64_VAL(PYRO_VERSION_PATCH);
        pyro_define_member(vm, mod_pyro, "version", OBJ_VAL(version));
    }

    pyro_define_member_fn(vm, mod_pyro, "memory", fn_memory, 0);
    pyro_define_member_fn(vm, mod_pyro, "gc", fn_memory, 0);
}
