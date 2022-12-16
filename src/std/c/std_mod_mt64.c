#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/io.h"
#include "../../inc/stringify.h"
#include "../../../lib/mt64/mt64.h"


static Value fn_test(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(mt64_test());
}


static void mt64_free_callback(PyroVM* vm, void* pointer) {
    free(pointer);
    vm->bytes_allocated -= sizeof(MT64);
}


static Value mt64_init_method(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    MT64* mt64 = malloc(sizeof(MT64));
    if (!mt64) {
        pyro_panic(vm, "$std::mt64::MT4:$init(): out of memory");
        return pyro_make_null();
    }
    mt64_init(mt64);

    ObjResourcePointer* resource = ObjResourcePointer_new(mt64, mt64_free_callback, vm);
    if (!resource) {
        free(mt64);
        pyro_panic(vm, "$std::mt64::MT4:$init(): out of memory");
        return pyro_make_null();
    }

    vm->bytes_allocated += sizeof(MT64);
    instance->fields[0] = pyro_make_obj(resource);
    return pyro_make_obj(instance);
}


static Value mt64_seed_with_hash(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
    return pyro_make_null();
}


static Value mt64_seed_with_array(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (IS_STR(args[0])) {
        ObjStr* string = AS_STR(args[0]);
        if (string->length < 8) {
            mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            mt64_seed_with_byte_array(mt64, (uint8_t*)string->bytes, string->length);
        }
    } else if (IS_BUF(args[0])) {
        ObjBuf* buf = AS_BUF(args[0]);
        if (buf->count < 8) {
            mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            mt64_seed_with_byte_array(mt64, buf->bytes, buf->count);
        }
    } else {
        pyro_panic(vm, "seed_with_array(): invalid argument, expected a string or buffer");
    }

    return pyro_make_null();
}

static Value mt64_rand_float(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    return pyro_make_f64(mt64_gen_f64_co(mt64));
}


static Value mt64_rand_int(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "rand_int(): invalid argument, expected a positive integer");
        return pyro_make_null();
    }

    return pyro_make_i64(mt64_gen_int(mt64, (uint64_t)args[0].as.i64));
}


static Value mt64_rand_int_in_range(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!IS_I64(args[0]) || !IS_I64(args[1])) {
        pyro_panic(vm, "rand_int_in_range(): invalid argument, expected an integer");
        return pyro_make_null();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)mt64_gen_int(mt64, delta);
    return pyro_make_i64(rand_int);
}


void pyro_load_std_mod_mt64(PyroVM* vm, ObjModule* module) {
    pyro_define_pub_member_fn(vm, module, "test", fn_test, 0);

    ObjClass* mt64_class = ObjClass_new(vm);
    if (!mt64_class) {
        return;
    }
    mt64_class->name = ObjStr_new("MT64", vm);
    pyro_define_pub_member(vm, module, "MT64", pyro_make_obj(mt64_class));

    pyro_define_pub_field(vm, mt64_class, "generator", pyro_make_null());

    pyro_define_pri_method(vm, mt64_class, "$init", mt64_init_method, 0);
    pyro_define_pub_method(vm, mt64_class, "seed_with_hash", mt64_seed_with_hash, 1);
    pyro_define_pub_method(vm, mt64_class, "seed_with_array", mt64_seed_with_array, 1);
    pyro_define_pub_method(vm, mt64_class, "rand_int", mt64_rand_int, 1);
    pyro_define_pub_method(vm, mt64_class, "rand_int_in_range", mt64_rand_int_in_range, 2);
    pyro_define_pub_method(vm, mt64_class, "rand_float", mt64_rand_float, 0);
}