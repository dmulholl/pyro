#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../lib/mt64/mt64.h"


static Value fn_rand_float(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_f64(pyro_mt64_gen_f64b(vm->mt64));
}


static Value fn_rand_int(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_int(): invalid argument [n], expected a positive integer");
        return pyro_make_null();
    }
    return pyro_make_i64(pyro_mt64_gen_int(vm->mt64, (uint64_t)args[0].as.i64));
}


static Value fn_rand_int_in_range(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0])) {
        pyro_panic(vm, "rand_int_in_range(): invalid argument [lower], expected an integer");
        return pyro_make_null();
    }

    if (!IS_I64(args[1])) {
        pyro_panic(vm, "rand_int_in_range(): invalid argument [upper], expected an integer");
        return pyro_make_null();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)pyro_mt64_gen_int(vm->mt64, delta);
    return pyro_make_i64(rand_int);
}


void pyro_load_std_mod_prng(PyroVM* vm, ObjModule* module) {
    pyro_define_pub_member_fn(vm, module, "rand_int", fn_rand_int, 1);
    pyro_define_pub_member_fn(vm, module, "rand_int_in_range", fn_rand_int_in_range, 2);
    pyro_define_pub_member_fn(vm, module, "rand_float", fn_rand_float, 0);
}
