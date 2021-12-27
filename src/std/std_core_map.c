#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/setup.h"
#include "../vm/panics.h"
#include "../vm/exec.h"


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


static Value map_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return BOOL_VAL(map->count - map->tombstone_count == 0);
}


static Value map_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], args[1], vm) == 0) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    return NULL_VAL();
}


static Value map_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    if (ObjMap_get(map, args[0], &value, vm)) {
        return value;
    }
    return OBJ_VAL(vm->empty_error);
}


static Value map_remove(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return BOOL_VAL(ObjMap_remove(map, args[0], vm));
}


static Value map_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    return BOOL_VAL(ObjMap_get(map, args[0], &value, vm));
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
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iter);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_VALUES, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iter);
}


static Value map_entries(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_ENTRIES, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(iter);
}


static Value fn_set(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjMap* map = ObjMap_new_as_set(vm);
        if (!map) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        return OBJ_VAL(map);
    }

    if (arg_count > 1) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 0 or 1 arguments for $set(), found %d.", arg_count);
        return NULL_VAL();
    }

    // Does the object have an :$iter() method?
    Value iter_method = pyro_get_method(vm, args[0], vm->str_iter);
    if (IS_NULL(iter_method)) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Argument to $set() is not iterable.");
        return NULL_VAL();
    }

    // Call the object's :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the $iter() method call
    Value iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    Value next_method = pyro_get_method(vm, iterator, vm->str_next);
    if (IS_NULL(next_method)) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid iterator -- no :$next() method.");
        return NULL_VAL();
    }
    pyro_push(vm, next_method); // protect from GC

    ObjMap* map = ObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    pyro_push(vm, OBJ_VAL(map)); // protect from GC

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        Value next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return NULL_VAL();
        }
        if (IS_ERR(next_value)) {
            break;
        }
        pyro_push(vm, next_value); // protect from GC
        if (ObjMap_set(map, next_value, NULL_VAL(), vm) == 0) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator
    return OBJ_VAL(map);
}


static Value fn_is_set(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_SET(args[0]));
}


static Value set_add(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], NULL_VAL(), vm) == 0) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    return NULL_VAL();
}


void pyro_load_std_core_map(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);
    pyro_define_global_fn(vm, "$set", fn_set, -1);
    pyro_define_global_fn(vm, "$is_set", fn_is_set, 1);

    // Map methods.
    pyro_define_method(vm, vm->map_class, "count", map_count, 0);
    pyro_define_method(vm, vm->map_class, "get", map_get, 1);
    pyro_define_method(vm, vm->map_class, "set", map_set, 2);
    pyro_define_method(vm, vm->map_class, "remove", map_remove, 1);
    pyro_define_method(vm, vm->map_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->map_class, "copy", map_copy, 0);
    pyro_define_method(vm, vm->map_class, "keys", map_keys, 0);
    pyro_define_method(vm, vm->map_class, "values", map_values, 0);
    pyro_define_method(vm, vm->map_class, "entries", map_entries, 0);
    pyro_define_method(vm, vm->map_class, "is_empty", map_is_empty, 0);

    // Set methods.
    pyro_define_method(vm, vm->set_class, "is_empty", map_is_empty, 0);
    pyro_define_method(vm, vm->set_class, "count", map_count, 0);
    pyro_define_method(vm, vm->set_class, "remove", map_remove, 1);
    pyro_define_method(vm, vm->set_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->set_class, "add", set_add, 1);
    pyro_define_method(vm, vm->set_class, "$iter", map_keys, 0);
}
