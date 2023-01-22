#include "../../../inc/pyro.h"


static PyroValue fn_memory(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_i64((int64_t)vm->bytes_allocated);
}


static PyroValue fn_gc(PyroVM* vm, size_t arg_count, PyroValue* args) {
    pyro_collect_garbage(vm);
    return pyro_null();
}


static PyroValue fn_sizeof(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_OBJ(args[0])) {
        switch (PYRO_AS_OBJ(args[0])->type) {
            case PYRO_OBJECT_VEC:
            case PYRO_OBJECT_VEC_AS_STACK: {
                PyroVec* vec = PYRO_AS_VEC(args[0]);
                return pyro_i64(sizeof(PyroVec) + sizeof(PyroValue) * vec->capacity);
            }
            case PYRO_OBJECT_TUP: {
                PyroTup* tup = PYRO_AS_TUP(args[0]);
                return pyro_i64(sizeof(PyroTup) + sizeof(PyroValue) * tup->count);
            }
            case PYRO_OBJECT_MAP:
            case PYRO_OBJECT_MAP_AS_SET: {
                PyroMap* map = PYRO_AS_MAP(args[0]);
                return pyro_i64(
                    sizeof(PyroMap) +
                    sizeof(PyroMapEntry) * map->entry_array_capacity +
                    sizeof(int64_t) * map->index_array_capacity
                );
            }
            case PYRO_OBJECT_QUEUE: {
                PyroQueue* queue = PYRO_AS_QUEUE(args[0]);
                return pyro_i64(sizeof(PyroQueue) + sizeof(PyroQueueItem) * queue->count);
            }
            case PYRO_OBJECT_BUF: {
                PyroBuf* buf = PYRO_AS_BUF(args[0]);
                return pyro_i64(sizeof(PyroBuf) + sizeof(uint8_t) * buf->capacity);
            }
            default: {
                return pyro_i64(-1);
            }
        }
    } else {
        return pyro_i64(sizeof(PyroValue));
    }
}


static PyroValue fn_address(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_OBJ(args[0])) {
        return pyro_obj(vm->error);
    }

    PyroObject* object = PYRO_AS_OBJ(args[0]);
    PyroStr* string = pyro_sprintf_to_obj(vm, "0x%" PRIXPTR, (uintptr_t)object);
    if (!string) {
        return pyro_null();
    }

    return pyro_obj(string);
}


void pyro_load_std_mod_pyro(PyroVM* vm, PyroMod* module) {
    PyroTup* version_tuple = PyroTup_new(5, vm);
    if (!version_tuple) {
        return;
    }
    version_tuple->values[0] = pyro_i64(PYRO_VERSION_MAJOR);
    version_tuple->values[1] = pyro_i64(PYRO_VERSION_MINOR);
    version_tuple->values[2] = pyro_i64(PYRO_VERSION_PATCH);
    version_tuple->values[3] = pyro_obj(PyroStr_new(PYRO_VERSION_LABEL, vm));
    version_tuple->values[4] = pyro_obj(PyroStr_new(PYRO_VERSION_BUILD, vm));
    pyro_define_pub_member(vm, module, "version_tuple", pyro_obj(version_tuple));

    char* version_c_string = pyro_get_version_string();
    if (version_c_string) {
        PyroStr* version_pyro_string = PyroStr_new(version_c_string, vm);
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