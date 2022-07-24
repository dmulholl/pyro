#include "../src/inc/pyro.h"
#include "../src/inc/operators.h"
#include "../src/inc/setup.h"

static Value fn_add(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_op_binary_plus(vm, args[0], args[1]);
}

bool pyro_init_mod_compiled_module(PyroVM* vm, ObjModule* module) {
    return pyro_define_member_fn(vm, module, "add", fn_add, 2);
}
