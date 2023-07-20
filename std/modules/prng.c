#include "../../inc/pyro.h"


static PyroValue fn_rand_int(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_int(): invalid argument [n], expected a positive integer");
        return pyro_null();
    }

    uint64_t rand_u64 = pyro_xoshiro256ss_next_in_range(&vm->prng_state, (uint64_t)args[0].as.i64);
    return pyro_i64(rand_u64);
}


static PyroValue fn_rand_float(PyroVM* vm, size_t arg_count, PyroValue* args) {
    uint64_t rand_u64 = pyro_xoshiro256ss_next(&vm->prng_state);

    // Generates a uniformly-distributed random double on the half-open interval [0.0, 1.0).
    // The divisor is 2^53.
    double rand_64 = (rand_u64 >> 11) * (1.0/9007199254740992.0);
    return pyro_f64(rand_64);
}


void pyro_load_std_mod_prng(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "rand_int", fn_rand_int, 1);
    pyro_define_pub_member_fn(vm, module, "rand_float", fn_rand_float, 0);
}
