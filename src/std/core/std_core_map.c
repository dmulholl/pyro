#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../vm/utils.h"
#include "../../vm/heap.h"
#include "../../vm/utf8.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"
#include "../../vm/exec.h"


static Value fn_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        pyro_panic(vm, "$map(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(map);
}


static Value fn_is_map(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_MAP(args[0]));
}


static Value map_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return MAKE_I64(map->live_entry_count);
}


static Value map_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return MAKE_BOOL(map->live_entry_count == 0);
}


static Value map_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "set(): out of memory");
    }
    return MAKE_NULL();
}


static Value map_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    if (ObjMap_get(map, args[0], &value, vm)) {
        return value;
    }
    return MAKE_OBJ(vm->empty_error);
}


static Value map_remove(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return MAKE_BOOL(ObjMap_remove(map, args[0], vm));
}


static Value map_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    return MAKE_BOOL(ObjMap_get(map, args[0], &value, vm));
}


static Value map_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap* copy = ObjMap_copy(map, vm);
    if (!copy) {
        pyro_panic(vm, "copy(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(copy);
}


static Value map_keys(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "keys(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(iter);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_VALUES, vm);
    if (!iter) {
        pyro_panic(vm, "values(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(iter);
}


static Value map_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_ENTRIES, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(iter);
}


static Value fn_set(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjMap* map = ObjMap_new_as_set(vm);
        if (!map) {
            pyro_panic(vm, "$set(): out of memory");
            return MAKE_NULL();
        }
        return MAKE_OBJ(map);
    }

    if (arg_count > 1) {
        pyro_panic(vm, "$set(): expected 0 or 1 arguments, found %zu", arg_count);
        return MAKE_NULL();
    }

    // Does the object have an :$iter() method?
    Value iter_method = pyro_get_method(vm, args[0], vm->str_iter);
    if (IS_NULL(iter_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], argument is not iterable");
        return MAKE_NULL();
    }

    // Call the object's :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the $iter() method call
    Value iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return MAKE_NULL();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    Value next_method = pyro_get_method(vm, iterator, vm->str_next);
    if (IS_NULL(next_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], :$iter() returns an object with no :$next() method");
        return MAKE_NULL();
    }
    pyro_push(vm, next_method); // protect from GC

    ObjMap* map = ObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, "$set(): out of memory");
        return MAKE_NULL();
    }
    pyro_push(vm, MAKE_OBJ(map)); // protect from GC

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        Value next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return MAKE_NULL();
        }
        if (IS_ERR(next_value)) {
            break;
        }
        pyro_push(vm, next_value); // protect from GC
        if (ObjMap_set(map, next_value, MAKE_NULL(), vm) == 0) {
            pyro_panic(vm, "$set(): out of memory");
            return MAKE_NULL();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator
    return MAKE_OBJ(map);
}


static Value fn_is_set(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_SET(args[0]));
}


static Value set_add(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], MAKE_NULL(), vm) == 0) {
        pyro_panic(vm, "add(): out of memory");
    }
    return MAKE_NULL();
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
    pyro_define_method(vm, vm->map_class, "$contains", map_contains, 1);
    pyro_define_method(vm, vm->map_class, "copy", map_copy, 0);
    pyro_define_method(vm, vm->map_class, "keys", map_keys, 0);
    pyro_define_method(vm, vm->map_class, "values", map_values, 0);
    pyro_define_method(vm, vm->map_class, "$iter", map_iter, 0);
    pyro_define_method(vm, vm->map_class, "iter", map_iter, 0);
    pyro_define_method(vm, vm->map_class, "is_empty", map_is_empty, 0);

    // Set methods.
    pyro_define_method(vm, vm->set_class, "is_empty", map_is_empty, 0);
    pyro_define_method(vm, vm->set_class, "count", map_count, 0);
    pyro_define_method(vm, vm->set_class, "remove", map_remove, 1);
    pyro_define_method(vm, vm->set_class, "contains", map_contains, 1);
    pyro_define_method(vm, vm->set_class, "$contains", map_contains, 1);
    pyro_define_method(vm, vm->set_class, "add", set_add, 1);
    pyro_define_method(vm, vm->set_class, "$iter", map_keys, 0);
}
