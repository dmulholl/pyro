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
    return I64_VAL(pyro_mt64_gen_int(vm->mt64, (uint64_t)args[0].as.i64));
}


static Value fn_rand_int_in_range(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_I64(args[0]) || !IS_I64(args[1])) {
        pyro_panic(vm, "Invalid argument to $std::prng::rand_int_in_range().");
        return NULL_VAL();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)pyro_mt64_gen_int(vm->mt64, delta);
    return I64_VAL(rand_int);
}


// This uses the Fisher-Yates/Durstenfeld algorithm for randomly shuffling an array.
static Value fn_shuffle(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_VEC(args[0])) {
        pyro_panic(vm, "Argument to $std::prng::shuffle() must be a vector.");
        return NULL_VAL();
    }
    ObjVec* vec = AS_VEC(args[0]);

    // Iterate over the array and at each index choose randomly from the remaining
    // unshuffled entries.
    for (size_t i = 0; i < vec->count; i++) {
        // Choose [j] from the half-open interval [i, vec->count).
        size_t j = i + pyro_mt64_gen_int(vm->mt64, vec->count - i);
        Value temp = vec->values[i];
        vec->values[i] = vec->values[j];
        vec->values[j] = temp;
    }
    return NULL_VAL();
}


void pyro_load_std_prng(PyroVM* vm) {
    ObjModule* mod_prng = pyro_define_module_2(vm, "$std", "prng");

    pyro_define_member_fn(vm, mod_prng, "test_mt64", fn_test_mt64, 0);
    pyro_define_member_fn(vm, mod_prng, "rand_int", fn_rand_int, 1);
    pyro_define_member_fn(vm, mod_prng, "rand_int_in_range", fn_rand_int_in_range, 2);
    pyro_define_member_fn(vm, mod_prng, "rand_float", fn_rand_float, 0);
    pyro_define_member_fn(vm, mod_prng, "seed", fn_seed, 1);
    pyro_define_member_fn(vm, mod_prng, "shuffle", fn_shuffle, 1);
}

