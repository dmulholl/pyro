#include "values.h"
#include "objects.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "opcodes.h"
#include "utf8.h"
#include "operators.h"
#include "exec.h"
#include "stringify.h"
#include "panics.h"


// Allocates memory for a fixed-size object.
#define ALLOCATE_OBJECT(vm, type, type_enum) \
    (type*)allocate_object(vm, sizeof(type), type_enum)


// Allocates memory for a flexibly-sized object, e.g. a tuple.
#define ALLOCATE_FLEX_OBJECT(vm, type, type_enum, value_count, value_type) \
    (type*)allocate_object(vm, sizeof(type) + value_count * sizeof(value_type), type_enum)


// Allocates memory for a new object. Automatically adds the new object to the VM's linked list of
// all heap-allocated objects. Returns NULL if memory cannot be allocated.
static Obj* allocate_object(PyroVM* vm, size_t size, ObjType type) {
    Obj* object = pyro_realloc(vm, NULL, 0, size);
    if (object == NULL) {
        return NULL;
    }

    object->type = type;
    object->is_marked = false;
    object->class = NULL;

    object->next = vm->objects;
    vm->objects = object;

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, ">> %p allocate %zu bytes for %s\n",
            (void*)object,
            size,
            pyro_stringify_object_type(type)
        );
    #endif

    return object;
}


/* ------ */
/* Tuples */
/* ------ */


ObjTup* ObjTup_new(size_t count, PyroVM* vm) {
    ObjTup* tup = ALLOCATE_FLEX_OBJECT(vm, ObjTup, OBJ_TUP, count, Value);
    if (!tup) {
        return NULL;
    }

    tup->obj.class = vm->class_tup;
    tup->count = count;
    for (size_t i = 0; i < count; i++) {
        tup->values[i] = MAKE_NULL();
    }
    return tup;
}


uint64_t ObjTup_hash(PyroVM* vm, ObjTup* tup) {
    uint64_t hash = 0;
    for (size_t i = 0; i < tup->count; i++) {
        hash ^= pyro_hash_value(vm, tup->values[i]);
    }
    return hash;
}


bool ObjTup_check_equal(ObjTup* a, ObjTup* b, PyroVM* vm) {
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


ObjClosure* ObjClosure_new(PyroVM* vm, ObjFn* fn, ObjModule* module) {
    ObjClosure* closure = ALLOCATE_OBJECT(vm, ObjClosure, OBJ_CLOSURE);
    if (!closure) {
        return NULL;
    }

    closure->fn = fn;
    closure->module = module;
    closure->upvalues = NULL;
    closure->upvalue_count = 0;

    if (fn->upvalue_count > 0) {
        ObjUpvalue** array = ALLOCATE_ARRAY(vm, ObjUpvalue*, fn->upvalue_count);
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


ObjUpvalue* ObjUpvalue_new(PyroVM* vm, Value* addr) {
    ObjUpvalue* upvalue = ALLOCATE_OBJECT(vm, ObjUpvalue, OBJ_UPVALUE);
    if (!upvalue) {
        return NULL;
    }
    upvalue->location = addr;
    upvalue->closed = MAKE_NULL();
    upvalue->next = NULL;
    return upvalue;
}


/* ------- */
/* Classes */
/* ------- */


ObjClass* ObjClass_new(PyroVM* vm) {
    ObjClass* class = ALLOCATE_OBJECT(vm, ObjClass, OBJ_CLASS);
    if (!class) {
        return NULL;
    }

    class->name = NULL;
    class->methods = NULL;
    class->field_values = NULL;
    class->field_indexes = NULL;
    class->superclass = NULL;

    class->methods = ObjMap_new(vm);
    class->field_values = ObjVec_new(vm);
    class->field_indexes = ObjMap_new(vm);

    if (!class->methods || !class->field_values || !class->field_indexes) {
        return NULL;
    }

    return class;
}


/* --------- */
/* Instances */
/* --------- */


ObjInstance* ObjInstance_new(PyroVM* vm, ObjClass* class) {
    size_t num_fields = class->field_values->count;

    ObjInstance* instance = ALLOCATE_FLEX_OBJECT(vm, ObjInstance, OBJ_INSTANCE, num_fields, Value);
    if (!instance) {
        return NULL;
    }

    instance->obj.class = class;
    if (num_fields > 0) {
        memcpy(instance->fields, class->field_values->values, sizeof(Value) * num_fields);
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
// - If the slot is empty or contains a tombstone, the map does not contain an entry matching [key].
//   If a new entry with [key] is appended to [entry_array], its index should be inserted into the
//   slot returned by this function.
//
// - If the slot contains a non-negative index value, the map contains an entry matching [key].
//   This entry can be found in [entry_array] at the specified index.
//
// Note that [index_array] must have a non-zero length before this function is called.
static int64_t* find_entry(
    PyroVM* vm,
    MapEntry* entry_array,
    int64_t* index_array,
    size_t index_array_capacity,
    Value key
) {
    // Capacity is always a power of 2 so we can use bitwise-AND as a fast modulo operator, i.e.
    // this is equivalent to: i = key_hash % index_array_capacity.
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


// This function is only used for looking up strings in the interned strings pool. If the pool
// contains an entry identical to [string], it returns a pointer to that entry, otherwise it
// returns NULL.
static ObjStr* find_string(ObjMap* map, const char* string, size_t length, uint64_t hash) {
    if (map->live_entry_count == 0) {
        return NULL;
    }

    size_t i = (size_t)hash & (map->index_array_capacity - 1);

    for (;;) {
        int64_t* slot = &map->index_array[i];

        if (*slot == EMPTY) {
            return NULL;
        } else if (*slot == TOMBSTONE) {
            // Skip over the tombstone and keep looking for a matching entry.
        } else {
            ObjStr* entry = AS_STR(map->entry_array[*slot].key);
            if (entry->length == length) {
                if (entry->hash == hash) {
                    if (memcmp(entry->bytes, string, length) == 0) {
                        return entry;
                    }
                }
            }
        }

        i = (i + 1) & (map->index_array_capacity - 1);
    }
}


// This function doubles the capacity of the index array, then rebuilds the index. (Rebuilding the
// index has the side-effect of eliminating any tombstone slots. This function also takes the
// opportunity to eliminate any tombstones from the entry array so when this function returns
// the map contains no tombstone entries.)
static bool resize_index_array(ObjMap* map, PyroVM* vm) {
    size_t new_index_array_capacity = GROW_CAPACITY(map->index_array_capacity);

    int64_t* new_index_array = ALLOCATE_ARRAY(vm, int64_t, new_index_array_capacity);
    if (!new_index_array) {
        return false;
    }

    for (size_t i = 0; i < new_index_array_capacity; i++) {
        new_index_array[i] = EMPTY;
    }

    FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
    map->index_array = new_index_array;
    map->index_array_capacity = new_index_array_capacity;
    map->index_array_count = map->live_entry_count;
    map->max_load_threshold = new_index_array_capacity * PYRO_MAX_HASHMAP_LOAD;

    // 1. If the entry array contains tombstones, this is a good time to compact it by removing
    // them while we're rebuilding the index.
    if (map->entry_array_count > map->live_entry_count) {
        size_t dst_index = 0;

        for (size_t src_index = 0; src_index < map->entry_array_count; src_index++) {
            if (IS_TOMBSTONE(map->entry_array[src_index].key)) {
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

    // 2. The entry array doesn't contain any tombstones so all we need to do is rebuild the index.
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


ObjMap* ObjMap_new(PyroVM* vm) {
    ObjMap* map = ALLOCATE_OBJECT(vm, ObjMap, OBJ_MAP);
    if (!map) {
        return NULL;
    }

    map->live_entry_count = 0;
    map->entry_array = NULL;
    map->entry_array_capacity = 0;
    map->entry_array_count = 0;
    map->index_array = NULL;
    map->index_array_capacity = 0;
    map->index_array_count = 0;
    map->max_load_threshold = 0;
    map->obj.class = vm->class_map;
    return map;
}


ObjMap* ObjMap_new_as_set(PyroVM* vm) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = OBJ_MAP_AS_SET;
    map->obj.class = vm->class_set;
    return map;
}


ObjMap* ObjMap_new_as_weakref(PyroVM* vm) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = OBJ_MAP_AS_WEAKREF;
    return map;
}


ObjMap* ObjMap_copy(ObjMap* src, PyroVM* vm) {
    ObjMap* dst = ObjMap_new(vm);
    if (!dst) {
        return NULL;
    }

    MapEntry* entry_array = ALLOCATE_ARRAY(vm, MapEntry, src->entry_array_capacity);
    if (!entry_array) {
        return NULL;
    }
    memcpy(entry_array, src->entry_array, sizeof(MapEntry) * src->entry_array_count);

    int64_t* index_array = ALLOCATE_ARRAY(vm, int64_t, src->index_array_capacity);
    if (!index_array) {
        FREE_ARRAY(vm, MapEntry, entry_array, src->entry_array_capacity);
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
static int64_t append_entry(ObjMap* map, Value key, Value value, PyroVM* vm) {
    if (map->entry_array_count == map->entry_array_capacity) {
        size_t new_entry_array_capacity = GROW_CAPACITY(map->entry_array_capacity);
        MapEntry* new_entry_array = REALLOCATE_ARRAY(
            vm,
            MapEntry,
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


int ObjMap_set(ObjMap* map, Value key, Value value, PyroVM* vm) {
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


bool ObjMap_update_entry(ObjMap* map, Value key, Value value, PyroVM* vm) {
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


bool ObjMap_get(ObjMap* map, Value key, Value* value, PyroVM* vm) {
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


bool ObjMap_remove(ObjMap* map, Value key, PyroVM* vm) {
    if (map->live_entry_count == 0) {
        return false;
    }

    int64_t* slot = find_entry(vm, map->entry_array, map->index_array, map->index_array_capacity, key);
    if (*slot == EMPTY || *slot == TOMBSTONE) {
        return false;
    }

    map->entry_array[*slot].key = MAKE_TOMBSTONE();
    *slot = TOMBSTONE;
    map->live_entry_count--;
    return true;
}


bool ObjMap_copy_entries(ObjMap* src, ObjMap* dst, PyroVM* vm) {
    for (size_t i = 0; i < src->entry_array_count; i++) {
        MapEntry* entry = &src->entry_array[i];
        if (IS_TOMBSTONE(entry->key)) {
            continue;
        }
        if (ObjMap_set(dst, entry->key, entry->value, vm) == 0) {
            return false;
        }
    }
    return true;
}


/* ------- */
/* Strings */
/* ------- */


// Creates a new string object taking ownership of a null-terminated, heap-allocated byte array,
// where [length] is the number of bytes in the array, not including the terminating null. The
// caller should already have verified that an identical string does not exist in the interned
// strings pool. Returns NULL if the attempt to allocate memory for the object fails -- in this
// case the input array is not altered or freed.
static ObjStr* allocate_string(PyroVM* vm, char* bytes, size_t length, uint64_t hash) {
    ObjStr* string = ALLOCATE_OBJECT(vm, ObjStr, OBJ_STR);
    if (!string) {
        return NULL;
    }

    string->length = length;
    string->hash = hash;
    string->bytes = bytes;
    string->obj.class = vm->class_str;

    if (ObjMap_set(vm->strings, MAKE_OBJ(string), MAKE_NULL(), vm) == 0) {
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_take(char* src, size_t length, PyroVM* vm) {
    assert(vm->strings != NULL);

    uint64_t hash = PYRO_STRING_HASH(src, length);
    ObjStr* interned = find_string(vm->strings, src, length, hash);
    if (interned) {
        FREE_ARRAY(vm, char, src, length + 1);
        return interned;
    }

    return allocate_string(vm, src, length, hash);
}


ObjStr* ObjStr_empty(PyroVM* vm) {
    if (vm->empty_string) {
        return vm->empty_string;
    }

    char* bytes = ALLOCATE_ARRAY(vm, char, 1);
    if (!bytes) {
        return NULL;
    }

    bytes[0] = '\0';

    ObjStr* string = ObjStr_take(bytes, 0, vm);
    if (!string) {
        FREE_ARRAY(vm, char, bytes, 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_copy_esc(const char* src, size_t length, PyroVM* vm) {
    if (length == 0) {
        return ObjStr_empty(vm);
    }

    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    size_t count = pyro_unescape_string(src, length, dst);
    dst[count] = '\0';

    // If there were no backslashed escapes, [count] will be equal to [length].
    // If there were escapes, [count] will be less than [length].
    if (count < length) {
        dst = REALLOCATE_ARRAY(vm, char, dst, length + 1, count + 1);
    }

    ObjStr* string = ObjStr_take(dst, count, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, count + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_copy_raw(const char* src, size_t length, PyroVM* vm) {
    if (length == 0) {
        return ObjStr_empty(vm);
    }

    uint64_t hash = PYRO_STRING_HASH(src, length);
    assert(vm->strings != NULL);
    ObjStr* interned = find_string(vm->strings, src, length, hash);
    if (interned) {
        return interned;
    }

    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src, length);
    dst[length] = '\0';

    ObjStr* string = allocate_string(vm, dst, length, hash);
    if (!string) {
        FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_concat_n_copies(ObjStr* str, size_t n, PyroVM* vm) {
    if (n == 0 || str->length == 0) {
        return ObjStr_empty(vm);
    }

    size_t total_length = str->length * n;
    char* dst = ALLOCATE_ARRAY(vm, char, total_length + 1);
    if (!dst) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        memcpy(dst + str->length * i, str->bytes, str->length);
    }
    dst[total_length] = '\0';

    ObjStr* string = ObjStr_take(dst, total_length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, total_length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_concat_n_codepoints_as_utf8(uint32_t codepoint, size_t n, PyroVM* vm) {
    if (n == 0) {
        return ObjStr_empty(vm);
    }

    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t total_length = buf_count * n;
    char* dst = ALLOCATE_ARRAY(vm, char, total_length + 1);
    if (!dst) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        memcpy(dst + buf_count * i, buf, buf_count);
    }
    dst[total_length] = '\0';

    ObjStr* string = ObjStr_take(dst, total_length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, total_length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_concat(ObjStr* src1, ObjStr* src2, PyroVM* vm) {
    if (src1->length == 0) return src2;
    if (src2->length == 0) return src1;

    size_t length = src1->length + src2->length;
    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, src1->bytes, src1->length);
    memcpy(dst + src1->length, src2->bytes, src2->length);
    dst[length] = '\0';

    ObjStr* string = ObjStr_take(dst, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_prepend_codepoint_as_utf8(ObjStr* str, uint32_t codepoint, PyroVM* vm) {
    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t length = buf_count + str->length;
    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, buf, buf_count);
    memcpy(dst + buf_count, str->bytes, str->length);
    dst[length] = '\0';

    ObjStr* string = ObjStr_take(dst, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_append_codepoint_as_utf8(ObjStr* str, uint32_t codepoint, PyroVM* vm) {
    uint8_t buf[4];
    size_t buf_count = pyro_write_utf8_codepoint(codepoint, buf);

    size_t length = str->length + buf_count;
    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, str->bytes, str->length);
    memcpy(dst + str->length, buf, buf_count);
    dst[length] = '\0';

    ObjStr* string = ObjStr_take(dst, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_concat_codepoints_as_utf8(uint32_t cp1, uint32_t cp2, PyroVM* vm) {
    uint8_t buf1[4];
    size_t buf1_count = pyro_write_utf8_codepoint(cp1, buf1);

    uint8_t buf2[4];
    size_t buf2_count = pyro_write_utf8_codepoint(cp2, buf2);

    size_t length = buf1_count + buf2_count;
    char* dst = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!dst) {
        return NULL;
    }

    memcpy(dst, buf1, buf1_count);
    memcpy(dst + buf1_count, buf2, buf2_count);
    dst[length] = '\0';

    ObjStr* string = ObjStr_take(dst, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, dst, length + 1);
        return NULL;
    }

    return string;
}


ObjStr* ObjStr_esc_percents(const char* src, size_t length, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new_with_cap(length, vm);
    if (!buf) {
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        if (src[i] == '%') {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)"%%", vm)) {
                return NULL;
            }
        } else {
            if (!ObjBuf_append_byte(buf, src[i], vm)) {
                return NULL;
            }
        }
    }

    return ObjBuf_to_str(buf, vm);
}


/* -------------- */
/* Pyro Functions */
/* -------------- */


ObjFn* ObjFn_new(PyroVM* vm) {
    ObjFn* fn = ALLOCATE_OBJECT(vm, ObjFn, OBJ_FN);
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


bool ObjFn_write(ObjFn* fn, uint8_t byte, size_t line_number, PyroVM* vm) {
    if (fn->code_count == fn->code_capacity) {
        size_t new_capacity = GROW_CAPACITY(fn->code_capacity);
        uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity, new_capacity);
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
        uint16_t* new_array = REALLOCATE_ARRAY(vm, uint16_t, fn->bpl, old_capacity, new_capacity);
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


size_t ObjFn_get_line_number(ObjFn* fn, size_t ip) {
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


int64_t ObjFn_add_constant(ObjFn* fn, Value value, PyroVM* vm) {
    for (size_t i = 0; i < fn->constants_count; i++) {
        if (pyro_compare_eq_strict(value, fn->constants[i])) {
            return i;
        }
    }

    if (fn->constants_count == fn->constants_capacity) {
        size_t new_capacity = GROW_CAPACITY(fn->constants_capacity);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, fn->constants, fn->constants_capacity, new_capacity);
        if (!new_array) {
            return -1;
        }
        fn->constants_capacity = new_capacity;
        fn->constants = new_array;
    }

    fn->constants[fn->constants_count++] = value;
    return fn->constants_count - 1;
}


size_t ObjFn_opcode_argcount(ObjFn* fn, size_t ip) {
    switch (fn->code[ip]) {
        case OP_ASSERT:
        case OP_BINARY_AMP:
        case OP_BINARY_BANG_EQUAL:
        case OP_BINARY_BAR:
        case OP_BINARY_CARET:
        case OP_BINARY_EQUAL_EQUAL:
        case OP_BINARY_GREATER:
        case OP_BINARY_GREATER_EQUAL:
        case OP_BINARY_GREATER_GREATER:
        case OP_BINARY_IN:
        case OP_BINARY_LESS:
        case OP_BINARY_LESS_EQUAL:
        case OP_BINARY_LESS_LESS:
        case OP_BINARY_MINUS:
        case OP_BINARY_PERCENT:
        case OP_BINARY_PLUS:
        case OP_BINARY_SLASH:
        case OP_BINARY_SLASH_SLASH:
        case OP_BINARY_STAR:
        case OP_BINARY_STAR_STAR:
        case OP_CLOSE_UPVALUE:
        case OP_DUP:
        case OP_DUP_2:
        case OP_GET_INDEX:
        case OP_GET_ITERATOR_NEXT_VALUE:
        case OP_GET_ITERATOR_OBJECT:
        case OP_INHERIT:
        case OP_LOAD_FALSE:
        case OP_LOAD_I64_0:
        case OP_LOAD_I64_1:
        case OP_LOAD_I64_2:
        case OP_LOAD_I64_3:
        case OP_LOAD_I64_4:
        case OP_LOAD_I64_5:
        case OP_LOAD_I64_6:
        case OP_LOAD_I64_7:
        case OP_LOAD_I64_8:
        case OP_LOAD_I64_9:
        case OP_LOAD_NULL:
        case OP_LOAD_TRUE:
        case OP_POP:
        case OP_POP_ECHO_IN_REPL:
        case OP_RETURN:
        case OP_SET_INDEX:
        case OP_TRY:
        case OP_UNARY_BANG:
        case OP_UNARY_MINUS:
        case OP_UNARY_PLUS:
        case OP_UNARY_TILDE:
            return 0;

        case OP_CALL:
        case OP_CALL_UNPACK_LAST_ARG:
        case OP_ECHO:
        case OP_GET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_IMPORT_ALL_MEMBERS:
        case OP_IMPORT_MODULE:
        case OP_SET_LOCAL:
        case OP_SET_UPVALUE:
        case OP_UNPACK:
            return 1;

        case OP_BREAK:
        case OP_DEFINE_FIELD:
        case OP_DEFINE_GLOBAL:
        case OP_DEFINE_METHOD:
        case OP_GET_FIELD:
        case OP_GET_GLOBAL:
        case OP_GET_MEMBER:
        case OP_GET_METHOD:
        case OP_GET_SUPER_METHOD:
        case OP_IMPORT_NAMED_MEMBERS:
        case OP_JUMP:
        case OP_JUMP_BACK:
        case OP_JUMP_IF_ERR:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_NOT_ERR:
        case OP_JUMP_IF_NOT_NULL:
        case OP_JUMP_IF_TRUE:
        case OP_LOAD_CONSTANT:
        case OP_MAKE_CLASS:
        case OP_MAKE_MAP:
        case OP_MAKE_VEC:
        case OP_POP_JUMP_IF_FALSE:
        case OP_SET_FIELD:
        case OP_SET_GLOBAL:
            return 2;

        case OP_CALL_METHOD:
        case OP_CALL_METHOD_UNPACK_LAST_ARG:
        case OP_CALL_SUPER_METHOD:
        case OP_CALL_SUPER_METHOD_UNPACK_LAST_ARG:
            return 3;

        // 2 bytes for the constant index, plus two for each upvalue.
        case OP_MAKE_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ObjFn* closure_fn = AS_FN(fn->constants[const_index]);
            return 2 + closure_fn->upvalue_count * 2;
        }

        // 1 byte for the count, plus two for each constant index.
        case OP_DEFINE_GLOBALS: {
            uint8_t count = fn->code[ip + 1];
            return 1 + count * 2;
        }

        default:
            assert(false);
            fprintf(stderr, "Unhandled opcode in ObjFn_opcode_argcount().");
            exit(1);
    }
}


/* ---------------- */
/* Native Functions */
/* ---------------- */


ObjNativeFn* ObjNativeFn_new(PyroVM* vm, pyro_native_fn_t fn_ptr, const char* name, int arity) {
    ObjStr* name_string = STR(name);
    if (!name_string) {
        return NULL;
    }

    ObjNativeFn* func = ALLOCATE_OBJECT(vm, ObjNativeFn, OBJ_NATIVE_FN);
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


ObjBoundMethod* ObjBoundMethod_new(PyroVM* vm, Value receiver, Obj* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJECT(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
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


ObjModule* ObjModule_new(PyroVM* vm) {
    ObjModule* module = ALLOCATE_OBJECT(vm, ObjModule, OBJ_MODULE);
    if (!module) {
        return NULL;
    }

    module->obj.class = vm->class_module;
    module->globals = NULL;
    module->submodules = NULL;

    module->globals = ObjMap_new(vm);
    if (!module->globals) {
        return NULL;
    }

    module->submodules = ObjMap_new(vm);
    if (!module->submodules) {
        return NULL;
    }

    return module;
}


/* ------------------ */
/*  Vectors & Stacks  */
/* ------------------ */


ObjVec* ObjVec_new(PyroVM* vm) {
    ObjVec* vec = ALLOCATE_OBJECT(vm, ObjVec, OBJ_VEC);
    if (!vec) {
        return NULL;
    }
    vec->count = 0;
    vec->capacity = 0;
    vec->values = NULL;
    vec->obj.class = vm->class_vec;
    return vec;
}


ObjVec* ObjVec_new_as_stack(PyroVM* vm) {
    ObjVec* stack = ObjVec_new(vm);
    if (!stack) {
        return NULL;
    }
    stack->obj.type = OBJ_VEC_AS_STACK;
    stack->obj.class = vm->class_stack;
    return stack;
}


ObjVec* ObjVec_new_with_cap(size_t capacity, PyroVM* vm) {
    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        return NULL;
    }

    if (capacity == 0) {
        return vec;
    }

    Value* value_array = ALLOCATE_ARRAY(vm, Value, capacity);
    if (!value_array) {
        return NULL;
    }

    vec->capacity = capacity;
    vec->values = value_array;

    return vec;
}


ObjVec* ObjVec_new_with_cap_and_fill(size_t capacity, Value fill_value, PyroVM* vm) {
    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        return NULL;
    }

    if (capacity == 0) {
        return vec;
    }

    Value* value_array = ALLOCATE_ARRAY(vm, Value, capacity);
    if (!value_array) {
        return NULL;
    }

    vec->capacity = capacity;
    vec->count = capacity;
    vec->values = value_array;

    for (size_t i = 0; i < capacity; i++) {
        vec->values[i] = fill_value;
    }

    return vec;
}


ObjVec* ObjVec_copy(ObjVec* src, PyroVM* vm) {
    ObjVec* vec = ObjVec_new_with_cap(src->count, vm);
    if (!vec) {
        return NULL;
    }
    memcpy(vec->values, src->values, sizeof(Value) * src->count);
    return vec;
}


bool ObjVec_append(ObjVec* vec, Value value, PyroVM* vm) {
    if (vec->count == vec->capacity) {
        size_t new_capacity = GROW_CAPACITY(vec->capacity);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, vec->values, vec->capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        vec->capacity = new_capacity;
        vec->values = new_array;
    }
    vec->values[vec->count++] = value;
    return true;
}


bool ObjVec_copy_entries(ObjVec* src, ObjVec* dst, PyroVM* vm) {
    for (size_t i = 0; i < src->count; i++) {
        if (!ObjVec_append(dst, src->values[i], vm)) {
            return false;
        }
    }
    return true;
}


Value ObjVec_remove_last(ObjVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, "cannot remove last item from empty vector");
        return MAKE_NULL();
    }
    vec->count--;
    return vec->values[vec->count];
}


Value ObjVec_remove_first(ObjVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, "cannot remove first item from empty vector");
        return MAKE_NULL();
    }

    if (vec->count == 1) {
        vec->count--;
        return vec->values[0];
    }

    Value output = vec->values[0];

    vec->count--;
    memmove(vec->values, &vec->values[1], sizeof(Value) * vec->count);

    return output;
}


Value ObjVec_remove_at_index(ObjVec* vec, size_t index, PyroVM* vm) {
    if (index >= vec->count) {
        pyro_panic(vm, "index is out of range");
        return MAKE_NULL();
    }

    if (vec->count == 1) {
        vec->count--;
        return vec->values[0];
    }

    if (index == vec->count - 1) {
        vec->count--;
        return vec->values[vec->count];
    }

    Value output = vec->values[index];

    size_t bytes_to_move = sizeof(Value) * (vec->count - index - 1);
    memmove(&vec->values[index], &vec->values[index + 1], bytes_to_move);
    vec->count--;

    return output;
}


void ObjVec_insert_at_index(ObjVec* vec, size_t index, Value value, PyroVM* vm) {
    if (index > vec->count) {
        pyro_panic(vm, "index is out of range");
        return;
    }

    if (index == vec->count) {
        if (!ObjVec_append(vec, value, vm)) {
            pyro_panic(vm, "out of memory");
        }
        return;
    }

    if (vec->count == vec->capacity) {
        size_t new_capacity = GROW_CAPACITY(vec->capacity);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, vec->values, vec->capacity, new_capacity);
        if (!new_array) {
            pyro_panic(vm, "out of memory");
            return;
        }
        vec->capacity = new_capacity;
        vec->values = new_array;
    }

    size_t bytes_to_move = sizeof(Value) * (vec->count - index);
    memmove(&vec->values[index + 1], &vec->values[index], bytes_to_move);

    vec->values[index] = value;
    vec->count++;
}


/* ------- */
/* Buffers */
/* ------- */


ObjBuf* ObjBuf_new(PyroVM* vm) {
    ObjBuf* buf = ALLOCATE_OBJECT(vm, ObjBuf, OBJ_BUF);
    if (!buf) {
        return NULL;
    }
    buf->count = 0;
    buf->capacity = 0;
    buf->bytes = NULL;
    buf->obj.class = vm->class_buf;
    return buf;
}


ObjBuf* ObjBuf_new_with_cap(size_t capacity, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }

    if (capacity == 0) {
        return buf;
    }

    uint8_t* new_array = ALLOCATE_ARRAY(vm, uint8_t, capacity + 1);
    if (!new_array) {
        return NULL;
    }

    buf->bytes = new_array;
    buf->capacity = capacity + 1;

    return buf;
}


ObjBuf* ObjBuf_new_with_cap_and_fill(size_t capacity, uint8_t fill_value, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }

    if (capacity == 0) {
        return buf;
    }

    uint8_t* new_array = ALLOCATE_ARRAY(vm, uint8_t, capacity + 1);
    if (!new_array) {
        return NULL;
    }

    buf->bytes = new_array;
    buf->capacity = capacity + 1;
    buf->count = capacity;

    for (size_t i = 0; i < capacity; i++) {
        buf->bytes[i] = fill_value;
    }

    return buf;
}


ObjBuf* ObjBuf_new_from_string(ObjStr* string, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }

    size_t capacity = string->length + 1;

    uint8_t* new_array = ALLOCATE_ARRAY(vm, uint8_t, capacity);
    if (!new_array) {
        return NULL;
    }

    buf->bytes = new_array;
    buf->capacity = capacity;
    buf->count = string->length;

    memcpy(buf->bytes, string->bytes, string->length);
    return buf;
}


bool ObjBuf_append_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm) {
    return ObjBuf_append_bytes(buf, 1, &byte, vm);
}


bool ObjBuf_append_hex_escaped_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm) {
    if (!ObjBuf_append_bytes(buf, 4, (uint8_t*)"\\x##", vm)) {
        return false;
    }

    char str[3];
    sprintf(str, "%02X", byte);

    buf->bytes[buf->count - 2] = str[0];
    buf->bytes[buf->count - 1] = str[1];

    return true;
}


// Attempts to grow the buffer to at least the required capacity. Returns true on success, false
// if memory allocation failed. In this case the buffer is unchanged.
bool ObjBuf_grow(ObjBuf* buf, size_t required_capacity, PyroVM* vm) {
    if (required_capacity > buf->capacity) {
        size_t new_capacity = GROW_CAPACITY(buf->capacity);
        while (new_capacity < required_capacity) {
            new_capacity = GROW_CAPACITY(new_capacity);
        }
        uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        buf->capacity = new_capacity;
        buf->bytes = new_array;
    }
    return true;
}


// We make sure there's always at least one spare byte of capacity -- this means that we can
// efficiently convert the buffer's underlying byte array to a string without needing to allocate
// extra memory for the terminating \0.
bool ObjBuf_append_bytes(ObjBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm) {
    size_t required_capacity = buf->count + count + 1;

    if (required_capacity > buf->capacity) {
        if (!ObjBuf_grow(buf, required_capacity, vm)) {
            return false;
        }
    }

    memcpy(&buf->bytes[buf->count], bytes, count);
    buf->count += count;

    return true;
}


// This function converts the contents of the buffer into a string, leaving a valid but empty
// buffer behind. Returns NULL if memory cannot be allocated for the new string object -- in this
// case the buffer is unchanged.
ObjStr* ObjBuf_to_str(ObjBuf* buf, PyroVM* vm) {
    if (buf->count == 0) {
        return vm->empty_string;
    }

    if (buf->capacity > buf->count + 1) {
        buf->bytes = REALLOCATE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity, buf->count + 1);
        buf->capacity = buf->count + 1;
    }
    buf->bytes[buf->count] = '\0';

    ObjStr* string = ObjStr_take((char*)buf->bytes, buf->count, vm);
    if (!string) {
        return NULL;
    }

    buf->count = 0;
    buf->capacity = 0;
    buf->bytes = NULL;

    return string;
}


int64_t ObjBuf_write_f(ObjBuf* buf, PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = ObjBuf_write_fv(buf, vm, format_string, args);
    va_end(args);
    return result;
}


int64_t ObjBuf_write_fv(ObjBuf* buf, PyroVM* vm, const char* format_string, va_list args) {
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

    if (required_capacity <= buf->capacity || ObjBuf_grow(buf, required_capacity, vm)) {
        vsprintf((char*)&buf->bytes[buf->count], format_string, args);
        buf->count += length;
        return length;
    }

    return -1;
}


bool ObjBuf_best_effort_write_fv(ObjBuf* buf, PyroVM* vm, const char* format_string, va_list args) {
    // Determine the length of the string. (Doesn't include the terminating null.)
    // A negative length indicates a formatting error.
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format_string, args_copy);
    va_end(args_copy);

    if (length == 0) {
        return true;
    }

    if (length < 0) {
        return false;
    }

    size_t required_capacity = buf->count + (size_t)length + 1;

    if (required_capacity <= buf->capacity || ObjBuf_grow(buf, required_capacity, vm)) {
        vsprintf((char*)&buf->bytes[buf->count], format_string, args);
        buf->count += length;
        return true;
    }

    size_t available_capacity = buf->capacity - buf->count;
    vsnprintf((char*)&buf->bytes[buf->count], available_capacity, format_string, args);
    return false;
}


/* ----- */
/* Files */
/* ----- */


ObjFile* ObjFile_new(PyroVM* vm, FILE* stream) {
    ObjFile* file = ALLOCATE_OBJECT(vm, ObjFile, OBJ_FILE);
    if (!file) {
        return NULL;
    }
    file->stream = stream;
    file->obj.class = vm->class_file;
    return file;
}


ObjStr* ObjFile_read_line(ObjFile* file, PyroVM* vm) {
    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, "out of memory");
                return NULL;
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, "I/O read error");
                return NULL;
            }
            break;
        }

        array[count++] = c;

        if (c == '\n') {
            break;
        }
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL;
    }

    if (count > 1 && array[count - 2] == '\r' && array[count - 1] == '\n') {
        count -= 2;
    } else if (array[count - 1] == '\n') {
        count -= 1;
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return vm->empty_string;
    }

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }

    array[count] = '\0';

    ObjStr* string = ObjStr_take((char*)array, count, vm);
    if (!string) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return string;
}


/* --------- */
/* Iterators */
/* --------- */


ObjIter* ObjIter_new(Obj* source, IterType iter_type, PyroVM* vm) {
    ObjIter* iter = ALLOCATE_OBJECT(vm, ObjIter, OBJ_ITER);
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


Value ObjIter_next(ObjIter* iter, PyroVM* vm) {
    switch (iter->iter_type) {
        case ITER_VEC: {
            ObjVec* vec = (ObjVec*)iter->source;
            if (iter->next_index < vec->count) {
                iter->next_index++;
                Value result = vec->values[iter->next_index - 1];
                return result;
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_TUP: {
            ObjTup* tup = (ObjTup*)iter->source;
            if (iter->next_index < tup->count) {
                iter->next_index++;
                return tup->values[iter->next_index - 1];
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_QUEUE: {
            if (iter->next_queue_item) {
                Value next_value = iter->next_queue_item->value;
                iter->next_queue_item = iter->next_queue_item->next;
                return next_value;
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_STR: {
            ObjStr* str = (ObjStr*)iter->source;
            if (iter->next_index < str->length) {
                ObjStr* new_str = ObjStr_copy_raw(&str->bytes[iter->next_index], 1, vm);
                if (!new_str) {
                    pyro_panic(vm, "out of memory");
                    return MAKE_OBJ(vm->empty_error);
                }
                iter->next_index++;
                return MAKE_OBJ(new_str);
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_STR_BYTES: {
            ObjStr* str = (ObjStr*)iter->source;
            if (iter->next_index < str->length) {
                int64_t byte_value = (uint8_t)str->bytes[iter->next_index];
                iter->next_index++;
                return MAKE_I64(byte_value);
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_STR_CHARS: {
            ObjStr* str = (ObjStr*)iter->source;
            if (iter->next_index < str->length) {
                uint8_t* src = (uint8_t*)&str->bytes[iter->next_index];
                size_t src_len = str->length - iter->next_index;
                Utf8CodePoint cp;
                if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
                    iter->next_index += cp.length;
                    return MAKE_CHAR(cp.value);
                }
                pyro_panic(
                    vm,
                    "String contains invalid utf-8 at byte index %zu",
                    iter->next_index
                );
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_MAP_KEYS: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                MapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->key;
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_MAP_VALUES: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                MapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->value;
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_MAP_ENTRIES: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->entry_array_count) {
                MapEntry* entry = &map->entry_array[iter->next_index];
                iter->next_index++;
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }

                ObjTup* tup = ObjTup_new(2, vm);
                if (!tup) {
                    pyro_panic(vm, "out of memory");
                    return MAKE_OBJ(vm->empty_error);
                }

                tup->values[0] = entry->key;
                tup->values[1] = entry->value;
                return MAKE_OBJ(tup);
            }
            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_FUNC_MAP: {
            ObjIter* src_iter = (ObjIter*)iter->source;
            Value next_value = ObjIter_next(src_iter, vm);
            if (IS_ERR(next_value)) {
                return next_value;
            }

            pyro_push(vm, MAKE_OBJ(iter->callback));
            pyro_push(vm, next_value);
            Value result = pyro_call_function(vm, 1);
            if (vm->halt_flag) {
                return MAKE_OBJ(vm->empty_error);
            }

            return result;
        }

        case ITER_FUNC_FILTER: {
            ObjIter* src_iter = (ObjIter*)iter->source;

            while (true) {
                Value next_value = ObjIter_next(src_iter, vm);
                if (IS_ERR(next_value)) {
                    return next_value;
                }

                pyro_push(vm, MAKE_OBJ(iter->callback));
                pyro_push(vm, next_value);
                Value result = pyro_call_function(vm, 1);
                if (vm->halt_flag) {
                    return MAKE_OBJ(vm->empty_error);
                }

                if (pyro_is_truthy(result)) {
                    return next_value;
                }
            }
        }

        case ITER_ENUM: {
            ObjIter* src_iter = (ObjIter*)iter->source;
            Value next_value = ObjIter_next(src_iter, vm);
            if (IS_ERR(next_value)) {
                return next_value;
            }

            ObjTup* tup = ObjTup_new(2, vm);
            if (!tup) {
                pyro_panic(vm, "out of memory");
                return MAKE_OBJ(vm->empty_error);
            }

            tup->values[0] = MAKE_I64(iter->next_enum);
            tup->values[1] = next_value;
            iter->next_enum++;

            return MAKE_OBJ(tup);
        }

        case ITER_GENERIC: {
            Value next_method = pyro_get_method(vm, MAKE_OBJ(iter->source), vm->str_dollar_next);
            pyro_push(vm, MAKE_OBJ(iter->source));
            Value result = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return MAKE_OBJ(vm->empty_error);
            }
            return result;
        }

        case ITER_RANGE: {
            if (iter->range_step > 0) {
                if (iter->range_next < iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return MAKE_I64(range_next);
                }
                return MAKE_OBJ(vm->empty_error);
            }

            if (iter->range_step < 0) {
                if (iter->range_next > iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return MAKE_I64(range_next);
                }
                return MAKE_OBJ(vm->empty_error);
            }

            return MAKE_OBJ(vm->empty_error);
        }

        case ITER_STR_LINES: {
            // If the source is NULL, the iterator has been exhausted.
            if (iter->source == NULL) {
                return MAKE_OBJ(vm->empty_error);
            }
            ObjStr* str = (ObjStr*)iter->source;

            // If we're at the end of the string, set the source to NULL and return an empty string.
            if (iter->next_index == str->length) {
                iter->source = NULL;
                return MAKE_OBJ(vm->empty_string);
            }

            // Points to the byte *after* the last byte in the string.
            const char* const string_end = str->bytes + str->length;

            // Points to the first byte of the next line.
            const char* const line_start = str->bytes + iter->next_index;

            // Once we've identified the end of the next line, this will point to the byte *after*
            // the last byte of the line, i.e. line_length = line_end - line_start.
            const char* line_end = line_start;

            // Find the end of the next line.
            while (line_end < string_end) {
                if (string_end - line_end > 1 && line_end[0] == '\r' && line_end[1] == '\n') {
                    iter->next_index = line_end - str->bytes + 2;
                    break;
                } else if (*line_end == '\n' || *line_end == '\r') {
                    iter->next_index = line_end - str->bytes + 1;
                    break;
                } else {
                    line_end++;
                }
            }

            ObjStr* next_line = ObjStr_copy_raw(line_start, line_end - line_start, vm);
            if (!next_line) {
                pyro_panic(vm, "out of memory");
                return MAKE_OBJ(vm->empty_error);
            }

            // If the string ends with a linebreak, we have one more (empty) string to return. This
            // checks for the case where the strings ends without a linebreak -- in this case the
            // returned string exhausts the iterator.
            if (line_end == string_end && next_line->length > 0) {
                iter->source = NULL;
            }

            return MAKE_OBJ(next_line);
        }

        case ITER_FILE_LINES: {
            // If the source is NULL, the iterator has been exhausted.
            if (iter->source == NULL) {
                return MAKE_OBJ(vm->empty_error);
            }

            ObjFile* file = (ObjFile*)iter->source;
            ObjStr* next_line = ObjFile_read_line(file, vm);
            if (vm->halt_flag) {
                return MAKE_OBJ(vm->empty_error);
            }

            // A NULL return value means the file has been exhausted.
            if (!next_line) {
                iter->source = NULL;
                return MAKE_OBJ(vm->empty_error);
            }

            return MAKE_OBJ(next_line);
        }

        default:
            assert(false);
            return MAKE_OBJ(vm->empty_error);
    }
}


ObjStr* ObjIter_join(ObjIter* iter, const char* sep, size_t sep_length, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, MAKE_OBJ(buf)); // Protect from GC in case we call into Pyro code.

    bool is_first_item = true;

    while (true) {
        Value next_value = ObjIter_next(iter, vm);
        if (vm->halt_flag) {
            return NULL;
        }
        if (IS_ERR(next_value)) {
            break;
        }

        if (!is_first_item) {
            if (!ObjBuf_append_bytes(buf, sep_length, (uint8_t*)sep, vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }

        pyro_push(vm, next_value); // Stringification can call into Pyro code and trigger the GC.
        ObjStr* value_string = pyro_stringify_value(vm, next_value);
        if (vm->halt_flag) {
            return NULL;
        }
        pyro_pop(vm); // next_value

        if (!ObjBuf_append_bytes(buf, value_string->length, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        is_first_item = false;
    }

    ObjStr* output_string = ObjBuf_to_str(buf, vm);
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


ObjQueue* ObjQueue_new(PyroVM* vm) {
    ObjQueue* queue = ALLOCATE_OBJECT(vm, ObjQueue, OBJ_QUEUE);
    if (!queue) {
        return NULL;
    }
    queue->obj.class = vm->class_queue;
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    return queue;
}


// Add the new item to the end of the linked list.
bool ObjQueue_enqueue(ObjQueue* queue, Value value, PyroVM* vm) {
    QueueItem* item = pyro_realloc(vm, NULL, 0, sizeof(QueueItem));
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
bool ObjQueue_dequeue(ObjQueue* queue, Value* value, PyroVM* vm) {
    if (queue->count == 0) {
        return false;
    }

    QueueItem* item = queue->head;
    *value = item->value;

    if (queue->count == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = item->next;
    }

    pyro_realloc(vm, item, sizeof(QueueItem), 0);
    queue->count--;
    return true;
}


/* ------------------- */
/*  Resource Pointers  */
/* ------------------- */


ObjResourcePointer* ObjResourcePointer_new(void* pointer, pyro_free_rp_callback_t callback, PyroVM* vm) {
    ObjResourcePointer* resource = ALLOCATE_OBJECT(vm, ObjResourcePointer, OBJ_RESOURCE_POINTER);
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


ObjErr* ObjErr_new(PyroVM* vm) {
    ObjErr* err = ALLOCATE_OBJECT(vm, ObjErr, OBJ_ERR);
    if (!err) {
        return NULL;
    }

    err->obj.class = vm->class_err;
    err->message = vm->empty_string;

    err->details = ObjMap_new(vm);
    if (!err->details) {
        return NULL;
    }

    return err;
}
