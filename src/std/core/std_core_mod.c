#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static PyroValue mod_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjModule* mod = AS_MOD(args[-1]);

    PyroValue member_index;
    if (!PyroObjMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_obj(vm->error);
    }

    return mod->members->values[member_index.as.i64];
}


static PyroValue mod_contains(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjModule* mod = AS_MOD(args[-1]);

    PyroValue member_index;
    if (PyroObjMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


static PyroValue mod_globals(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjModule* mod = AS_MOD(args[-1]);

    PyroObjMap* new_map = PyroObjMap_new(vm);
    if (!new_map) {
        pyro_panic(vm, "globals(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < mod->pub_member_indexes->entry_array_count; i++) {
        PyroMapEntry* entry = &mod->pub_member_indexes->entry_array[i];
        if (PYRO_IS_TOMBSTONE(entry->key)) {
            continue;
        }
        PyroValue member_name = entry->key;
        PyroValue member_index = entry->value;
        PyroValue member_value = mod->members->values[member_index.as.i64];
        if (PyroObjMap_set(new_map, member_name, member_value, vm) == 0) {
            pyro_panic(vm, "globals(): out of memory");
            return pyro_null();
        }
    }

    return pyro_obj(new_map);
}


static PyroValue mod_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroObjModule* mod = AS_MOD(args[-1]);

    PyroObjIter* iter = PyroObjIter_new((PyroObj*)mod->pub_member_indexes, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


void pyro_load_std_core_mod(PyroVM* vm) {
    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_module, "$contains", mod_contains, 1);
    pyro_define_pri_method(vm, vm->class_module, "$iter", mod_iter, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_module, "get", mod_get, 1);
    pyro_define_pub_method(vm, vm->class_module, "contains", mod_contains, 1);
    pyro_define_pub_method(vm, vm->class_module, "globals", mod_globals, 0);
    pyro_define_pub_method(vm, vm->class_module, "iter", mod_iter, 0);
}
