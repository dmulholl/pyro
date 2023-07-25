#include "../includes/pyro.h"


static PyroValue mod_member(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroMod* mod = PYRO_AS_MOD(args[-1]);

    PyroValue member_index;
    if (!PyroMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_obj(vm->error);
    }

    return mod->members->values[member_index.as.i64];
}


static PyroValue mod_has_member(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroMod* mod = PYRO_AS_MOD(args[-1]);

    PyroValue member_index;
    if (PyroMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


static PyroValue mod_members(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroMod* mod = PYRO_AS_MOD(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)mod->pub_member_indexes, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "members(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue mod_all_members(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroMod* mod = PYRO_AS_MOD(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)mod->all_member_indexes, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "all_members(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue mod_globals(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroMod* mod = PYRO_AS_MOD(args[-1]);

    PyroMap* new_map = PyroMap_new(vm);
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
        if (!PyroMap_set(new_map, member_name, member_value, vm)) {
            pyro_panic(vm, "globals(): out of memory");
            return pyro_null();
        }
    }

    return pyro_obj(new_map);
}


void pyro_load_std_builtins_mod(PyroVM* vm) {
    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_module, "has_member", mod_has_member, 1);
    pyro_define_pub_method(vm, vm->class_module, "member", mod_member, 1);
    pyro_define_pub_method(vm, vm->class_module, "members", mod_members, 0);
    pyro_define_pub_method(vm, vm->class_module, "all_members", mod_all_members, 0);
    pyro_define_pub_method(vm, vm->class_module, "globals", mod_globals, 0);
}
