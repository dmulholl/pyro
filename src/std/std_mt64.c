#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../lib/mt64/mt64.h"
#include "../vm/setup.h"
#include "../vm/panics.h"
#include "../vm/io.h"
#include "../vm/stringify.h"


static Value fn_test(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(pyro_mt64_test());
}


static void mt64_free_callback(PyroVM* vm, void* pointer) {
    pyro_mt64_free(pointer);
    vm->bytes_allocated -= pyro_mt64_size();
}


static Value mt64_init(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);

    MT64* mt64 = pyro_mt64_new();
    if (!mt64) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }

    ObjResourcePointer* resource = ObjResourcePointer_new(mt64, mt64_free_callback, vm);
    if (!resource) {
        pyro_mt64_free(mt64);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return MAKE_NULL();
    }
    vm->bytes_allocated += pyro_mt64_size();

    instance->fields[0] = MAKE_OBJ(resource);
    return MAKE_OBJ(instance);
}


static Value mt64_seed_with_hash(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    pyro_mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
    return MAKE_NULL();
}


static Value mt64_seed_with_array(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (IS_STR(args[0])) {
        ObjStr* string = AS_STR(args[0]);
        if (string->length < 8) {
            pyro_mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            pyro_mt64_seed_with_byte_array(mt64, (uint8_t*)string->bytes, string->length);
        }
    } else if (IS_BUF(args[0])) {
        ObjBuf* buf = AS_BUF(args[0]);
        if (buf->count < 8) {
            pyro_mt64_seed_with_u64(mt64, pyro_hash_value(vm, args[0]));
        } else {
            pyro_mt64_seed_with_byte_array(mt64, buf->bytes, buf->count);
        }
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::mt64::seed_with_array().");
    }

    return MAKE_NULL();
}

static Value mt64_rand_float(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;
    return MAKE_F64(pyro_mt64_gen_f64b(mt64));
}


static Value mt64_rand_int(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::mt64::rand_int().");
        return MAKE_NULL();
    }

    return MAKE_I64(pyro_mt64_gen_int(mt64, (uint64_t)args[0].as.i64));
}


static Value mt64_rand_int_in_range(PyroVM* vm, size_t arg_count, Value* args) {
    ObjInstance* instance = AS_INSTANCE(args[-1]);
    ObjResourcePointer* resource = AS_RESOURCE_POINTER(instance->fields[0]);
    MT64* mt64 = (MT64*)resource->pointer;

    if (!IS_I64(args[0]) || !IS_I64(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::mt64::rand_int_in_range().");
        return MAKE_NULL();
    }

    int64_t lower = args[0].as.i64;
    int64_t upper = args[1].as.i64;
    uint64_t delta = (uint64_t)(upper - lower);

    // Select a random integer from the range [lower, upper).
    int64_t rand_int = lower + (int64_t)pyro_mt64_gen_int(mt64, delta);
    return MAKE_I64(rand_int);
}


void pyro_load_std_mt64(PyroVM* vm) {
    ObjModule* mt64_module = pyro_define_module_2(vm, "$std", "mt64");
    if (!mt64_module) {
        return;
    }

    pyro_define_member_fn(vm, mt64_module, "test", fn_test, 0);

    ObjClass* mt64_class = ObjClass_new(vm);
    if (!mt64_class) {
        return;
    }
    mt64_class->name = STR("MT64");
    pyro_define_member(vm, mt64_module, "MT64", MAKE_OBJ(mt64_class));

    pyro_define_field(vm, mt64_class, "generator", MAKE_NULL());

    pyro_define_method(vm, mt64_class, "$init", mt64_init, 0);
    pyro_define_method(vm, mt64_class, "seed_with_hash", mt64_seed_with_hash, 1);
    pyro_define_method(vm, mt64_class, "seed_with_array", mt64_seed_with_array, 1);
    pyro_define_method(vm, mt64_class, "rand_int", mt64_rand_int, 1);
    pyro_define_method(vm, mt64_class, "rand_int_in_range", mt64_rand_int_in_range, 2);
    pyro_define_method(vm, mt64_class, "rand_float", mt64_rand_float, 0);
}
