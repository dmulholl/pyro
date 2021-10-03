#include "std_pyro.h"

#include "../vm/vm.h"
#include "../vm/heap.h"


static Value fn_hash(PyroVM* vm, size_t arg_count, Value* args) {
    return I64_VAL((int64_t)pyro_hash_value(args[0]));
}


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return I64_VAL((int64_t)vm->bytes_allocated);
}


static Value fn_gc(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_collect_garbage(vm);
    return NULL_VAL();
}


void pyro_load_std_pyro(PyroVM* vm) {
    ObjModule* mod_pyro = pyro_define_module_2(vm, "$std", "pyro");

    pyro_define_member(vm, mod_pyro, "import_roots", OBJ_VAL(vm->import_roots));

    pyro_define_member_fn(vm, mod_pyro, "hash", fn_hash, 1);
    pyro_define_member_fn(vm, mod_pyro, "memory", fn_memory, 0);
    pyro_define_member_fn(vm, mod_pyro, "gc", fn_memory, 0);
}

