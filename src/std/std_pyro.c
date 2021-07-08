#include "std_pyro.h"

#include "../vm/vm.h"


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return I64_VAL((int64_t)vm->bytes_allocated);
}


void pyro_load_std_pyro(PyroVM* vm) {
    ObjModule* mod_pyro = pyro_define_module_2(vm, "$std", "pyro");
    pyro_define_member_fn(vm, mod_pyro, "memory", fn_memory, 0);
}

