#include "../inc/pyro.h"


// Allocates memory for a fixed-size object.
#define ALLOCATE_OBJECT(vm, type, type_enum) \
    (type*)allocate_object(vm, sizeof(type), type_enum)


// Allocates memory for a flexibly-sized object, e.g. a tuple.
#define ALLOCATE_FLEX_OBJECT(vm, type, type_enum, value_count, value_type) \
    (type*)allocate_object(vm, sizeof(type) + value_count * sizeof(value_type), type_enum)


// Allocates memory for a new object. Automatically adds the new object to the VM's linked list of
// all heap-allocated objects. Returns NULL if memory cannot be allocated.
static PyroObject* allocate_object(PyroVM* vm, size_t size, PyroObjectType type) {
    PyroObject* object = pyro_realloc(vm, NULL, 0, size);
    if (object == NULL) {
        return NULL;
    }

    object->type = type;
    object->is_marked = false;
    object->class = NULL;

    object->next = vm->objects;
    vm->objects = object;

    return object;
}


/* ------ */
/* Tuples */
/* ------ */


PyroTup* PyroTup_new(size_t count, PyroVM* vm) {
    PyroTup* tup = ALLOCATE_FLEX_OBJECT(vm, PyroTup, PYRO_OBJECT_TUP, count, PyroValue);
    if (!tup) {
        return NULL;
    }

    tup->obj.class = vm->class_tup;
    tup->count = count;
    for (size_t i = 0; i < count; i++) {
        tup->values[i] = pyro_null();
    }
    return tup;
}


bool PyroTup_check_equal(PyroTup* a, PyroTup* b, PyroVM* vm) {
    if (a->count == b->count) {
        for (size_t i = 0; i < a->count; i++) {
            if (!pyro_op_compare_eq(vm, a->values[i], b->values[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}


/* -------- */
/* Closures */
/* -------- */


PyroClosure* PyroClosure_new(PyroVM* vm, PyroFn* fn, PyroMod* module) {
    PyroClosure* closure = ALLOCATE_OBJECT(vm, PyroClosure, PYRO_OBJECT_CLOSURE);
    if (!closure) {
        return NULL;
    }

    closure->fn = fn;
    closure->module = module;
    closure->default_values = NULL;
    closure->upvalues = NULL;
    closure->upvalue_count = 0;

    closure->default_values = PyroVec_new(vm);
    if (!closure->default_values) {
        return NULL;
    }

    if (fn->upvalue_count > 0) {
        PyroUpvalue** array = PYRO_ALLOCATE_ARRAY(vm, PyroUpvalue*, fn->upvalue_count);
        if (!array) {
            return NULL;
        }
        for (size_t i = 0; i < fn->upvalue_count; i++) {
            array[i] = NULL;
        }
        closure->upvalues = array;
        closure->upvalue_count = fn->upvalue_count;
    }

    return closure;
}


/* -------- */
/* Upvalues */
/* -------- */


PyroUpvalue* PyroUpvalue_new(PyroVM* vm, PyroValue* slot) {
    PyroUpvalue* upvalue = ALLOCATE_OBJECT(vm, PyroUpvalue, PYRO_OBJECT_UPVALUE);
    if (!upvalue) {
        return NULL;
    }
    upvalue->location = slot;
    upvalue->closed = pyro_null();
    upvalue->next = NULL;
    return upvalue;
}


/* ------- */
/* Classes */
/* ------- */


PyroClass* PyroClass_new(PyroVM* vm) {
    PyroClass* class = ALLOCATE_OBJECT(vm, PyroClass, PYRO_OBJECT_CLASS);
    if (!class) {
        return NULL;
    }

    class->name = NULL;
    class->superclass = NULL;
    class->all_instance_methods = NULL;
    class->pub_instance_methods = NULL;
    class->all_field_indexes = NULL;
    class->pub_field_indexes = NULL;
    class->default_field_values = NULL;
    class->static_methods = NULL;
    class->static_fields = NULL;
    class->all_instance_methods_cached_name = NULL;
    class->all_instance_methods_cached_value = pyro_null();
    class->pub_instance_methods_cached_name = NULL;
    class->pub_instance_methods_cached_value = pyro_null();
    class->init_method = pyro_null();

    class->all_instance_methods = PyroMap_new(vm);
    class->pub_instance_methods = PyroMap_new(vm);
    class->all_field_indexes = PyroMap_new(vm);
    class->pub_field_indexes = PyroMap_new(vm);
    class->default_field_values = PyroVec_new(vm);
    class->static_methods = PyroMap_new(vm);
    class->static_fields = PyroMap_new(vm);

    if (vm->memory_allocation_failed) {
        return NULL;
    }

    return class;
}


/* --------- */
/* Instances */
/* --------- */


PyroInstance* PyroInstance_new(PyroVM* vm, PyroClass* class) {
    size_t num_fields = class->default_field_values->count;

    PyroInstance* instance = ALLOCATE_FLEX_OBJECT(vm, PyroInstance, PYRO_OBJECT_INSTANCE, num_fields, PyroValue);
    if (!instance) {
        return NULL;
    }

    instance->obj.class = class;
    if (num_fields > 0) {
        memcpy(instance->fields, class->default_field_values->values, sizeof(PyroValue) * num_fields);
    }

    return instance;
}


/* ------ */
/*  Maps  */
/* ------ */


// Sentinel value: indicates that a slot in [index_array] is empty.
#define EMPTY -1

// Sentinel value: indicates that a slot in [index_array] is a tombstone.
#define TOMBSTONE -2


// This function returns a pointer to a slot in [index_array]. This slot can (1) be empty,
// (2) contain a tombstone, or (3) contain the index of an entry in [entry_array].
//
// - If the slot is empty or contains a tombstone, the map does not contain an entry matching
//   [key]. If a new entry with [key] is appended to [entry_array], its index should be
//   inserted into the slot returned by this function.
//
// - If the slot contains a non-negative index value, the map contains an entry matching
//   [key]. This entry can be found in [entry_array] at the specified index.
//
// Note that [index_array] must have a non-zero length before this function is called.
static int64_t* find_entry(
    PyroVM* vm,
    PyroMapEntry* entry_array,
    int64_t* index_array,
    size_t index_array_capacity,
    PyroValue key
) {
    // Capacity is always a power of 2 so we can use bitwise-AND as a fast modulo operator,
    // i.e. this is equivalent to: i = key_hash % index_array_capacity.
    size_t i = (size_t)pyro_hash_value(vm, key) & (index_array_capacity - 1);
    int64_t* first_tombstone = NULL;

    for (;;) {
        int64_t* slot = &index_array[i];

        if (*slot == EMPTY) {
            return first_tombstone == NULL ? slot : first_tombstone;
        } else if (*slot == TOMBSTONE) {
            if (first_tombstone == NULL) first_tombstone = slot;
        } else if (pyro_op_compare_eq(vm, key, entry_array[*slot].key)) {
            return slot;
        }

        i = (i + 1) & (index_array_capacity - 1);
    }
}


// This function is used for looking up strings in the interned-strings pool. If the pool of
// interned strings contains an entry identical to [string], it returns a pointer to that
// string, otherwise it returns NULL.
static PyroStr* find_string_in_pool(
    PyroMap* string_pool,
    const char* string,
    size_t count,
    uint64_t hash
) {
    if (string_pool->live_entry_count == 0) {
        return NULL;
    }

    size_t i = (size_t)hash & (string_pool->index_array_capacity - 1);

    for (;;) {
        int64_t* slot = &string_pool->index_array[i];

        if (*slot == EMPTY) {
            return NULL;
        } else if (*slot == TOMBSTONE) {
            // Skip over the tombstone and keep looking for a matching entry.
        } else {
            PyroStr* interned_string = PYRO_AS_STR(string_pool->entry_array[*slot].key);
            if (interned_string->count == count) {
                if (interned_string->hash == hash) {
                    if (memcmp(interned_string->bytes, string, count) == 0) {
                        return interned_string;
                    }
                }
            }
        }

        i = (i + 1) & (string_pool->index_array_capacity - 1);
    }
}


// This function doubles the capacity of the index array, then rebuilds the index. (Rebuilding
// the index has the side-effect of eliminating any tombstone slots. This function also takes
// the opportunity to eliminate any tombstones from the entry array so when this function
// returns the map contains no tombstone entries.)
static bool resize_index_array(PyroMap* map, PyroVM* vm) {
    size_t new_index_array_capacity = PYRO_GROW_CAPACITY(map->index_array_capacity);

    int64_t* new_index_array = PYRO_ALLOCATE_ARRAY(vm, int64_t, new_index_array_capacity);
    if (!new_index_array) {
        return false;
    }

    for (size_t i = 0; i < new_index_array_capacity; i++) {
        new_index_array[i] = EMPTY;
    }

    PYRO_FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
    map->index_array = new_index_array;
    map->index_array_capacity = new_index_array_capacity;
    map->index_array_count = map->live_entry_count;
    map->max_load_threshold = new_index_array_capacity * PYRO_MAX_HASHMAP_LOAD;

    // If the entry array contains tombstones, this is a good time to compact it by removing
    // them while we're rebuilding the index.
    if (map->entry_array_count > map->live_entry_count) {
        size_t dst_index = 0;

        for (size_t src_index = 0; src_index < map->entry_array_count; src_index++) {
            if (PYRO_IS_TOMBSTONE(map->entry_array[src_index].key)) {
                continue;
            }

            if (src_index > dst_index) {
                map->entry_array[dst_index] = map->entry_array[src_index];
            }

            int64_t* slot = find_entry(
                vm,
                map->entry_array,
                new_index_array,
                new_index_array_capacity,
                map->entry_array[src_index].key
            );

            *slot = (int64_t)dst_index;
            dst_index++;
        }

        assert(dst_index == map->live_entry_count);
        map->entry_array_count = map->live_entry_count;
        return true;
    }

    // No tombstones so we just need to rebuild the index.
    for (size_t i = 0; i < map->entry_array_count; i++) {
        int64_t* slot = find_entry(
            vm,
            map->entry_array,
            new_index_array,
            new_index_array_capacity,
            map->entry_array[i].key
        );
        *slot = (int64_t)i;
    }

    return true;
}


PyroMap* PyroMap_new(PyroVM* vm) {
    PyroMap* map = ALLOCATE_OBJECT(vm, PyroMap, PYRO_OBJECT_MAP);
    if (!map) {
        return NULL;
    }

    map->entry_array = NULL;
    map->entry_array_count = 0;
    map->entry_array_capacity = 0;
    map->live_entry_count = 0;
    map->index_array = NULL;
    map->index_array_count = 0;
    map->index_array_capacity = 0;
    map->max_load_threshold = 0;
    map->obj.class = vm->class_map;
    return map;
}


void PyroMap_clear(PyroMap* map, PyroVM* vm) {
    PYRO_FREE_ARRAY(vm, PyroMapEntry, map->entry_array, map->entry_array_capacity);
    PYRO_FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
    map->entry_array = NULL;
    map->entry_array_count = 0;
    map->entry_array_capacity = 0;
    map->live_entry_count = 0;
    map->index_array = NULL;
    map->index_array_count = 0;
    map->index_array_capacity = 0;
    map->max_load_threshold = 0;
}


PyroMap* PyroMap_new_as_set(PyroVM* vm) {
    PyroMap* map = PyroMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = PYRO_OBJECT_MAP_AS_SET;
    map->obj.class = vm->class_set;
    return map;
}


PyroMap* PyroMap_new_as_weakref(PyroVM* vm) {
    PyroMap* map = PyroMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = PYRO_OBJECT_MAP_AS_WEAKREF;
    return map;
}


PyroMap* PyroMap_copy(PyroMap* src, PyroVM* vm) {
    PyroMap* dst = PyroMap_new(vm);
    if (!dst) {
        return NULL;
    }

    PyroMapEntry* entry_array = PYRO_ALLOCATE_ARRAY(vm, PyroMapEntry, src->entry_array_capacity);
    if (!entry_array) {
        return NULL;
    }
    memcpy(entry_array, src->entry_array, sizeof(PyroMapEntry) * src->entry_array_count);

    int64_t* index_array = PYRO_ALLOCATE_ARRAY(vm, int64_t, src->index_array_capacity);
    if (!index_array) {
        PYRO_FREE_ARRAY(vm, PyroMapEntry, entry_array, src->entry_array_capacity);
        return NULL;
    }
    memcpy(index_array, src->index_array, sizeof(int64_t) * src->index_array_capacity);

    dst->live_entry_count = src->live_entry_count;
    dst->max_load_threshold = src->max_load_threshold;

    dst->entry_array = entry_array;
    dst->entry_array_capacity = src->entry_array_capacity;
    dst->entry_array_count = src->entry_array_count;

    dst->index_array = index_array;
    dst->index_array_capacity = src->index_array_capacity;
    dst->index_array_count = src->index_array_count;

    return dst;
}


// This function appends a new entry to the map's entry array. It returns -1 if memory could not
// be allocated for the new entry -- in this case the map is unchanged. Otherwise it returns the
// index of the new entry.
static int64_t append_entry(PyroMap* map, PyroValue key, PyroValue value, PyroVM* vm) {
    if (map->entry_array_count == map->entry_array_capacity) {
        size_t new_entry_array_capacity = PYRO_GROW_CAPACITY(map->entry_array_capacity);
        PyroMapEntry* new_entry_array = PYRO_REALLOCATE_ARRAY(
            vm,
            PyroMapEntry,
            map->entry_array,
            map->entry_array_capacity,
            new_entry_array_capacity
        );
        if (!new_entry_array) {
            return -1;
        }
        map->entry_array = new_entry_array;
        map->entry_array_capacity = new_entry_array_capacity;
    }

    int64_t index = (int64_t)map->entry_array_count;
    map->entry_array[index].key = key;
    map->entry_array[index].value = value;
    map->entry_array_count++;
    return index;
}


int PyroMap_set(PyroMap* map, PyroValue key, PyroValue value, PyroVM* vm) {
    if (map->index_array_capacity == 0) {
        if (!resize_index_array(map, vm)) {
            return 0;
        }
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);

    // 1. The slot is empty.
    if (*slot == EMPTY) {
        if (map->index_array_count == map->max_load_threshold) {
            if (!resize_index_array(map, vm)) {
                return 0;
            }
            slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
        }
        int64_t index = append_entry(map, key, value, vm);
        if (index < 0) {
            return 0;
        }
        *slot = index;
        map->live_entry_count++;
        map->index_array_count++;
        return 1;
    }

    // 2. The slot contains a tombstone.
    if (*slot == TOMBSTONE) {
        int64_t index = append_entry(map, key, value, vm);
        if (index < 0) {
            return 0;
        }
        *slot = index;
        map->live_entry_count++;
        return 1;
    }

    // 3. The slot contains the index of an existing entry.
    map->entry_array[*slot].key = key;
    map->entry_array[*slot].value = value;
    return 2;
}


bool PyroMap_update_entry(PyroMap* map, PyroValue key, PyroValue value, PyroVM* vm) {
    if (map->live_entry_count == 0) {
        return false;
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
    if (*slot == EMPTY || *slot == TOMBSTONE) {
        return false;
    }

    map->entry_array[*slot].key = key;
    map->entry_array[*slot].value = value;
    return true;
}


bool PyroMap_get(PyroMap* map, PyroValue key, PyroValue* value, PyroVM* vm) {
    if (map->live_entry_count == 0) {
        return false;
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
    if (*slot == EMPTY || *slot == TOMBSTONE) {
        return false;
    }

    *value = map->entry_array[*slot].value;
    return true;
}


bool PyroMap_contains(PyroMap* map, PyroValue key, PyroVM* vm) {
    if (map->live_entry_count == 0) {
        return false;
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
    if (*slot == EMPTY || *slot == TOMBSTONE) {
        return false;
    }

    return true;
}


bool PyroMap_remove(PyroMap* map, PyroValue key, PyroVM* vm) {
    if (map->live_entry_count == 0) {
        return false;
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
    if (*slot == EMPTY || *slot == TOMBSTONE) {
        return false;
    }

    map->entry_array[*slot].key = pyro_tombstone();
    *slot = TOMBSTONE;
    map->live_entry_count--;
    return true;
}


bool PyroMap_copy_entries(PyroMap* src, PyroMap* dst, PyroVM* vm) {
    for (size_t i = 0; i < src->entry_array_count; i++) {
        PyroMapEntry* entry = &src->entry_array[i];
        if (PYRO_IS_TOMBSTONE(entry->key)) {
            continue;
        }
        if (PyroMap_set(dst, entry->key, entry->value, vm) == 0) {
            return false;
        }
    }
    return true;
}


/* ------- */
/* Strings */
/* ------- */


// Creates a new string object taking ownership of the heap-allocated array [bytes].
// - Adds the new string to the interned strings pool.
// - Returns NULL if memory cannot be allocated for the new string object.
// - Returns null if the string cannot be added to the string pool.
// - If the return value is NULL, ownership of [bytes] is returned to the caller.
static PyroStr* create_new_string(PyroVM* vm, char* bytes, size_t count, size_t capacity, uint64_t hash) {
    PyroStr* string = ALLOCATE_OBJECT(vm, PyroStr, PYRO_OBJECT_STR);
    if (!string) {
        return NULL;
    }

    string->obj.class = vm->class_str;
    string->count = count;
    string->capacity = capacity;
    string->hash = hash;
    string->bytes = bytes;

    if (!PyroMap_set(vm->string_pool, pyro_obj(string), pyro_null(), vm)) {
        string->count = 0;
        string->capacity = 0;
        string->hash = 0;
        string->bytes = NULL;
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_take(char* bytes, size_t count, size_t capacity, PyroVM* vm) {
    assert(bytes != NULL);
    assert(vm->string_pool != NULL);

    uint64_t hash = PYRO_STRING_HASH(bytes, count);

    PyroStr* interned_string = find_string_in_pool(vm->string_pool, bytes, count, hash);
    if (interned_string) {
        PYRO_FREE_ARRAY(vm, char, bytes, capacity);
        return interned_string;
    }

    return create_new_string(vm, bytes, count, capacity, hash);
}


PyroStr* PyroStr_copy(const char* src, size_t count, bool process_backslashed_escapes, PyroVM* vm) {
    assert(src != NULL);

    if (count == 0 && vm->empty_string) {
        return vm->empty_string;
    }

    if (process_backslashed_escapes) {
        size_t dst_capacity = count + 1;
        char* dst = PYRO_ALLOCATE_ARRAY(vm, char, dst_capacity);
        if (!dst) {
            return NULL;
        }

        size_t dst_count = pyro_process_backslashed_escapes(src, count, dst);
        dst[dst_count] = '\0';

        PyroStr* string = PyroStr_take(dst, dst_count, dst_capacity, vm);
        if (!string) {
            PYRO_FREE_ARRAY(vm, char, dst, dst_capacity);
            return NULL;
        }

        return string;
    }

    assert(vm->string_pool != NULL);
    uint64_t hash = PYRO_STRING_HASH(src, count);

    PyroStr* interned_string = find_string_in_pool(vm->string_pool, src, count, hash);
    if (interned_string) {
        return interned_string;
    }

    size_t dst_capacity = count + 1;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, dst_capacity);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src, count);
    dst[count] = '\0';

    PyroStr* string = create_new_string(vm, dst, count, dst_capacity, hash);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, dst_capacity);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_concat_n_copies(PyroStr* str, size_t n, PyroVM* vm) {
    if (n == 0 || str->count == 0) {
        return vm->empty_string;
    }

    size_t total_length = str->count * n;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, total_length + 1);
    if (!dst) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        memcpy(dst + str->count * i, str->bytes, str->count);
    }
    dst[total_length] = '\0';

    PyroStr* string = PyroStr_take(dst, total_length, total_length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, total_length + 1);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_concat_n_codepoints_as_utf8(uint32_t codepoint, size_t n, PyroVM* vm) {
    if (n == 0) {
        return vm->empty_string;
    }

    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t total_length = buf_count * n;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, total_length + 1);
    if (!dst) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        memcpy(dst + buf_count * i, buf, buf_count);
    }
    dst[total_length] = '\0';

    PyroStr* string = PyroStr_take(dst, total_length, total_length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, total_length + 1);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_concat(PyroStr* src1, PyroStr* src2, PyroVM* vm) {
    if (src1->count == 0) return src2;
    if (src2->count == 0) return src1;

    size_t length = src1->count + src2->count;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src1->bytes, src1->count);
    memcpy(dst + src1->count, src2->bytes, src2->count);
    dst[length] = '\0';

    PyroStr* string = PyroStr_take(dst, length, length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_prepend_codepoint_as_utf8(PyroStr* str, uint32_t codepoint, PyroVM* vm) {
    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t length = buf_count + str->count;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, buf, buf_count);
    memcpy(dst + buf_count, str->bytes, str->count);
    dst[length] = '\0';

    PyroStr* string = PyroStr_take(dst, length, length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_append_codepoint_as_utf8(PyroStr* str, uint32_t codepoint, PyroVM* vm) {
    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t length = str->count + buf_count;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, str->bytes, str->count);
    memcpy(dst + str->count, buf, buf_count);
    dst[length] = '\0';

    PyroStr* string = PyroStr_take(dst, length, length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


PyroStr* PyroStr_concat_codepoints_as_utf8(uint32_t cp1, uint32_t cp2, PyroVM* vm) {
    uint8_t buf1[4];
    size_t buf1_count = pyro_write_utf8_codepoint(cp1, buf1);

    uint8_t buf2[4];
    size_t buf2_count = pyro_write_utf8_codepoint(cp2, buf2);

    size_t length = buf1_count + buf2_count;
    char* dst = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, buf1, buf1_count);
    memcpy(dst + buf1_count, buf2, buf2_count);
    dst[length] = '\0';

    PyroStr* string = PyroStr_take(dst, length, length + 1, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


/* -------------- */
/* Pyro Functions */
/* -------------- */


PyroFn* PyroFn_new(PyroVM* vm) {
    PyroFn* fn = ALLOCATE_OBJECT(vm, PyroFn, PYRO_OBJECT_FN);
    if (!fn) {
        return NULL;
    }

    fn->arity = 0;
    fn->is_variadic = false;
    fn->upvalue_count = 0;
    fn->name = NULL;
    fn->source_id = NULL;

    fn->code = NULL;
    fn->code_count = 0;
    fn->code_capacity = 0;

    fn->constants = NULL;
    fn->constants_count = 0;
    fn->constants_capacity = 0;

    fn->first_line_number = 0;
    fn->bpl = NULL;
    fn->bpl_capacity = 0;

    return fn;
}


bool PyroFn_write(PyroFn* fn, uint8_t byte, size_t line_number, PyroVM* vm) {
    if (fn->code_count == fn->code_capacity) {
        size_t new_capacity = PYRO_GROW_CAPACITY(fn->code_capacity);
        uint8_t* new_array = PYRO_REALLOCATE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        fn->code_capacity = new_capacity;
        fn->code = new_array;
    }
    fn->code[fn->code_count++] = byte;

    if (fn->first_line_number == 0) {
        fn->first_line_number = line_number;
    }

    size_t offset = line_number - fn->first_line_number;

    if (fn->bpl_capacity < offset + 1) {
        size_t old_capacity = fn->bpl_capacity;
        size_t new_capacity = old_capacity + offset + 1 + 8; // allocating 8 spares is arbitrary
        uint16_t* new_array = PYRO_REALLOCATE_ARRAY(vm, uint16_t, fn->bpl, old_capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        fn->bpl_capacity = new_capacity;
        fn->bpl = new_array;
        for (size_t i = old_capacity; i < new_capacity; i++) {
            fn->bpl[i] = 0;
        }
    }
    fn->bpl[offset] += 1;

    return true;
}


size_t PyroFn_get_line_number(PyroFn* fn, size_t ip) {
    size_t offset = 0;
    size_t sum = 0;

    while (offset < fn->bpl_capacity) {
        sum += fn->bpl[offset];
        if (sum > ip) {
            break;
        }
        offset++;
    }

    return fn->first_line_number + offset;
}


int64_t PyroFn_add_constant(PyroFn* fn, PyroValue value, PyroVM* vm) {
    for (size_t i = 0; i < fn->constants_count; i++) {
        if (pyro_compare_eq_strict(value, fn->constants[i])) {
            return i;
        }
    }

    if (fn->constants_count == fn->constants_capacity) {
        size_t new_capacity = PYRO_GROW_CAPACITY(fn->constants_capacity);
        PyroValue* new_array = PYRO_REALLOCATE_ARRAY(vm, PyroValue, fn->constants, fn->constants_capacity, new_capacity);
        if (!new_array) {
            return -1;
        }
        fn->constants_capacity = new_capacity;
        fn->constants = new_array;
    }

    fn->constants[fn->constants_count++] = value;
    return fn->constants_count - 1;
}


size_t PyroFn_opcode_argcount(PyroFn* fn, size_t ip) {
    switch (fn->code[ip]) {
        case PYRO_OPCODE_ASSERT:
        case PYRO_OPCODE_BINARY_AMP:
        case PYRO_OPCODE_BINARY_BANG_EQUAL:
        case PYRO_OPCODE_BINARY_BAR:
        case PYRO_OPCODE_BINARY_CARET:
        case PYRO_OPCODE_BINARY_EQUAL_EQUAL:
        case PYRO_OPCODE_BINARY_GREATER:
        case PYRO_OPCODE_BINARY_GREATER_EQUAL:
        case PYRO_OPCODE_BINARY_GREATER_GREATER:
        case PYRO_OPCODE_BINARY_IN:
        case PYRO_OPCODE_BINARY_LESS:
        case PYRO_OPCODE_BINARY_LESS_EQUAL:
        case PYRO_OPCODE_BINARY_LESS_LESS:
        case PYRO_OPCODE_BINARY_MINUS:
        case PYRO_OPCODE_BINARY_PERCENT:
        case PYRO_OPCODE_BINARY_PLUS:
        case PYRO_OPCODE_BINARY_SLASH:
        case PYRO_OPCODE_BINARY_SLASH_SLASH:
        case PYRO_OPCODE_BINARY_STAR:
        case PYRO_OPCODE_BINARY_STAR_STAR:
        case PYRO_OPCODE_CLOSE_UPVALUE:
        case PYRO_OPCODE_DUP:
        case PYRO_OPCODE_DUP_2:
        case PYRO_OPCODE_GET_INDEX:
        case PYRO_OPCODE_GET_NEXT_FROM_ITERATOR:
        case PYRO_OPCODE_GET_ITERATOR:
        case PYRO_OPCODE_INHERIT:
        case PYRO_OPCODE_LOAD_FALSE:
        case PYRO_OPCODE_LOAD_I64_0:
        case PYRO_OPCODE_LOAD_I64_1:
        case PYRO_OPCODE_LOAD_I64_2:
        case PYRO_OPCODE_LOAD_I64_3:
        case PYRO_OPCODE_LOAD_I64_4:
        case PYRO_OPCODE_LOAD_I64_5:
        case PYRO_OPCODE_LOAD_I64_6:
        case PYRO_OPCODE_LOAD_I64_7:
        case PYRO_OPCODE_LOAD_I64_8:
        case PYRO_OPCODE_LOAD_I64_9:
        case PYRO_OPCODE_LOAD_NULL:
        case PYRO_OPCODE_LOAD_TRUE:
        case PYRO_OPCODE_POP:
        case PYRO_OPCODE_POP_ECHO_IN_REPL:
        case PYRO_OPCODE_RETURN:
        case PYRO_OPCODE_SET_INDEX:
        case PYRO_OPCODE_TRY:
        case PYRO_OPCODE_UNARY_BANG:
        case PYRO_OPCODE_UNARY_MINUS:
        case PYRO_OPCODE_UNARY_PLUS:
        case PYRO_OPCODE_UNARY_TILDE:
        case PYRO_OPCODE_START_WITH:
        case PYRO_OPCODE_END_WITH:
        case PYRO_OPCODE_GET_LOCAL_0:
        case PYRO_OPCODE_GET_LOCAL_1:
        case PYRO_OPCODE_GET_LOCAL_2:
        case PYRO_OPCODE_GET_LOCAL_3:
        case PYRO_OPCODE_GET_LOCAL_4:
        case PYRO_OPCODE_GET_LOCAL_5:
        case PYRO_OPCODE_GET_LOCAL_6:
        case PYRO_OPCODE_GET_LOCAL_7:
        case PYRO_OPCODE_GET_LOCAL_8:
        case PYRO_OPCODE_GET_LOCAL_9:
        case PYRO_OPCODE_SET_LOCAL_0:
        case PYRO_OPCODE_SET_LOCAL_1:
        case PYRO_OPCODE_SET_LOCAL_2:
        case PYRO_OPCODE_SET_LOCAL_3:
        case PYRO_OPCODE_SET_LOCAL_4:
        case PYRO_OPCODE_SET_LOCAL_5:
        case PYRO_OPCODE_SET_LOCAL_6:
        case PYRO_OPCODE_SET_LOCAL_7:
        case PYRO_OPCODE_SET_LOCAL_8:
        case PYRO_OPCODE_SET_LOCAL_9:
        case PYRO_OPCODE_STRINGIFY:
        case PYRO_OPCODE_FORMAT:
        case PYRO_OPCODE_LOAD_CONSTANT_0:
        case PYRO_OPCODE_LOAD_CONSTANT_1:
        case PYRO_OPCODE_LOAD_CONSTANT_2:
        case PYRO_OPCODE_LOAD_CONSTANT_3:
        case PYRO_OPCODE_LOAD_CONSTANT_4:
        case PYRO_OPCODE_LOAD_CONSTANT_5:
        case PYRO_OPCODE_LOAD_CONSTANT_6:
        case PYRO_OPCODE_LOAD_CONSTANT_7:
        case PYRO_OPCODE_LOAD_CONSTANT_8:
        case PYRO_OPCODE_LOAD_CONSTANT_9:
        case PYRO_OPCODE_CALL_VALUE_0:
        case PYRO_OPCODE_CALL_VALUE_1:
        case PYRO_OPCODE_CALL_VALUE_2:
        case PYRO_OPCODE_CALL_VALUE_3:
        case PYRO_OPCODE_CALL_VALUE_4:
        case PYRO_OPCODE_CALL_VALUE_5:
        case PYRO_OPCODE_CALL_VALUE_6:
        case PYRO_OPCODE_CALL_VALUE_7:
        case PYRO_OPCODE_CALL_VALUE_8:
        case PYRO_OPCODE_CALL_VALUE_9:
            return 0;

        case PYRO_OPCODE_CALL_VALUE:
        case PYRO_OPCODE_CALL_VALUE_WITH_UNPACK:
        case PYRO_OPCODE_ECHO:
        case PYRO_OPCODE_GET_LOCAL:
        case PYRO_OPCODE_GET_UPVALUE:
        case PYRO_OPCODE_IMPORT_ALL_MEMBERS:
        case PYRO_OPCODE_IMPORT_MODULE:
        case PYRO_OPCODE_SET_LOCAL:
        case PYRO_OPCODE_SET_UPVALUE:
        case PYRO_OPCODE_UNPACK:
        case PYRO_OPCODE_RETURN_TUPLE:
            return 1;

        case PYRO_OPCODE_BREAK:
        case PYRO_OPCODE_DEFINE_PRI_FIELD:
        case PYRO_OPCODE_DEFINE_PUB_FIELD:
        case PYRO_OPCODE_DEFINE_STATIC_FIELD:
        case PYRO_OPCODE_DEFINE_PRI_GLOBAL:
        case PYRO_OPCODE_DEFINE_PUB_GLOBAL:
        case PYRO_OPCODE_DEFINE_PRI_METHOD:
        case PYRO_OPCODE_DEFINE_PUB_METHOD:
        case PYRO_OPCODE_DEFINE_STATIC_METHOD:
        case PYRO_OPCODE_GET_FIELD:
        case PYRO_OPCODE_GET_PUB_FIELD:
        case PYRO_OPCODE_GET_GLOBAL:
        case PYRO_OPCODE_GET_MEMBER:
        case PYRO_OPCODE_GET_METHOD:
        case PYRO_OPCODE_GET_PUB_METHOD:
        case PYRO_OPCODE_GET_SUPER_METHOD:
        case PYRO_OPCODE_IMPORT_NAMED_MEMBERS:
        case PYRO_OPCODE_JUMP:
        case PYRO_OPCODE_JUMP_BACK:
        case PYRO_OPCODE_JUMP_IF_ERR:
        case PYRO_OPCODE_JUMP_IF_FALSE:
        case PYRO_OPCODE_JUMP_IF_NOT_ERR:
        case PYRO_OPCODE_JUMP_IF_NOT_KINDA_FALSEY:
        case PYRO_OPCODE_JUMP_IF_NOT_NULL:
        case PYRO_OPCODE_JUMP_IF_TRUE:
        case PYRO_OPCODE_LOAD_CONSTANT:
        case PYRO_OPCODE_MAKE_CLASS:
        case PYRO_OPCODE_MAKE_MAP:
        case PYRO_OPCODE_MAKE_SET:
        case PYRO_OPCODE_MAKE_VEC:
        case PYRO_OPCODE_POP_JUMP_IF_FALSE:
        case PYRO_OPCODE_SET_FIELD:
        case PYRO_OPCODE_SET_PUB_FIELD:
        case PYRO_OPCODE_SET_GLOBAL:
        case PYRO_OPCODE_CONCAT_STRINGS:
            return 2;

        case PYRO_OPCODE_CALL_METHOD:
        case PYRO_OPCODE_CALL_PUB_METHOD:
        case PYRO_OPCODE_CALL_METHOD_WITH_UNPACK:
        case PYRO_OPCODE_CALL_PUB_METHOD_WITH_UNPACK:
        case PYRO_OPCODE_CALL_SUPER_METHOD:
        case PYRO_OPCODE_CALL_SUPER_METHOD_WITH_UNPACK:
            return 3;

        // 2 bytes for the constant index, plus two for each upvalue.
        case PYRO_OPCODE_MAKE_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            PyroFn* closure_fn = PYRO_AS_PYRO_FN(fn->constants[const_index]);
            return 2 + closure_fn->upvalue_count * 2;
        }

        // 2 bytes for the constant index, 1 byte for the value count, plus two for each upvalue.
        case PYRO_OPCODE_MAKE_CLOSURE_WITH_DEF_ARGS: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            PyroFn* closure_fn = PYRO_AS_PYRO_FN(fn->constants[const_index]);
            return 2 + 1 + closure_fn->upvalue_count * 2;
        }

        // 1 byte for the count, plus two for each constant index.
        case PYRO_OPCODE_DEFINE_PRI_GLOBALS:
        case PYRO_OPCODE_DEFINE_PUB_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            return 1 + count * 2;
        }

        default:
            assert(false);
            fprintf(stderr, "Unhandled opcode in PyroFn_opcode_argcount().");
            exit(1);
    }
}


/* ---------------- */
/* Native Functions */
/* ---------------- */


PyroNativeFn* PyroNativeFn_new(PyroVM* vm, pyro_native_fn_t fn_ptr, const char* name, int arity) {
    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return NULL;
    }

    PyroNativeFn* func = ALLOCATE_OBJECT(vm, PyroNativeFn, PYRO_OBJECT_NATIVE_FN);
    if (!func) {
        return NULL;
    }

    func->fn_ptr = fn_ptr;
    func->arity = arity;
    func->name = name_string;
    return func;
}


/* ------------- */
/* Bound Methods */
/* ------------- */


PyroBoundMethod* PyroBoundMethod_new(PyroVM* vm, PyroValue receiver, PyroObject* method) {
    PyroBoundMethod* bound = ALLOCATE_OBJECT(vm, PyroBoundMethod, PYRO_OBJECT_BOUND_METHOD);
    if (!bound) {
        return NULL;
    }
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}


/* ------- */
/* Modules */
/* ------- */


PyroMod* PyroMod_new(PyroVM* vm) {
    PyroMod* module = ALLOCATE_OBJECT(vm, PyroMod, PYRO_OBJECT_MODULE);
    if (!module) {
        return NULL;
    }

    module->obj.class = vm->class_module;
    module->members = NULL;
    module->all_member_indexes = NULL;
    module->pub_member_indexes = NULL;

    module->members = PyroVec_new(vm);
    if (!module->members) {
        return NULL;
    }

    module->all_member_indexes = PyroMap_new(vm);
    if (!module->all_member_indexes) {
        return NULL;
    }

    module->pub_member_indexes = PyroMap_new(vm);
    if (!module->pub_member_indexes) {
        return NULL;
    }

    return module;
}


/* ------------------ */
/*  Vectors & Stacks  */
/* ------------------ */


PyroVec* PyroVec_new(PyroVM* vm) {
    PyroVec* vec = ALLOCATE_OBJECT(vm, PyroVec, PYRO_OBJECT_VEC);
    if (!vec) {
        return NULL;
    }
    vec->count = 0;
    vec->capacity = 0;
    vec->values = NULL;
    vec->obj.class = vm->class_vec;
    return vec;
}


void PyroVec_clear(PyroVec* vec, PyroVM* vm) {
    PYRO_FREE_ARRAY(vm, PyroValue, vec->values, vec->capacity);
    vec->count = 0;
    vec->capacity = 0;
    vec->values = NULL;
}


PyroVec* PyroVec_new_as_stack(PyroVM* vm) {
    PyroVec* stack = PyroVec_new(vm);
    if (!stack) {
        return NULL;
    }
    stack->obj.type = PYRO_OBJECT_VEC_AS_STACK;
    stack->obj.class = vm->class_stack;
    return stack;
}


PyroVec* PyroVec_new_with_capacity(size_t capacity, PyroVM* vm) {
    PyroVec* vec = PyroVec_new(vm);
    if (!vec) {
        return NULL;
    }

    if (capacity == 0) {
        return vec;
    }

    PyroValue* value_array = PYRO_ALLOCATE_ARRAY(vm, PyroValue, capacity);
    if (!value_array) {
        return NULL;
    }

    vec->capacity = capacity;
    vec->values = value_array;

    return vec;
}


PyroVec* PyroVec_copy(PyroVec* src_vec, PyroVM* vm) {
    PyroVec* new_vec = PyroVec_new_with_capacity(src_vec->count, vm);
    if (!new_vec) {
        return NULL;
    }
    memcpy(new_vec->values, src_vec->values, sizeof(PyroValue) * src_vec->count);
    return new_vec;
}


bool PyroVec_append(PyroVec* vec, PyroValue value, PyroVM* vm) {
    if (vec->count == vec->capacity) {
        size_t new_capacity = (vec->capacity < 8) ? 8 : (size_t)(vec->capacity * PYRO_VEC_MEMORY_MULTIPLIER);
        assert(new_capacity > vec->count);
        PyroValue* new_array = PYRO_REALLOCATE_ARRAY(vm, PyroValue, vec->values, vec->capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        vec->capacity = new_capacity;
        vec->values = new_array;
    }
    vec->values[vec->count++] = value;
    return true;
}


bool PyroVec_copy_entries(PyroVec* src, PyroVec* dst, PyroVM* vm) {
    for (size_t i = 0; i < src->count; i++) {
        if (!PyroVec_append(dst, src->values[i], vm)) {
            return false;
        }
    }
    return true;
}


PyroValue PyroVec_remove_last(PyroVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, "cannot remove last item from empty vector");
        return pyro_null();
    }
    vec->count--;
    return vec->values[vec->count];
}


PyroValue PyroVec_remove_first(PyroVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, "cannot remove first item from empty vector");
        return pyro_null();
    }

    if (vec->count == 1) {
        vec->count--;
        return vec->values[0];
    }

    PyroValue output = vec->values[0];

    vec->count--;
    memmove(vec->values, &vec->values[1], sizeof(PyroValue) * vec->count);

    return output;
}


PyroValue PyroVec_remove_at_index(PyroVec* vec, size_t index, PyroVM* vm) {
    if (index >= vec->count) {
        pyro_panic(vm, "index is out of range");
        return pyro_null();
    }

    if (vec->count == 1) {
        vec->count--;
        return vec->values[0];
    }

    if (index == vec->count - 1) {
        vec->count--;
        return vec->values[vec->count];
    }

    PyroValue output = vec->values[index];

    size_t bytes_to_move = sizeof(PyroValue) * (vec->count - index - 1);
    memmove(&vec->values[index], &vec->values[index + 1], bytes_to_move);
    vec->count--;

    return output;
}


void PyroVec_insert_at_index(PyroVec* vec, size_t index, PyroValue value, PyroVM* vm) {
    if (index > vec->count) {
        pyro_panic(vm, "index is out of range");
        return;
    }

    if (index == vec->count) {
        if (!PyroVec_append(vec, value, vm)) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (vec->count == vec->capacity) {
        size_t new_capacity = (vec->capacity < 8) ? 8 : (size_t)(vec->capacity * PYRO_VEC_MEMORY_MULTIPLIER);
        assert(new_capacity > vec->count);
        PyroValue* new_array = PYRO_REALLOCATE_ARRAY(vm, PyroValue, vec->values, vec->capacity, new_capacity);
        if (!new_array) {
            pyro_panic(vm, "out of memory");
            return;
        }
        vec->capacity = new_capacity;
        vec->values = new_array;
    }

    size_t bytes_to_move = sizeof(PyroValue) * (vec->count - index);
    memmove(&vec->values[index + 1], &vec->values[index], bytes_to_move);

    vec->values[index] = value;
    vec->count++;
}


/* ------- */
/* Buffers */
/* ------- */


PyroBuf* PyroBuf_new(PyroVM* vm) {
    PyroBuf* buf = ALLOCATE_OBJECT(vm, PyroBuf, PYRO_OBJECT_BUF);
    if (!buf) {
        return NULL;
    }
    buf->count = 0;
    buf->capacity = 0;
    buf->bytes = NULL;
    buf->obj.class = vm->class_buf;
    return buf;
}


PyroBuf* PyroBuf_new_with_capacity(size_t capacity, PyroVM* vm) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        return NULL;
    }

    if (capacity == 0) {
        return buf;
    }

    uint8_t* new_array = PYRO_ALLOCATE_ARRAY(vm, uint8_t, capacity);
    if (!new_array) {
        return NULL;
    }

    buf->bytes = new_array;
    buf->capacity = capacity;

    return buf;
}


PyroBuf* PyroBuf_new_from_string(PyroStr* string, PyroVM* vm) {
    size_t required_capacity = string->count + 1;

    PyroBuf* buf = PyroBuf_new_with_capacity(required_capacity, vm);
    if (!buf) {
        return NULL;
    }

    memcpy(buf->bytes, string->bytes, string->count);
    buf->count = string->count;
    return buf;
}


bool PyroBuf_resize_capacity(PyroBuf* buf, size_t new_capacity, PyroVM* vm) {
    assert(new_capacity >= buf->count);

    uint8_t* new_array = PYRO_REALLOCATE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity, new_capacity);
    if (!new_array) {
        return false;
    }

    buf->capacity = new_capacity;
    buf->bytes = new_array;
    return true;
}


PyroStr* PyroBuf_to_str(PyroBuf* buf, PyroVM* vm) {
    if (buf->count == 0) {
        return vm->empty_string;
    }

    if (!PyroBuf_resize_capacity(buf, buf->count + 1, vm)) {
        return NULL;
    }
    buf->bytes[buf->count] = '\0';

    PyroStr* string = PyroStr_take((char*)buf->bytes, buf->count, buf->capacity, vm);
    if (!string) {
        return NULL;
    }

    buf->count = 0;
    buf->capacity = 0;
    buf->bytes = NULL;

    return string;
}


void PyroBuf_clear(PyroBuf* buf, PyroVM* vm) {
    PYRO_FREE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity);
    buf->count = 0;
    buf->capacity = 0;
    buf->bytes = NULL;
}


bool PyroBuf_append_bytes(PyroBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm) {
    if (count == 0) {
        return true;
    }

    size_t required_capacity = buf->count + count + 1;

    if (required_capacity > buf->capacity) {
        size_t new_capacity = 8;
        if (required_capacity > 8) {
            new_capacity = (size_t)(required_capacity * PYRO_BUF_MEMORY_MULTIPLIER);
            assert(new_capacity >= required_capacity);
        }
        if (!PyroBuf_resize_capacity(buf, new_capacity, vm)) {
            return false;
        }
    }

    memcpy(&buf->bytes[buf->count], bytes, count);
    buf->count += count;

    return true;
}


bool PyroBuf_append_byte(PyroBuf* buf, uint8_t byte, PyroVM* vm) {
    return PyroBuf_append_bytes(buf, 1, &byte, vm);
}


bool PyroBuf_append_hex_escaped_byte(PyroBuf* buf, uint8_t byte, PyroVM* vm) {
    if (!PyroBuf_append_bytes(buf, 4, (uint8_t*)"\\x##", vm)) {
        return false;
    }

    char str[3];
    sprintf(str, "%02X", byte);

    buf->bytes[buf->count - 2] = str[0];
    buf->bytes[buf->count - 1] = str[1];

    return true;
}


int64_t PyroBuf_write_fv(PyroBuf* buf, PyroVM* vm, const char* format_string, va_list args) {
    // Determine the length of the string. (Doesn't include the terminating null.)
    // A negative length indicates a formatting error.
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format_string, args_copy);
    va_end(args_copy);

    if (length == 0) {
        return 0;
    }

    if (length < 0) {
        return -1;
    }

    size_t required_capacity = buf->count + (size_t)length + 1;

    if (required_capacity <= buf->capacity || PyroBuf_resize_capacity(buf, required_capacity, vm)) {
        vsprintf((char*)&buf->bytes[buf->count], format_string, args);
        buf->count += length;
        return length;
    }

    return -1;
}


int64_t PyroBuf_write_f(PyroBuf* buf, PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = PyroBuf_write_fv(buf, vm, format_string, args);
    va_end(args);
    return result;
}


void PyroBuf_try_write_fv(PyroBuf* buf, PyroVM* vm, const char* format_string, va_list args) {
    // Determine the length of the string we want to write. (Doesn't include the terminating null.)
    // A negative length indicates a formatting error.
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format_string, args_copy);
    va_end(args_copy);

    if (length <= 0) {
        return;
    }

    // Determine the capacity we would need to write the entire string.
    size_t required_capacity = buf->count + (size_t)length + 1;

    // If we can write the entire string, write it.
    if (buf->capacity >= required_capacity || PyroBuf_resize_capacity(buf, required_capacity, vm)) {
        vsprintf((char*)&buf->bytes[buf->count], format_string, args);
        buf->count += length;
        return;
    }

    // Try to grow the buffer as much as possible.
    while (buf->capacity < required_capacity) {
        if (!PyroBuf_resize_capacity(buf, buf->capacity + 128, vm)) {
            break;
        }
    }

    // Check again as the buffer may now be big enough to fit the entire string.
    if (buf->capacity >= required_capacity) {
        vsprintf((char*)&buf->bytes[buf->count], format_string, args);
        buf->count += length;
        return;
    }

    // The buffer is still too small. Write as much of the string as possible.
    size_t available_capacity = buf->capacity - buf->count;
    if (available_capacity > 0) {
        vsnprintf((char*)&buf->bytes[buf->count], available_capacity, format_string, args);
        buf->count += (available_capacity - 1);
    }
}


/* ----- */
/* Files */
/* ----- */


PyroFile* PyroFile_new(PyroVM* vm, FILE* stream) {
    PyroFile* file = ALLOCATE_OBJECT(vm, PyroFile, PYRO_OBJECT_FILE);
    if (!file) {
        return NULL;
    }
    file->obj.class = vm->class_file;
    file->stream = stream;
    file->path = NULL;
    return file;
}


PyroStr* PyroFile_read_line(PyroFile* file, PyroVM* vm) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    while (true) {
        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, "I/O read error");
                return NULL;
            }
            break;
        }

        if (!PyroBuf_append_byte(buf, c, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        if (c == '\n') {
            break;
        }
    }

    if (buf->count == 0) {
        return NULL;
    }

    if (buf->count >= 2 && buf->bytes[buf->count - 2] == '\r' && buf->bytes[buf->count - 1] == '\n') {
        buf->count -= 2;
    }

    if (buf->count >= 1 && buf->bytes[buf->count - 1] == '\n') {
        buf->count -= 1;
    }

    PyroStr* string = PyroBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return string;
}


PyroBuf* PyroFile_read(PyroFile* file, const char* err_prefix, PyroVM* vm) {
    FILE* file_stream = file->stream;

    // If the file is seekable, we want to determine its size and read it using a single fread().
    // - In theory, fseek() returns 0 if the file is seekable, -1 if not.
    // - In practice, if the file stream is connected to an interactive terminal, fseek() returns 0
    //   but has no effect. In this case, [end_pos - start_pos] can be negative.
    int64_t start_pos = ftell(file_stream);
    int seek_result = fseek(file_stream, 0, SEEK_END);
    int64_t end_pos = ftell(file_stream);
    fseek(file_stream, start_pos, SEEK_SET);

    size_t required_capacity = 0;
    if (seek_result == 0 && end_pos > start_pos) {
        required_capacity = (end_pos - start_pos) + 1;
    }

    PyroBuf* buf = PyroBuf_new_with_capacity(required_capacity, vm);
    if (!buf) {
        pyro_panic(vm, "%s: out of memory", err_prefix);
        return NULL;
    }

    if (seek_result == 0 && end_pos > start_pos) {
        size_t file_size = end_pos - start_pos;

        size_t num_bytes_read = fread(buf->bytes, sizeof(uint8_t), file_size, file_stream);
        if (num_bytes_read < file_size) {
            pyro_panic(vm, "%s: error reading file", err_prefix);
            return NULL;
        }

        buf->count = num_bytes_read;
    }

    while (true) {
        int c = fgetc(file_stream);

        if (c == EOF) {
            if (ferror(file_stream)) {
                pyro_panic(vm, "%s: error reading file", err_prefix);
                return NULL;
            }
            break;
        }

        if (!PyroBuf_append_byte(buf, c, vm)) {
            pyro_panic(vm, "%s: out of memory", err_prefix);
            return NULL;
        }
    }

    return buf;
}


/* --------- */
/* Iterators */
/* --------- */


PyroIter* PyroIter_new(PyroObject* source, PyroIterType iter_type, PyroVM* vm) {
    PyroIter* iter = ALLOCATE_OBJECT(vm, PyroIter, PYRO_OBJECT_ITER);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->class_iter;
    iter->source = source;
    iter->iter_type = iter_type;
    iter->next_index = 0;
    iter->next_enum = 0;
    iter->callback = NULL;
    iter->range_next = 0;
    iter->range_stop = 0;
    iter->range_step = 0;
    iter->next_queue_item = NULL;
    return iter;
}


PyroIter* PyroIter_empty(PyroVM* vm) {
    return PyroIter_new(NULL, PYRO_ITER_EMPTY, vm);
}


PyroValue PyroIter_next(PyroIter* iter, PyroVM* vm) {
    switch (iter->iter_type) {
        case PYRO_ITER_EMPTY: {
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_VEC: {
            PyroVec* vec = (PyroVec*)iter->source;
            if (iter->next_index < vec->count) {
                iter->next_index++;
                PyroValue result = vec->values[iter->next_index - 1];
                return result;
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_TUP: {
            PyroTup* tup = (PyroTup*)iter->source;
            if (iter->next_index < tup->count) {
                iter->next_index++;
                return tup->values[iter->next_index - 1];
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_QUEUE: {
            if (iter->next_queue_item) {
                PyroValue next_value = iter->next_queue_item->value;
                iter->next_queue_item = iter->next_queue_item->next;
                return next_value;
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_STR: {
            PyroStr* str = (PyroStr*)iter->source;
            if (iter->next_index < str->count) {
                PyroStr* new_str = PyroStr_copy(&str->bytes[iter->next_index], 1, false, vm);
                if (!new_str) {
                    pyro_panic(vm, "out of memory");
                    return pyro_obj(vm->error);
                }
                iter->next_index++;
                return pyro_obj(new_str);
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_STR_BYTES: {
            PyroStr* str = (PyroStr*)iter->source;
            if (iter->next_index < str->count) {
                int64_t byte_value = (uint8_t)str->bytes[iter->next_index];
                iter->next_index++;
                return pyro_i64(byte_value);
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_STR_CHARS: {
            PyroStr* str = (PyroStr*)iter->source;
            if (iter->next_index < str->count) {
                uint8_t* src = (uint8_t*)&str->bytes[iter->next_index];
                size_t src_len = str->count - iter->next_index;
                Utf8CodePoint cp;
                if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
                    iter->next_index += cp.length;
                    return pyro_char(cp.value);
                }
                pyro_panic(
                    vm,
                    "String contains invalid utf-8 at byte index %zu",
                    iter->next_index
                );
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_MAP_KEYS: {
            PyroMap* map = (PyroMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                PyroMapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (PYRO_IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->key;
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_MAP_VALUES: {
            PyroMap* map = (PyroMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                PyroMapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (PYRO_IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->value;
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_MAP_ENTRIES: {
            PyroMap* map = (PyroMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                PyroMapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (PYRO_IS_TOMBSTONE(entry->key)) {
                    continue;
                }

                PyroTup* tup = PyroTup_new(2, vm);
                if (!tup) {
                    pyro_panic(vm, "out of memory");
                    return pyro_obj(vm->error);
                }

                tup->values[0] = entry->key;
                tup->values[1] = entry->value;
                return pyro_obj(tup);
            }
            return pyro_obj(vm->error);
        }

        case PYRO_ITER_FUNC_MAP: {
            PyroIter* src_iter = (PyroIter*)iter->source;
            PyroValue next_value = PyroIter_next(src_iter, vm);
            if (PYRO_IS_ERR(next_value)) {
                return next_value;
            }

            pyro_push(vm, pyro_obj(iter->callback));
            pyro_push(vm, next_value);
            PyroValue result = pyro_call_function(vm, 1);
            if (vm->halt_flag) {
                return pyro_obj(vm->error);
            }

            return result;
        }

        case PYRO_ITER_FUNC_FILTER: {
            PyroIter* src_iter = (PyroIter*)iter->source;

            while (true) {
                PyroValue next_value = PyroIter_next(src_iter, vm);
                if (PYRO_IS_ERR(next_value)) {
                    return next_value;
                }

                pyro_push(vm, pyro_obj(iter->callback));
                pyro_push(vm, next_value);
                PyroValue result = pyro_call_function(vm, 1);
                if (vm->halt_flag) {
                    return pyro_obj(vm->error);
                }

                if (pyro_is_truthy(result)) {
                    return next_value;
                }
            }
        }

        case PYRO_ITER_ENUMERATE: {
            PyroIter* src_iter = (PyroIter*)iter->source;
            PyroValue next_value = PyroIter_next(src_iter, vm);
            if (PYRO_IS_ERR(next_value)) {
                return next_value;
            }

            PyroTup* tup = PyroTup_new(2, vm);
            if (!tup) {
                pyro_panic(vm, "out of memory");
                return pyro_obj(vm->error);
            }

            tup->values[0] = pyro_i64(iter->next_enum);
            tup->values[1] = next_value;
            iter->next_enum++;

            return pyro_obj(tup);
        }

        case PYRO_ITER_GENERIC: {
            PyroValue next_method = pyro_get_method(vm, pyro_obj(iter->source), vm->str_dollar_next);
            pyro_push(vm, pyro_obj(iter->source));
            PyroValue result = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return pyro_obj(vm->error);
            }
            return result;
        }

        case PYRO_ITER_RANGE: {
            if (iter->range_step > 0) {
                if (iter->range_next < iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return pyro_i64(range_next);
                }
                return pyro_obj(vm->error);
            }

            if (iter->range_step < 0) {
                if (iter->range_next > iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return pyro_i64(range_next);
                }
                return pyro_obj(vm->error);
            }

            return pyro_obj(vm->error);
        }

        case PYRO_ITER_STR_LINES: {
            PyroStr* str = (PyroStr*)iter->source;
            if (iter->next_index == str->count) {
                return pyro_obj(vm->error);
            }

            const char* const line_start = str->bytes + iter->next_index;
            size_t count = 0;

            while (iter->next_index < str->count) {
                count++;
                if (str->bytes[iter->next_index++] == '\n') {
                    break;
                }
            }

            if (count >= 2 && line_start[count - 2] == '\r' && line_start[count - 1] == '\n') {
                count -= 2;
            }

            if (count >= 1 && line_start[count - 1] == '\n') {
                count -= 1;
            }

            PyroStr* next_line = PyroStr_copy(line_start, count, false, vm);
            if (!next_line) {
                pyro_panic(vm, "out of memory");
                return pyro_obj(vm->error);
            }

            return pyro_obj(next_line);
        }

        case PYRO_ITER_FILE_LINES: {
            // If the source is NULL, the iterator has been exhausted.
            if (iter->source == NULL) {
                return pyro_obj(vm->error);
            }

            PyroFile* file = (PyroFile*)iter->source;
            PyroStr* next_line = PyroFile_read_line(file, vm);
            if (vm->halt_flag) {
                return pyro_obj(vm->error);
            }

            // A NULL return value means the file has been exhausted.
            if (!next_line) {
                iter->source = NULL;
                return pyro_obj(vm->error);
            }

            return pyro_obj(next_line);
        }

        default:
            assert(false);
            return pyro_obj(vm->error);
    }
}


PyroStr* PyroIter_join(PyroIter* iter, const char* sep, size_t sep_length, PyroVM* vm) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    bool is_first_item = true;

    while (true) {
        PyroValue next_value = PyroIter_next(iter, vm);
        if (vm->halt_flag) {
            return NULL;
        }
        if (PYRO_IS_ERR(next_value)) {
            break;
        }

        if (!is_first_item) {
            if (!PyroBuf_append_bytes(buf, sep_length, (uint8_t*)sep, vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }

        pyro_push(vm, next_value); // Stringification can call into Pyro code and trigger the GC.
        PyroStr* value_string = pyro_stringify_value(vm, next_value);
        if (vm->halt_flag) {
            return NULL;
        }
        pyro_pop(vm); // next_value

        if (!PyroBuf_append_bytes(buf, value_string->count, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        is_first_item = false;
    }

    PyroStr* output_string = PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


/* ------ */
/* Queues */
/* ------ */


PyroQueue* PyroQueue_new(PyroVM* vm) {
    PyroQueue* queue = ALLOCATE_OBJECT(vm, PyroQueue, PYRO_OBJECT_QUEUE);
    if (!queue) {
        return NULL;
    }
    queue->obj.class = vm->class_queue;
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    return queue;
}


void PyroQueue_clear(PyroQueue* queue, PyroVM* vm) {
    PyroQueueItem* next_item = queue->head;
    while (next_item) {
        PyroQueueItem* current_item = next_item;
        next_item = current_item->next;
        pyro_realloc(vm, current_item, sizeof(PyroQueueItem), 0);
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
}


// Add the new item to the end of the linked list.
bool PyroQueue_enqueue(PyroQueue* queue, PyroValue value, PyroVM* vm) {
    PyroQueueItem* item = pyro_realloc(vm, NULL, 0, sizeof(PyroQueueItem));
    if (!item) {
        return false;
    }
    item->value = value;
    item->next = NULL;

    if (queue->count == 0) {
        queue->head = item;
        queue->tail = item;
    } else {
        queue->tail->next = item;
        queue->tail = item;
    }

    queue->count++;
    return true;
}


// Remove the item at the front of the linked list.
bool PyroQueue_dequeue(PyroQueue* queue, PyroValue* value, PyroVM* vm) {
    if (queue->count == 0) {
        return false;
    }

    PyroQueueItem* item = queue->head;
    *value = item->value;

    if (queue->count == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = item->next;
    }

    pyro_realloc(vm, item, sizeof(PyroQueueItem), 0);
    queue->count--;
    return true;
}


// Return the item at the front of the linked list without removing it.
bool PyroQueue_peek(PyroQueue* queue, PyroValue* value, PyroVM* vm) {
    if (queue->count == 0) {
        return false;
    }

    PyroQueueItem* item = queue->head;
    *value = item->value;
    return true;
}


/* ------------------- */
/*  Resource Pointers  */
/* ------------------- */


PyroResourcePointer* PyroResourcePointer_new(void* pointer, pyro_free_rp_callback_t callback, PyroVM* vm) {
    PyroResourcePointer* resource = ALLOCATE_OBJECT(vm, PyroResourcePointer, PYRO_OBJECT_RESOURCE_POINTER);
    if (!resource) {
        return NULL;
    }
    resource->obj.class = NULL; // TODO: create this class
    resource->pointer = pointer;
    resource->callback = callback;
    return resource;
}


/* -------- */
/*  Errors  */
/* -------- */


PyroErr* PyroErr_new(PyroVM* vm) {
    PyroErr* err = ALLOCATE_OBJECT(vm, PyroErr, PYRO_OBJECT_ERR);
    if (!err) {
        return NULL;
    }

    err->obj.class = vm->class_err;
    err->message = vm->empty_string;

    err->details = PyroMap_new(vm);
    if (!err->details) {
        return NULL;
    }

    return err;
}
