#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../lib/mt64/mt64.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"


static Value fn_rand_float(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_F64(pyro_mt64_gen_f64b(vm->mt64));
}


static Value fn_rand_int(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "$std::prng::rand_int(): invalid argument [n], expected a positive integer");
        return MAKE_NULL();
    }
    return MAKE_I64(pyro_mt64_gen_int(vm->mt64, (uint64_t)args[0].as.i64));
}


static Value fn_rand_int_in_range(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0])) {
        pyro_panic(vm, "$std::prng::rand_int_in_range(): invalid argument [lower], expected an integer");
        return MAKE_NULL();
    }

    if (!IS_I64(args[1])) {
        pyro_panic(vm, "$std::prng::rand_int_in_range(): invalid argument [upper], expected an integer");
        return MAKE_NULL();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)pyro_mt64_gen_int(vm->mt64, delta);
    return MAKE_I64(rand_int);
}


void pyro_load_std_mod_prng(PyroVM* vm, ObjModule* module) {
    pyro_define_member_fn(vm, module, "rand_int", fn_rand_int, 1);
    pyro_define_member_fn(vm, module, "rand_int_in_range", fn_rand_int_in_range, 2);
    pyro_define_member_fn(vm, module, "rand_float", fn_rand_float, 0);
}
