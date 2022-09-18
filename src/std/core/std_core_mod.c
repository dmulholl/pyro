#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value mod_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);

    Value member_index;
    if (!ObjMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_make_obj(vm->error);
    }

    return mod->members->values[member_index.as.i64];
}


static Value mod_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);

    Value member_index;
    if (ObjMap_get(mod->all_member_indexes, args[0], &member_index, vm)) {
        return pyro_make_bool(true);
    }

    return pyro_make_bool(false);
}


static Value mod_globals(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);

    ObjMap* new_map = ObjMap_new(vm);
    if (!new_map) {
        pyro_panic(vm, "globals(): out of memory");
        return pyro_make_null();
    }

    for (size_t i = 0; i < mod->pub_member_indexes->entry_array_count; i++) {
        MapEntry* entry = &mod->pub_member_indexes->entry_array[i];
        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }
        Value member_name = entry->key;
        Value member_index = entry->value;
        Value member_value = mod->members->values[member_index.as.i64];
        if (ObjMap_set(new_map, member_name, member_value, vm) == 0) {
            pyro_panic(vm, "globals(): out of memory");
            return pyro_make_null();
        }
    }

    return pyro_make_obj(new_map);
}


static Value mod_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjModule* mod = AS_MOD(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)mod->pub_member_indexes, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_make_null();
    }

    return pyro_make_obj(iter);
}


void pyro_load_std_core_mod(PyroVM* vm) {
    // Module methods.
    pyro_define_pri_method(vm, vm->class_module, "$contains", mod_contains, 1);
    pyro_define_pri_method(vm, vm->class_module, "$iter", mod_iter, 0);
    pyro_define_pub_method(vm, vm->class_module, "get", mod_get, 1);
    pyro_define_pub_method(vm, vm->class_module, "contains", mod_contains, 1);
    pyro_define_pub_method(vm, vm->class_module, "globals", mod_globals, 0);
    pyro_define_pub_method(vm, vm->class_module, "iter", mod_iter, 0);
}
