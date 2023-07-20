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
    double rand_f64 = (rand_u64 >> 11) * (1.0/9007199254740992.0);
    return pyro_f64(rand_f64);
}


static void free_xoshiro256ss_state(PyroVM* vm, void* pointer) {
    free(pointer);
    vm->bytes_allocated -= sizeof(pyro_xoshiro256ss_state_t);
}


static PyroValue generator_init(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    pyro_xoshiro256ss_state_t* state = malloc(sizeof(pyro_xoshiro256ss_state_t));
    if (!state) {
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    vm->bytes_allocated += sizeof(pyro_xoshiro256ss_state_t);
    pyro_xoshiro256ss_init(state, pyro_random_seed());

    PyroResourcePointer* rp = PyroResourcePointer_new(state, free_xoshiro256ss_state, vm);
    if (!rp) {
        free(state);
        vm->bytes_allocated -= sizeof(pyro_xoshiro256ss_state_t);
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    instance->fields[0] = pyro_obj(rp);
    return pyro_obj(instance);
}


static PyroValue generator_rand_int(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_int(): invalid argument [n], expected a positive integer");
        return pyro_null();
    }

    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* rp = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    pyro_xoshiro256ss_state_t* state = rp->pointer;

    uint64_t rand_u64 = pyro_xoshiro256ss_next_in_range(state, (uint64_t)args[0].as.i64);
    return pyro_i64(rand_u64);
}


static PyroValue generator_rand_float(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* rp = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    pyro_xoshiro256ss_state_t* state = rp->pointer;

    uint64_t rand_u64 = pyro_xoshiro256ss_next(state);

    // Generates a uniformly-distributed random double on the half-open interval [0.0, 1.0).
    // The divisor is 2^53.
    double rand_f64 = (rand_u64 >> 11) * (1.0/9007199254740992.0);
    return pyro_f64(rand_f64);
}


static PyroValue generator_seed(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "seed(): invalid argument, expected an integer");
        return pyro_null();
    }

    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* rp = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    pyro_xoshiro256ss_state_t* state = rp->pointer;

    pyro_xoshiro256ss_init(state, args[0].as.u64);
    return pyro_null();
}


void pyro_load_std_mod_prng(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "rand_int", fn_rand_int, 1);
    pyro_define_pub_member_fn(vm, module, "rand_float", fn_rand_float, 0);

    PyroClass* generator_class = PyroClass_new(vm);
    if (!generator_class) {
        return;
    }

    generator_class->name = PyroStr_COPY("Generator");
    pyro_define_pub_member(vm, module, "Generator", pyro_obj(generator_class));

    pyro_define_pub_field(vm, generator_class, "state", pyro_null());

    pyro_define_pri_method(vm, generator_class, "$init", generator_init, 0);
    pyro_define_pub_method(vm, generator_class, "rand_int", generator_rand_int, 1);
    pyro_define_pub_method(vm, generator_class, "rand_float", generator_rand_float, 0);
    pyro_define_pub_method(vm, generator_class, "seed", generator_seed, 1);
}
