#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value fn_map(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        pyro_panic(vm, "$map(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(map);
}


static Value fn_is_map(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_MAP(args[0]));
}


static Value map_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_make_i64(map->live_entry_count);
}


static Value map_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_make_bool(map->live_entry_count == 0);
}


static Value map_set(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "set(): out of memory");
    }
    return pyro_make_null();
}


static Value map_get(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    Value value;
    if (ObjMap_get(map, args[0], &value, vm)) {
        return value;
    }
    return MAKE_OBJ(vm->error);
}


static Value map_remove(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_make_bool(ObjMap_remove(map, args[0], vm));
}


static Value map_contains(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_make_bool(ObjMap_contains(map, args[0], vm));
}


static Value map_copy(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap* copy = ObjMap_copy(map, vm);
    if (!copy) {
        pyro_panic(vm, "copy(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(copy);
}


static Value map_keys(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "keys(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(iter);
}


static Value map_values(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_VALUES, vm);
    if (!iter) {
        pyro_panic(vm, "values(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(iter);
}


static Value map_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)map, ITER_MAP_ENTRIES, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(iter);
}


static Value map_clear(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap_clear(map, vm);
    return pyro_make_null();
}


static Value fn_set(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 0) {
        ObjMap* map = ObjMap_new_as_set(vm);
        if (!map) {
            pyro_panic(vm, "$set(): out of memory");
            return pyro_make_null();
        }
        return MAKE_OBJ(map);
    }

    if (arg_count > 1) {
        pyro_panic(vm, "$set(): expected 0 or 1 arguments, found %zu", arg_count);
        return pyro_make_null();
    }

    // Does the object have an :$iter() method?
    Value iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (IS_NULL(iter_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], argument is not iterable");
        return pyro_make_null();
    }

    // Call the object's :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the $iter() method call
    Value iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return pyro_make_null();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    Value next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
    if (IS_NULL(next_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], :$iter() returns an object with no :$next() method");
        return pyro_make_null();
    }
    pyro_push(vm, next_method); // protect from GC

    ObjMap* map = ObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, "$set(): out of memory");
        return pyro_make_null();
    }
    pyro_push(vm, MAKE_OBJ(map)); // protect from GC

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        Value next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return pyro_make_null();
        }
        if (IS_ERR(next_value)) {
            break;
        }
        pyro_push(vm, next_value); // protect from GC
        if (ObjMap_set(map, next_value, pyro_make_null(), vm) == 0) {
            pyro_panic(vm, "$set(): out of memory");
            return pyro_make_null();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator
    return MAKE_OBJ(map);
}


static Value fn_is_set(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_SET(args[0]));
}


static Value set_add(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], pyro_make_null(), vm) == 0) {
        pyro_panic(vm, "add(): out of memory");
    }
    return pyro_make_null();
}


static Value set_union(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "union(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "union(): out of memory");
        return pyro_make_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
            pyro_panic(vm, "union(): out of memory");
            return pyro_make_null();
        }
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        MapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
            pyro_panic(vm, "union(): out of memory");
            return pyro_make_null();
        }
    }

    return MAKE_OBJ(new_map);
}


static Value set_intersection(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "intersection(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "intersection(): out of memory");
        return pyro_make_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
                pyro_panic(vm, "intersection(): out of memory");
                return pyro_make_null();
            }
        }
    }

    return MAKE_OBJ(new_map);
}


static Value set_difference(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "difference(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "difference(): out of memory");
        return pyro_make_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
                pyro_panic(vm, "difference(): out of memory");
                return pyro_make_null();
            }
        }
    }

    return MAKE_OBJ(new_map);
}


static Value set_symmetric_difference(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "symmetric_difference(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "symmetric_difference(): out of memory");
        return pyro_make_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
                pyro_panic(vm, "symmetric_difference(): out of memory");
                return pyro_make_null();
            }
        }
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        MapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_make_null(), vm) == 0) {
                pyro_panic(vm, "symmetric_difference(): out of memory");
                return pyro_make_null();
            }
        }
    }

    return MAKE_OBJ(new_map);
}


static Value set_is_subset_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_subset_of(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count <= map2->live_entry_count)) {
        return pyro_make_bool(false);
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            return pyro_make_bool(false);
        }
    }

    return pyro_make_bool(true);
}


static Value set_is_proper_subset_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_proper_subset_of(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count < map2->live_entry_count)) {
        return pyro_make_bool(false);
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        MapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            return pyro_make_bool(false);
        }
    }

    return pyro_make_bool(true);
}


static Value set_is_superset_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_superset_of(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count >= map2->live_entry_count)) {
        return pyro_make_bool(false);
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        MapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            return pyro_make_bool(false);
        }
    }

    return pyro_make_bool(true);
}


static Value set_is_proper_superset_of(PyroVM* vm, size_t arg_count, Value* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_proper_superset_of(): invalid argument, expected a set");
        return pyro_make_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count > map2->live_entry_count)) {
        return pyro_make_bool(false);
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        MapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            return pyro_make_bool(false);
        }
    }

    return pyro_make_bool(true);
}


void pyro_load_std_core_map(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);
    pyro_define_global_fn(vm, "$set", fn_set, -1);
    pyro_define_global_fn(vm, "$is_set", fn_is_set, 1);

    // Map methods.
    pyro_define_pri_method(vm, vm->class_map, "$iter", map_iter, 0);
    pyro_define_pri_method(vm, vm->class_map, "$contains", map_contains, 1);
    pyro_define_pub_method(vm, vm->class_map, "count", map_count, 0);
    pyro_define_pub_method(vm, vm->class_map, "get", map_get, 1);
    pyro_define_pub_method(vm, vm->class_map, "set", map_set, 2);
    pyro_define_pub_method(vm, vm->class_map, "remove", map_remove, 1);
    pyro_define_pub_method(vm, vm->class_map, "contains", map_contains, 1);
    pyro_define_pub_method(vm, vm->class_map, "copy", map_copy, 0);
    pyro_define_pub_method(vm, vm->class_map, "keys", map_keys, 0);
    pyro_define_pub_method(vm, vm->class_map, "values", map_values, 0);
    pyro_define_pub_method(vm, vm->class_map, "iter", map_iter, 0);
    pyro_define_pub_method(vm, vm->class_map, "is_empty", map_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_map, "clear", map_clear, 0);

    // Set methods.
    pyro_define_pri_method(vm, vm->class_set, "$contains", map_contains, 1);
    pyro_define_pri_method(vm, vm->class_set, "$iter", map_keys, 0);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_bar", set_union, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_amp", set_intersection, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_caret", set_symmetric_difference, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_less_equals", set_is_subset_of, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_less", set_is_proper_subset_of, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_minus", set_difference, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_greater_equals", set_is_superset_of, 1);
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_greater", set_is_proper_superset_of, 1);
    pyro_define_pub_method(vm, vm->class_set, "is_empty", map_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_set, "count", map_count, 0);
    pyro_define_pub_method(vm, vm->class_set, "remove", map_remove, 1);
    pyro_define_pub_method(vm, vm->class_set, "contains", map_contains, 1);
    pyro_define_pub_method(vm, vm->class_set, "add", set_add, 1);
    pyro_define_pub_method(vm, vm->class_set, "union", set_union, 1);
    pyro_define_pub_method(vm, vm->class_set, "intersection", set_intersection, 1);
    pyro_define_pub_method(vm, vm->class_set, "difference", set_difference, 1);
    pyro_define_pub_method(vm, vm->class_set, "symmetric_difference", set_symmetric_difference, 1);
    pyro_define_pub_method(vm, vm->class_set, "is_subset_of", set_is_subset_of, 1);
    pyro_define_pub_method(vm, vm->class_set, "is_proper_subset_of", set_is_proper_subset_of, 1);
    pyro_define_pub_method(vm, vm->class_set, "is_superset_of", set_is_superset_of, 1);
    pyro_define_pub_method(vm, vm->class_set, "is_proper_superset_of", set_is_proper_superset_of, 1);
    pyro_define_pub_method(vm, vm->class_set, "clear", map_clear, 0);
}
