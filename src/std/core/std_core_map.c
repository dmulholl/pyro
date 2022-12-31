#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static PyroValue fn_map(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        pyro_panic(vm, "$map(): out of memory");
        return pyro_null();
    }
    return pyro_obj(map);
}


static PyroValue fn_is_map(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(IS_MAP(args[0]));
}


static PyroValue map_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_i64(map->live_entry_count);
}


static PyroValue map_is_empty(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_bool(map->live_entry_count == 0);
}


static PyroValue map_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], args[1], vm) == 0) {
        pyro_panic(vm, "set(): out of memory");
    }
    return pyro_null();
}


static PyroValue map_get(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    PyroValue value;
    if (ObjMap_get(map, args[0], &value, vm)) {
        return value;
    }
    return pyro_obj(vm->error);
}


static PyroValue map_remove(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_bool(ObjMap_remove(map, args[0], vm));
}


static PyroValue map_contains(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    return pyro_bool(ObjMap_contains(map, args[0], vm));
}


static PyroValue map_copy(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap* copy = ObjMap_copy(map, vm);
    if (!copy) {
        pyro_panic(vm, "copy(): out of memory");
        return pyro_null();
    }
    return pyro_obj(copy);
}


static PyroValue map_keys(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((PyroObj*)map, PYRO_ITER_MAP_KEYS, vm);
    if (!iter) {
        pyro_panic(vm, "keys(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue map_values(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((PyroObj*)map, PYRO_ITER_MAP_VALUES, vm);
    if (!iter) {
        pyro_panic(vm, "values(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue map_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjIter* iter = ObjIter_new((PyroObj*)map, PYRO_ITER_MAP_ENTRIES, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_null();
    }
    return pyro_obj(iter);
}


static PyroValue map_clear(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    ObjMap_clear(map, vm);
    return pyro_null();
}


static PyroValue fn_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 0) {
        ObjMap* map = ObjMap_new_as_set(vm);
        if (!map) {
            pyro_panic(vm, "$set(): out of memory");
            return pyro_null();
        }
        return pyro_obj(map);
    }

    if (arg_count > 1) {
        pyro_panic(vm, "$set(): expected 0 or 1 arguments, found %zu", arg_count);
        return pyro_null();
    }

    // Does the object have an :$iter() method?
    PyroValue iter_method = pyro_get_method(vm, args[0], vm->str_dollar_iter);
    if (IS_NULL(iter_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], argument is not iterable");
        return pyro_null();
    }

    // Call the object's :$iter() method to get an iterator.
    pyro_push(vm, args[0]); // receiver for the $iter() method call
    PyroValue iterator = pyro_call_method(vm, iter_method, 0);
    if (vm->halt_flag) {
        return pyro_null();
    }
    pyro_push(vm, iterator); // protect from GC

    // Get the iterator's :$next() method.
    PyroValue next_method = pyro_get_method(vm, iterator, vm->str_dollar_next);
    if (IS_NULL(next_method)) {
        pyro_panic(vm, "$set(): invalid argument [arg], :$iter() returns an object with no :$next() method");
        return pyro_null();
    }
    pyro_push(vm, next_method); // protect from GC

    ObjMap* map = ObjMap_new_as_set(vm);
    if (!map) {
        pyro_panic(vm, "$set(): out of memory");
        return pyro_null();
    }
    pyro_push(vm, pyro_obj(map)); // protect from GC

    while (true) {
        pyro_push(vm, iterator); // receiver for the :$next() method call
        PyroValue next_value = pyro_call_method(vm, next_method, 0);
        if (vm->halt_flag) {
            return pyro_null();
        }
        if (IS_ERR(next_value)) {
            break;
        }
        pyro_push(vm, next_value); // protect from GC
        if (ObjMap_set(map, next_value, pyro_null(), vm) == 0) {
            pyro_panic(vm, "$set(): out of memory");
            return pyro_null();
        }
        pyro_pop(vm); // next_value
    }

    pyro_pop(vm); // map
    pyro_pop(vm); // next_method
    pyro_pop(vm); // iterator
    return pyro_obj(map);
}


static PyroValue fn_is_set(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(IS_SET(args[0]));
}


static PyroValue set_add(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map = AS_MAP(args[-1]);
    if (ObjMap_set(map, args[0], pyro_null(), vm) == 0) {
        pyro_panic(vm, "add(): out of memory");
    }
    return pyro_null();
}


static PyroValue set_union(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "union(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "union(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
            pyro_panic(vm, "union(): out of memory");
            return pyro_null();
        }
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        PyroMapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
            pyro_panic(vm, "union(): out of memory");
            return pyro_null();
        }
    }

    return pyro_obj(new_map);
}


static PyroValue set_intersection(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "intersection(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "intersection(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
                pyro_panic(vm, "intersection(): out of memory");
                return pyro_null();
            }
        }
    }

    return pyro_obj(new_map);
}


static PyroValue set_difference(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "difference(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "difference(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
                pyro_panic(vm, "difference(): out of memory");
                return pyro_null();
            }
        }
    }

    return pyro_obj(new_map);
}


static PyroValue set_symmetric_difference(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "symmetric_difference(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    ObjMap* new_map = ObjMap_new_as_set(vm);
    if (!new_map) {
        pyro_panic(vm, "symmetric_difference(): out of memory");
        return pyro_null();
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
                pyro_panic(vm, "symmetric_difference(): out of memory");
                return pyro_null();
            }
        }
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        PyroMapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            if (ObjMap_set(new_map, entry->key, pyro_null(), vm) == 0) {
                pyro_panic(vm, "symmetric_difference(): out of memory");
                return pyro_null();
            }
        }
    }

    return pyro_obj(new_map);
}


static PyroValue set_is_subset_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_subset_of(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count <= map2->live_entry_count)) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue set_is_proper_subset_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_proper_subset_of(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count < map2->live_entry_count)) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue set_is_superset_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_superset_of(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count >= map2->live_entry_count)) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        PyroMapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue set_is_proper_superset_of(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_proper_superset_of(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (!(map1->live_entry_count > map2->live_entry_count)) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < map2->entry_array_count; i++) {
        PyroMapEntry* entry = &map2->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map1, entry->key, vm)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


static PyroValue set_is_equal_to(PyroVM* vm, size_t arg_count, PyroValue* args) {
    ObjMap* map1 = AS_MAP(args[-1]);

    if (!IS_SET(args[0])) {
        pyro_panic(vm, "is_equal_to(): invalid argument, expected a set");
        return pyro_null();
    }
    ObjMap* map2 = AS_MAP(args[0]);

    if (map1->live_entry_count != map2->live_entry_count) {
        return pyro_bool(false);
    }

    for (size_t i = 0; i < map1->entry_array_count; i++) {
        PyroMapEntry* entry = &map1->entry_array[i];

        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!ObjMap_contains(map2, entry->key, vm)) {
            return pyro_bool(false);
        }
    }

    return pyro_bool(true);
}


void pyro_load_std_core_map(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$map", fn_map, 0);
    pyro_define_global_fn(vm, "$is_map", fn_is_map, 1);
    pyro_define_global_fn(vm, "$set", fn_set, -1);
    pyro_define_global_fn(vm, "$is_set", fn_is_set, 1);

    // Map methods -- private.
    pyro_define_pri_method(vm, vm->class_map, "$iter", map_iter, 0);
    pyro_define_pri_method(vm, vm->class_map, "$contains", map_contains, 1);

    // Map methods -- public.
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

    // Set methods -- private.
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
    pyro_define_pri_method(vm, vm->class_set, "$op_binary_equals_equals", set_is_equal_to, 1);

    // Set methods -- public.
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
    pyro_define_pub_method(vm, vm->class_set, "is_equal_to", set_is_equal_to, 1);
}
