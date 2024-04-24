#include "../includes/pyro.h"
#include "../../lib/whereami/whereami.h"


static PyroValue fn_memory(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_i64((int64_t)vm->bytes_allocated);
}


static PyroValue fn_gc(PyroVM* vm, size_t arg_count, PyroValue* args) {
    pyro_collect_garbage(vm);
    return pyro_null();
}


static PyroValue fn_sizeof(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_OBJ(args[0])) {
        return pyro_i64(sizeof(PyroValue));
    }

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
            pyro_panic(vm, "sizeof(): not implemented for type: %s", pyro_get_type_name(vm, args[0])->bytes);
            return pyro_i64(-1);
        }
    }
}


static PyroValue fn_address(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_OBJ(args[0])) {
        return pyro_obj(vm->empty_error);
    }

    PyroObject* object = PYRO_AS_OBJ(args[0]);
    PyroStr* string = pyro_sprintf_to_obj(vm, "0x%" PRIXPTR, (uintptr_t)object);
    if (!string) {
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue fn_path(PyroVM* vm, size_t arg_count, PyroValue* args) {
    int length = wai_getExecutablePath(NULL, 0, NULL);
    if (length < 0) {
        pyro_panic(vm, "path(): failed to resolve executable path");
        return pyro_null();
    }

    char* buffer = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!buffer) {
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    int result = wai_getExecutablePath(buffer, length, NULL);
    if (result != length) {
        PYRO_FREE_ARRAY(vm, char, buffer, length + 1);
        pyro_panic(vm, "path(): failed to resolve executable path");
        return pyro_null();
    }

    buffer[length] = '\0';

    PyroStr* path = PyroStr_take(buffer, length, length + 1, vm);
    if (!path) {
        PYRO_FREE_ARRAY(vm, char, buffer, length + 1);
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    return pyro_obj(path);
}


void pyro_load_std_mod_pyro(PyroVM* vm, PyroMod* module) {
    PyroTup* version_tuple = PyroTup_new(5, vm);
    if (!version_tuple) {
        return;
    }
    version_tuple->values[0] = pyro_i64(PYRO_VERSION_MAJOR);
    version_tuple->values[1] = pyro_i64(PYRO_VERSION_MINOR);
    version_tuple->values[2] = pyro_i64(PYRO_VERSION_PATCH);
    version_tuple->values[3] = pyro_obj(PyroStr_COPY(PYRO_VERSION_LABEL));
    version_tuple->values[4] = pyro_obj(PyroStr_COPY(PYRO_VERSION_BUILD));
    pyro_define_pub_member(vm, module, "version_tuple", pyro_obj(version_tuple));

    char* c_string = pyro_get_version_string();
    if (c_string) {
        PyroStr* pyro_string = PyroStr_COPY(c_string);
        if (pyro_string) {
            pyro_define_pub_member(vm, module, "version_string", pyro_obj(pyro_string));
        }
        free(c_string);
    }

    pyro_define_pub_member(vm, module, "module_cache", pyro_obj(vm->module_cache));

    pyro_define_pub_member_fn(vm, module, "memory", fn_memory, 0);
    pyro_define_pub_member_fn(vm, module, "gc", fn_gc, 0);
    pyro_define_pub_member_fn(vm, module, "sizeof", fn_sizeof, 1);
    pyro_define_pub_member_fn(vm, module, "address", fn_address, 1);
    pyro_define_pub_member_fn(vm, module, "path", fn_path, 0);
}
