#include "../../../inc/pyro.h"


static PyroValue fn_test(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(mt64_test());
}


static void mt64_free_callback(PyroVM* vm, void* pointer) {
    free(pointer);
    vm->bytes_allocated -= sizeof(MT64);
}


static PyroValue mt64_init_method(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);

    MT64* mt64 = malloc(sizeof(MT64));
    if (!mt64) {
        pyro_panic(vm, "$std::mt64::MT4:$init(): out of memory");
        return pyro_null();
    }
    mt64_init(mt64);

    PyroResourcePointer* resource = PyroResourcePointer_new(mt64, mt64_free_callback, vm);
    if (!resource) {
        free(mt64);
        pyro_panic(vm, "$std::mt64::MT4:$init(): out of memory");
        return pyro_null();
    }

    vm->bytes_allocated += sizeof(MT64);
    instance->fields[0] = pyro_obj(resource);
    return pyro_obj(instance);
}


static PyroValue mt64_seed_with_hash(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* resource = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
    return pyro_null();
}


static PyroValue mt64_seed_with_array(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* resource = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (PYRO_IS_STR(args[0])) {
        PyroStr* string = PYRO_AS_STR(args[0]);
        if (string->count < 8) {
            mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            mt64_seed_with_byte_array(mt64, (uint8_t*)string->bytes, string->count);
        }
    } else if (PYRO_IS_BUF(args[0])) {
        PyroBuf* buf = PYRO_AS_BUF(args[0]);
        if (buf->count < 8) {
            mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            mt64_seed_with_byte_array(mt64, buf->bytes, buf->count);
        }
    } else {
        pyro_panic(vm, "seed_with_array(): invalid argument, expected a string or buffer");
    }

    return pyro_null();
}

static PyroValue mt64_rand_float(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* resource = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    return pyro_f64(mt64_gen_f64_co(mt64));
}


static PyroValue mt64_rand_int(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* resource = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_int(): invalid argument, expected a positive integer");
        return pyro_null();
    }

    return pyro_i64(mt64_gen_int(mt64, (uint64_t)args[0].as.i64));
}


static PyroValue mt64_rand_int_in_range(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroInstance* instance = PYRO_AS_INSTANCE(args[-1]);
    PyroResourcePointer* resource = PYRO_AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!PYRO_IS_I64(args[0]) || !PYRO_IS_I64(args[1])) {
        pyro_panic(vm, "rand_int_in_range(): invalid argument, expected an integer");
        return pyro_null();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)mt64_gen_int(mt64, delta);
    return pyro_i64(rand_int);
}


void pyro_load_std_mod_mt64(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "test", fn_test, 0);

    PyroClass* mt64_class = PyroClass_new(vm);
    if (!mt64_class) {
        return;
    }
    mt64_class->name = PyroStr_COPY("MT64");
    pyro_define_pub_member(vm, module, "MT64", pyro_obj(mt64_class));

    pyro_define_pub_field(vm, mt64_class, "generator", pyro_null());

    pyro_define_pri_method(vm, mt64_class, "$init", mt64_init_method, 0);
    pyro_define_pub_method(vm, mt64_class, "seed_with_hash", mt64_seed_with_hash, 1);
    pyro_define_pub_method(vm, mt64_class, "seed_with_array", mt64_seed_with_array, 1);
    pyro_define_pub_method(vm, mt64_class, "rand_int", mt64_rand_int, 1);
    pyro_define_pub_method(vm, mt64_class, "rand_int_in_range", mt64_rand_int_in_range, 2);
    pyro_define_pub_method(vm, mt64_class, "rand_float", mt64_rand_float, 0);
}
