#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/errors.h"


static Value fn_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(map);
}


static Value fn_is_map(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_MAP(args[0]));
}


static Value map_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return I64_VAL(map->count - map->tombstone_count);
}


static Value map_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (!ObjMap_set(map, args[0], args[1], vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    return NULL_VAL();
}


static Value map_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    if (ObjMap_get(map, args[0], &value)) {
        return value;
    }
    return OBJ_VAL(vm->empty_error);
}


static Value map_del(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return BOOL_VAL(ObjMap_remove(map, args[0]));
}


static Value map_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    return BOOL_VAL(ObjMap_get(map, args[0], &value));
}


static Value map_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap* copy = ObjMap_copy(map, vm);
    if (!copy) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(copy);
}


static Value map_keys(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_KEYS, vm);
    if (!iterator) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iterator);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_VALUES, vm);
    if (!iterator) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iterator);
}


static Value map_entries(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMapIter* iterator = ObjMapIter_new(map, MAP_ITER_ENTRIES, vm);
    if (!iterator) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iterator);
}


static Value map_iter_iter(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1];
}


static Value map_iter_next(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMapIter* iterator = AS_MAP_ITER(args[-1]);
    return ObjMapIter_next(iterator, vm);
}


void pyro_load_std_core_map(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);

    // Methods.
    pyro_define_method(vm, vm->map_class, "count", map_count, 0);
    pyro_define_method(vm, vm->map_class, "get", map_get, 1);
    pyro_define_method(vm, vm->map_class, "$get_index", map_get, 1);
    pyro_define_method(vm, vm->map_class, "set", map_set, 2);
    pyro_define_method(vm, vm->map_class, "$set_index", map_set, 2);
    pyro_define_method(vm, vm->map_class, "del", map_del, 1);
    pyro_define_method(vm, vm->map_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->map_class, "copy", map_copy, 0);
    pyro_define_method(vm, vm->map_class, "keys", map_keys, 0);
    pyro_define_method(vm, vm->map_class, "values", map_values, 0);
    pyro_define_method(vm, vm->map_class, "entries", map_entries, 0);
    pyro_define_method(vm, vm->map_iter_class, "$iter", map_iter_iter, 0);
    pyro_define_method(vm, vm->map_iter_class, "$next", map_iter_next, 0);
}
