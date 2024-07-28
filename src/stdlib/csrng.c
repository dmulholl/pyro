#include "../includes/pyro.h"


static PyroValue fn_rand_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_bytes(): invalid argument [n], expected a positive integer");
        return pyro_null();
    }

    PyroBuf* buf = pyro_csrng_rand_bytes(vm, (size_t)args[0].as.i64, "rand_bytes()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(buf);
}


static PyroValue fn_rand_i64(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroBuf* buf = pyro_csrng_rand_bytes(vm, 8, "rand_bytes()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    int64_t result = *((int64_t*)buf->bytes);
    return pyro_i64(result);
}


void pyro_load_stdlib_module_csrng(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "rand_bytes", fn_rand_bytes, 1);
    pyro_define_pub_member_fn(vm, module, "rand_i64", fn_rand_i64, 0);
}
