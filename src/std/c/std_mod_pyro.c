#include "../../inc/std_lib.h"
#include "../../inc/vm.h"
#include "../../inc/heap.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/gc.h"
#include "../../inc/stringify.h"


static Value fn_memory(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_i64((int64_t)vm->bytes_allocated);
}


static Value fn_gc(PyroVM* vm, size_t arg_count, Value* args) {
    pyro_collect_garbage(vm);
    return pyro_null();
}


static Value fn_sizeof(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_OBJ(args[0])) {
        switch (AS_OBJ(args[0])->type) {
            case PYRO_OBJECT_VEC:
            case PYRO_OBJECT_VEC_AS_STACK: {
                ObjVec* vec = AS_VEC(args[0]);
                return pyro_i64(sizeof(ObjVec) + sizeof(Value) * vec->capacity);
            }
            case PYRO_OBJECT_TUP: {
                ObjTup* tup = AS_TUP(args[0]);
                return pyro_i64(sizeof(ObjTup) + sizeof(Value) * tup->count);
            }
            case PYRO_OBJECT_MAP:
            case PYRO_OBJECT_MAP_AS_SET: {
                ObjMap* map = AS_MAP(args[0]);
                return pyro_i64(
                    sizeof(ObjMap) +
                    sizeof(PyroMapEntry) * map->entry_array_capacity +
                    sizeof(int64_t) * map->index_array_capacity
                );
            }
            case PYRO_OBJECT_QUEUE: {
                ObjQueue* queue = AS_QUEUE(args[0]);
                return pyro_i64(sizeof(ObjQueue) + sizeof(QueueItem) * queue->count);
            }
            case PYRO_OBJECT_BUF: {
                ObjBuf* buf = AS_BUF(args[0]);
                return pyro_i64(sizeof(ObjBuf) + sizeof(uint8_t) * buf->capacity);
            }
            default: {
                return pyro_i64(-1);
            }
        }
    } else {
        return pyro_i64(sizeof(Value));
    }
}


static Value fn_address(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_OBJ(args[0])) {
        return pyro_obj(vm->error);
    }

    Obj* object = AS_OBJ(args[0]);
    ObjStr* string = pyro_sprintf_to_obj(vm, "0x%" PRIXPTR, (uintptr_t)object);
    if (!string) {
        return pyro_null();
    }

    return pyro_obj(string);
}


void pyro_load_std_mod_pyro(PyroVM* vm, ObjModule* module) {
    ObjTup* version_tuple = ObjTup_new(5, vm);
    if (!version_tuple) {
        return;
    }
    version_tuple->values[0] = pyro_i64(PYRO_VERSION_MAJOR);
    version_tuple->values[1] = pyro_i64(PYRO_VERSION_MINOR);
    version_tuple->values[2] = pyro_i64(PYRO_VERSION_PATCH);
    version_tuple->values[3] = pyro_obj(ObjStr_new(PYRO_VERSION_LABEL, vm));
    version_tuple->values[4] = pyro_obj(ObjStr_new(PYRO_VERSION_BUILD, vm));
    pyro_define_pub_member(vm, module, "version_tuple", pyro_obj(version_tuple));

    char* version_c_string = pyro_get_version_string();
    if (version_c_string) {
        ObjStr* version_pyro_string = ObjStr_new(version_c_string, vm);
        if (!version_pyro_string) {
            return;
        }
        pyro_define_pub_member(vm, module, "version_string", pyro_obj(version_pyro_string));
    }

    pyro_define_pub_member_fn(vm, module, "memory", fn_memory, 0);
    pyro_define_pub_member_fn(vm, module, "gc", fn_memory, 0);
    pyro_define_pub_member_fn(vm, module, "sizeof", fn_sizeof, 1);
    pyro_define_pub_member_fn(vm, module, "address", fn_address, 1);
}
