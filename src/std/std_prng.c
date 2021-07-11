#include "std_prng.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../lib/mt64/mt64.h"


static Value fn_test_mt64(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(pyro_test_mt64());
}


static Value fn_rand_float(PyroVM* vm, size_t arg_count, Value* args) {
    return F64_VAL(pyro_mt64_gen_f64b(vm->mt64));
}


static Value fn_seed(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_init_mt64(vm->mt64, pyro_hash(args[0]));
    return NULL_VAL();
}


static Value fn_rand_int(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "Invalid argument to $std::prng::rand_int().");
        return NULL_VAL();
    }

    uint64_t n = (uint64_t)args[0].as.i64;

    if (n == 0 || n == 1) {
        return I64_VAL(0);
    }

    // Discarding values at or above [cutoff] avoids skew.
    uint64_t cutoff = (UINT64_MAX / n) * n;

    uint64_t rand;
    while ((rand = pyro_mt64_gen_u64(vm->mt64)) >= cutoff);

    return I64_VAL(rand % n);
}


void pyro_load_std_prng(PyroVM* vm) {
    ObjModule* mod_prng = pyro_define_module_2(vm, "$std", "prng");

    pyro_define_member_fn(vm, mod_prng, "test_mt64", fn_test_mt64, 0);
    pyro_define_member_fn(vm, mod_prng, "rand_int", fn_rand_int, 1);
    pyro_define_member_fn(vm, mod_prng, "rand_float", fn_rand_float, 0);
    pyro_define_member_fn(vm, mod_prng, "seed", fn_seed, 1);
}

