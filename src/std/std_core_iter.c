#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/errors.h"


static Value iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjIter* iter = AS_ITER(args[-1]);
    return ObjIter_next(iter, vm);
}


void pyro_load_std_core_iter(PyroVM* vm) {
    // Functions.
    /* pyro_define_global_fn(vm, "$vec", fn_vec, -1); */
    /* pyro_define_global_fn(vm, "$is_vec", fn_is_vec, 1); */

    // Methods.
    pyro_define_method(vm, vm->iter_class, "$iter", iter_iter, 0);
    pyro_define_method(vm, vm->iter_class, "$next", iter_next, 0);
}
