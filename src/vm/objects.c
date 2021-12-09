#include "values.h"
#include "objects.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "opcodes.h"
#include "errors.h"
#include "utf8.h"


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
            pyro_stringify_obj_type(type)
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

    tup->obj.class = vm->tup_class;
    tup->count = count;
    for (size_t i = 0; i < count; i++) {
        tup->values[i] = NULL_VAL();
    }
    return tup;
}


ObjTup* ObjTup_new_err(size_t count, PyroVM* vm) {
    ObjTup* tup = ObjTup_new(count, vm);
    if (!tup) {
        return NULL;
    }
    tup->obj.type = OBJ_TUP_AS_ERR;
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
            if (!pyro_compare_eq(vm, a->values[i], b->values[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}


ObjStr* ObjTup_stringify(ObjTup* tup, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '(', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < tup->count; i++) {
        Value item = tup->values[i];
        ObjStr* item_string;

        if (IS_STR(item)) {
            item_string = ObjStr_debug_str(AS_STR(item), vm);
        } else if (IS_CHAR(item)) {
            item_string = pyro_char_to_debug_str(vm, item);
        } else {
            item_string = pyro_stringify_value(vm, item);
        }

        if (vm->halt_flag || !item_string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(item_string));
        if (!ObjBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);

        if (i + 1 < tup->count) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_pop(vm);
                return NULL;
            }
        }
    }

    if (!ObjBuf_append_byte(buf, ')', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
}


/* -------- */
/* Closures */
/* -------- */


ObjClosure* ObjClosure_new(PyroVM* vm, ObjFn* fn, ObjModule* module) {
    ObjClosure* closure = ALLOCATE_OBJECT(vm, ObjClosure, OBJ_CLOSURE);
    if (closure == NULL) {
        return NULL;
    }

    closure->fn = fn;
    closure->module = module;
    closure->upvalues = NULL;
    closure->upvalue_count = 0;

    if (fn->upvalue_count > 0) {
        pyro_push(vm, OBJ_VAL(closure));
        closure->upvalues = ALLOCATE_ARRAY(vm, ObjUpvalue*, fn->upvalue_count);
        pyro_pop(vm);

        if (closure->upvalues == NULL) {
            return NULL;
        }

        closure->upvalue_count = fn->upvalue_count;
        for (size_t i = 0; i < fn->upvalue_count; i++) {
            closure->upvalues[i] = NULL;
        }
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
    upvalue->closed = NULL_VAL();
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
    class->field_initializers = NULL;
    class->field_indexes = NULL;
    class->superclass = NULL;

    pyro_push(vm, OBJ_VAL(class));
    class->methods = ObjMap_new(vm);
    class->field_initializers = ObjVec_new(vm);
    class->field_indexes = ObjMap_new(vm);
    pyro_pop(vm);

    if (!class->methods || !class->field_initializers || !class->field_indexes) {
        return NULL;
    }

    return class;
}


/* --------- */
/* Instances */
/* --------- */


ObjInstance* ObjInstance_new(PyroVM* vm, ObjClass* class) {
    size_t num_fields = class->field_initializers->count;

    ObjInstance* instance = ALLOCATE_FLEX_OBJECT(vm, ObjInstance, OBJ_INSTANCE, num_fields, Value);
    if (!instance) {
        return NULL;
    }

    instance->obj.class = class;
    if (num_fields > 0) {
        memcpy(instance->fields, class->field_initializers->values, sizeof(Value) * num_fields);
    }

    return instance;
}


/* ------ */
/*  Maps  */
/* ------ */


static MapEntry* find_entry(PyroVM* vm, MapEntry* entries, size_t capacity, Value key) {
    // Capacity is always a power of 2 so we can use bitwise-AND as a fast
    // modulo operator, i.e. this is equivalent to: index = key_hash % capacity.
    size_t index = (size_t)pyro_hash_value(vm, key) & (capacity - 1);
    MapEntry* tombstone = NULL;

    for (;;) {
        MapEntry* entry = &entries[index];

        if (IS_EMPTY(entry->key)) {
            return tombstone == NULL ? entry : tombstone;
        } else if (IS_TOMBSTONE(entry->key)) {
            if (tombstone == NULL) tombstone = entry;
        } else if (pyro_compare_eq(vm, key, entry->key)) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}


static ObjStr* find_string(ObjMap* map, const char* string, size_t length, uint64_t hash) {
    if (map->count == 0) {
        return NULL;
    }

    size_t index = (size_t)hash & (map->capacity - 1);

    for (;;) {
        MapEntry* entry = &map->entries[index];

        if (IS_EMPTY(entry->key)) {
            return NULL;
        }

        if (IS_STR(entry->key)) {
            if (AS_STR(entry->key)->length == length) {
                if (AS_STR(entry->key)->hash == hash) {
                    if (memcmp(AS_STR(entry->key)->bytes, string, length) == 0) {
                        return AS_STR(entry->key);
                    }
                }
            }
        }

        index = (index + 1) & (map->capacity - 1);
    }
}


static bool resize_map(ObjMap* map, size_t new_capacity, PyroVM* vm) {
    MapEntry* new_entries = ALLOCATE_ARRAY(vm, MapEntry, new_capacity);
    if (!new_entries) {
        return false;
    }

    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = EMPTY_VAL();
    }

    size_t new_count = 0;
    for (size_t i = 0; i < map->capacity; i++) {
        MapEntry* src = &map->entries[i];
        if (IS_EMPTY(src->key) || IS_TOMBSTONE(src->key)) {
            continue;
        }
        MapEntry* dst = find_entry(vm, new_entries, new_capacity, src->key);
        dst->key = src->key;
        dst->value = src->value;
        new_count++;
    }

    FREE_ARRAY(vm, MapEntry, map->entries, map->capacity);
    map->entries = new_entries;
    map->capacity = new_capacity;
    map->count = new_count;
    map->tombstone_count = 0;
    map->max_load_threshold = new_capacity * PYRO_MAX_HASHMAP_LOAD;
    return true;
}


ObjMap* ObjMap_new(PyroVM* vm) {
    ObjMap* map = ALLOCATE_OBJECT(vm, ObjMap, OBJ_MAP);
    if (!map) {
        return NULL;
    }

    map->count = 0;
    map->capacity = 0;
    map->tombstone_count = 0;
    map->entries = NULL;
    map->max_load_threshold = 0;
    map->obj.class = vm->map_class;
    return map;
}


ObjMap* ObjMap_new_as_set(PyroVM* vm) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = OBJ_MAP_AS_SET;
    map->obj.class = vm->set_class;
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

    pyro_push(vm, OBJ_VAL(dst));
    MapEntry* array = ALLOCATE_ARRAY(vm, MapEntry, src->capacity);
    pyro_pop(vm);
    if (!array) {
        return NULL;
    }

    memcpy(array, src->entries, sizeof(MapEntry) * src->capacity);

    dst->count = src->count;
    dst->capacity = src->capacity;
    dst->tombstone_count = src->tombstone_count;
    dst->entries = array;
    dst->max_load_threshold = src->max_load_threshold;

    return dst;
}


int ObjMap_set(ObjMap* map, Value key, Value value, PyroVM* vm) {
    if (map->capacity == 0) {
        size_t new_capacity = GROW_CAPACITY(map->capacity);
        if (!resize_map(map, new_capacity, vm)) {
            return 0;
        }
    }

    MapEntry* entry = find_entry(vm, map->entries, map->capacity, key);

    if (IS_EMPTY(entry->key)) {
        if (map->count == map->max_load_threshold) {
            size_t new_capacity = GROW_CAPACITY(map->capacity);
            if (!resize_map(map, new_capacity, vm)) {
                return 0;
            }
            entry = find_entry(vm, map->entries, map->capacity, key);
        }
        entry->key = key;
        entry->value = value;
        map->count++;
        return 1;
    }

    if (IS_TOMBSTONE(entry->key)) {
        entry->key = key;
        entry->value = value;
        map->tombstone_count--;
        return 1;
    }

    entry->key = key;
    entry->value = value;
    return 2;
}


bool ObjMap_update_entry(ObjMap* map, Value key, Value value, PyroVM* vm) {
    if (map->count == 0) {
        return false;
    }

    MapEntry* entry = find_entry(vm, map->entries, map->capacity, key);
    if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
        return false;
    }

    entry->key = key;
    entry->value = value;
    return true;
}


bool ObjMap_get(ObjMap* map, Value key, Value* value, PyroVM* vm) {
    if (map->count == 0) {
        return false;
    }

    MapEntry* entry = find_entry(vm, map->entries, map->capacity, key);
    if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
        return false;
    }

    *value = entry->value;
    return true;
}


bool ObjMap_remove(ObjMap* map, Value key, PyroVM* vm) {
    if (map->count == 0) {
        return false;
    }

    MapEntry* entry = find_entry(vm, map->entries, map->capacity, key);
    if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
        return false;
    }

    entry->key = TOMBSTONE_VAL();
    map->tombstone_count++;
    return true;
}


bool ObjMap_copy_entries(ObjMap* src, ObjMap* dst, PyroVM* vm) {
    for (size_t i = 0; i < src->capacity; i++) {
        MapEntry* entry = &src->entries[i];
        if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
            continue;
        }
        if (ObjMap_set(dst, entry->key, entry->value, vm) == 0) {
            return false;
        }
    }
    return true;
}


ObjStr* ObjMap_stringify(ObjMap* map, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '{', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    bool is_first_entry = true;

    for (size_t i = 0; i < map->capacity; i++) {
        MapEntry* entry = &map->entries[i];

        if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!is_first_entry) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_pop(vm);
                return NULL;
            }
        }
        is_first_entry = false;

        ObjStr* key_string;

        if (IS_STR(entry->key)) {
            key_string = ObjStr_debug_str(AS_STR(entry->key), vm);
        } else if (IS_CHAR(entry->key)) {
            key_string = pyro_char_to_debug_str(vm, entry->key);
        } else {
            key_string = pyro_stringify_value(vm, entry->key);
        }

        if (vm->halt_flag || !key_string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(key_string));
        if (!ObjBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);

        if (!ObjBuf_append_bytes(buf, 3, (uint8_t*)" = ", vm)) {
            pyro_pop(vm);
            return NULL;
        }

        ObjStr* value_string;

        if (IS_STR(entry->value)) {
            value_string = ObjStr_debug_str(AS_STR(entry->value), vm);
        } else if (IS_CHAR(entry->value)) {
            value_string = pyro_char_to_debug_str(vm, entry->value);
        } else {
            value_string = pyro_stringify_value(vm, entry->value);
        }

        if (vm->halt_flag || !value_string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(value_string));
        if (!ObjBuf_append_bytes(buf, value_string->length, (uint8_t*)value_string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);
    }

    if (!ObjBuf_append_byte(buf, '}', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
}


ObjStr* ObjMap_stringify_as_set(ObjMap* map, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '{', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    bool is_first_entry = true;

    for (size_t i = 0; i < map->capacity; i++) {
        MapEntry* entry = &map->entries[i];

        if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!is_first_entry) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_pop(vm);
                return NULL;
            }
        }
        is_first_entry = false;

        ObjStr* key_string;

        if (IS_STR(entry->key)) {
            key_string = ObjStr_debug_str(AS_STR(entry->key), vm);
        } else if (IS_CHAR(entry->key)) {
            key_string = pyro_char_to_debug_str(vm, entry->key);
        } else {
            key_string = pyro_stringify_value(vm, entry->key);
        }

        if (vm->halt_flag || !key_string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(key_string));
        if (!ObjBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);
    }

    if (!ObjBuf_append_byte(buf, '}', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
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
    string->obj.class = vm->str_class;

    pyro_push(vm, OBJ_VAL(string));
    ObjMap_set(vm->strings, OBJ_VAL(string), NULL_VAL(), vm);
    pyro_pop(vm);

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


ObjStr* ObjStr_from_fmt(PyroVM* vm, const char* fmtstr, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [length] will be the output string length,
    // not counting the terminating null, so we'll need to allocate [length + 1] bytes.
    va_start(args, fmtstr);
    int length = vsnprintf(NULL, 0, fmtstr, args);
    va_end(args);

    // If [length] is negative, an encoding error occurred.
    if (length < 0) {
        return NULL;
    }

    char* array = ALLOCATE_ARRAY(vm, char, length + 1);
    if (array == NULL) {
        return NULL;
    }

    va_start(args, fmtstr);
    vsnprintf(array, length + 1, fmtstr, args);
    va_end(args);

    ObjStr* string = ObjStr_take(array, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, array, length + 1);
        return NULL;
    }

    return string;
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


ObjStr* ObjStr_debug_str(ObjStr* str, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '"', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < str->length; i++) {
        bool result;
        char c = str->bytes[i];

        switch (c) {
            case '"':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\"", vm);
                break;
            case '\\':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\\", vm);
                break;
            case '\0':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\0", vm);
                break;
            case '\b':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\b", vm);
                break;
            case '\n':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\n", vm);
                break;
            case '\r':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\r", vm);
                break;
            case '\t':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\t", vm);
                break;
            default:
                if (c < 32 || c == 127) {
                    result = ObjBuf_append_hex_escaped_byte(buf, c, vm);
                } else {
                    result = ObjBuf_append_byte(buf, c, vm);
                }
                break;
        }

        if (!result) {
            pyro_pop(vm);
            return NULL;
        }
    }

    if (!ObjBuf_append_byte(buf, '"', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
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
    fn->upvalue_count = 0;
    fn->name = NULL;
    fn->source = NULL;

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
        pyro_push(vm, value);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, fn->constants, fn->constants_capacity, new_capacity);
        pyro_pop(vm);

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
        case OP_BINARY_PLUS:
        case OP_CLOSE_UPVALUE:
        case OP_DUP:
        case OP_DUP_2:
        case OP_BINARY_EQUAL_EQUAL:
        case OP_BINARY_SLASH:
        case OP_BINARY_GREATER:
        case OP_BINARY_GREATER_EQUAL:
        case OP_INHERIT:
        case OP_GET_ITERATOR_OBJECT:
        case OP_GET_ITERATOR_NEXT_VALUE:
        case OP_BINARY_LESS:
        case OP_BINARY_LESS_EQUAL:
        case OP_LOAD_FALSE:
        case OP_LOAD_NULL:
        case OP_LOAD_TRUE:
        case OP_BINARY_STAR:
        case OP_UNARY_MINUS:
        case OP_UNARY_BANG:
        case OP_BINARY_BANG_EQUAL:
        case OP_POP:
        case OP_RETURN:
        case OP_BINARY_MINUS:
        case OP_BINARY_SLASH_SLASH:
        case OP_TRY:
        case OP_GET_INDEX:
        case OP_SET_INDEX:
        case OP_BINARY_STAR_STAR:
        case OP_BINARY_PERCENT:
        case OP_UNARY_TILDE:
        case OP_BINARY_CARET:
        case OP_BINARY_AMP:
        case OP_BINARY_BAR:
        case OP_BINARY_LESS_LESS:
        case OP_BINARY_GREATER_GREATER:
        case OP_LOAD_INT_0:
        case OP_LOAD_INT_1:
        case OP_LOAD_INT_2:
        case OP_LOAD_INT_3:
        case OP_LOAD_INT_4:
        case OP_LOAD_INT_5:
        case OP_LOAD_INT_6:
        case OP_LOAD_INT_7:
        case OP_LOAD_INT_8:
        case OP_LOAD_INT_9:
            return 0;

        case OP_CALL:
        case OP_ECHO:
        case OP_GET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_IMPORT_MODULE:
        case OP_SET_LOCAL:
        case OP_SET_UPVALUE:
        case OP_UNPACK:
            return 1;

        case OP_BREAK:
        case OP_MAKE_CLASS:
        case OP_DEFINE_GLOBAL:
        case OP_DEFINE_FIELD:
        case OP_GET_FIELD:
        case OP_GET_GLOBAL:
        case OP_GET_MEMBER:
        case OP_GET_METHOD:
        case OP_GET_SUPER_METHOD:
        case OP_JUMP:
        case OP_JUMP_IF_ERR:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_NOT_ERR:
        case OP_JUMP_IF_NOT_NULL:
        case OP_JUMP_IF_TRUE:
        case OP_LOAD_CONSTANT:
        case OP_LOOP:
        case OP_MAKE_MAP:
        case OP_MAKE_VEC:
        case OP_DEFINE_METHOD:
        case OP_IMPORT_MEMBERS:
        case OP_POP_JUMP_IF_FALSE:
        case OP_SET_FIELD:
        case OP_SET_GLOBAL:
            return 2;

        case OP_INVOKE_METHOD:
        case OP_INVOKE_SUPER_METHOD:
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


ObjNativeFn* ObjNativeFn_new(PyroVM* vm, NativeFn fn_ptr, const char* name, int arity) {
    ObjStr* name_object = STR(name);
    if (!name_object) {
        return NULL;
    }

    pyro_push(vm, OBJ_VAL(name_object));
    ObjNativeFn* func = ALLOCATE_OBJECT(vm, ObjNativeFn, OBJ_NATIVE_FN);
    if (!func) {
        pyro_pop(vm);
        return NULL;
    }
    pyro_pop(vm);

    func->fn_ptr = fn_ptr;
    func->arity = arity;
    func->name = name_object;
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

    module->globals = NULL;
    module->submodules = NULL;

    pyro_push(vm, OBJ_VAL(module));
    module->globals = ObjMap_new(vm);
    module->submodules = ObjMap_new(vm);
    pyro_pop(vm);

    if (!module->globals || !module->submodules) {
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
    vec->obj.class = vm->vec_class;
    return vec;
}


ObjVec* ObjVec_new_as_stack(PyroVM* vm) {
    ObjVec* stack = ObjVec_new(vm);
    if (!stack) {
        return NULL;
    }
    stack->obj.type = OBJ_VEC_AS_STACK;
    stack->obj.class = vm->stack_class;
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

    pyro_push(vm, OBJ_VAL(vec));
    Value* value_array = ALLOCATE_ARRAY(vm, Value, capacity);
    pyro_pop(vm);

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

    pyro_push(vm, OBJ_VAL(vec));
    Value* value_array = ALLOCATE_ARRAY(vm, Value, capacity);
    pyro_pop(vm);

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


ObjStr* ObjVec_stringify(ObjVec* vec, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '[', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < vec->count; i++) {
        Value item = vec->values[i];
        ObjStr* item_string;

        if (IS_STR(item)) {
            item_string = ObjStr_debug_str(AS_STR(item), vm);
        } else if (IS_CHAR(item)) {
            item_string = pyro_char_to_debug_str(vm, item);
        } else {
            item_string = pyro_stringify_value(vm, item);
        }

        if (vm->halt_flag || !item_string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(item_string));
        if (!ObjBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);

        if (i + 1 < vec->count) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_pop(vm);
                return NULL;
            }
        }
    }

    if (!ObjBuf_append_byte(buf, ']', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string = ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
}


// Merges the sorted slices array[low..mid] and array[mid+1..high]. Indices are inclusive.
static void merge(Value* array, Value* aux_array, size_t low, size_t mid, size_t high, Value fn, PyroVM* vm) {
    if (vm->halt_flag) {
        return;
    }

    size_t i = low;
    size_t j = mid + 1;

    // Copy the slice from [array] to [aux_array].
    memcpy(&aux_array[low], &array[low], sizeof(Value) * (high - low + 1));

    for (size_t k = low; k <= high; k++) {
        if (i > mid) {
            array[k] = aux_array[j];
            j++;
            continue;
        }

        if (j > high) {
            array[k] = aux_array[i];
            i++;
            continue;
        }

        bool j_is_less_than_i;

        if (IS_NULL(fn)) {
            j_is_less_than_i = pyro_compare_lt(vm, aux_array[j], aux_array[i]);
        } else {
            pyro_push(vm, fn);
            pyro_push(vm, aux_array[j]);
            pyro_push(vm, aux_array[i]);
            Value return_value = pyro_call_fn(vm, fn, 2);
            if (vm->halt_flag) {
                return;
            } else if (!IS_BOOL(return_value)) {
                pyro_panic(vm, ERR_TYPE_ERROR, "Comparison function must return a boolean.");
                return;
            } else {
                j_is_less_than_i = return_value.as.boolean;
            }
        }

        if (j_is_less_than_i) {
            array[k] = aux_array[j];
            j++;
        } else {
            array[k] = aux_array[i];
            i++;
        }
    }
}


// Sorts the slice of [array] identified by the inclusive indices [low] and [high]. [aux_array] is
// used as scratch space and should have the same length as [array].
static void pyro_mergesort(Value* array, Value* aux_array, size_t low, size_t high, Value fn, PyroVM* vm) {
    if (high <= low) {
        return;
    }

    size_t mid = low + (high - low) / 2;

    pyro_mergesort(array, aux_array, low, mid, fn, vm);
    pyro_mergesort(array, aux_array, mid + 1, high, fn, vm);
    merge(array, aux_array, low, mid, high, fn, vm);
}


bool ObjVec_mergesort(ObjVec* vec, Value fn, PyroVM* vm) {
    if (vec->count < 2) {
        return true;
    }

    Value* aux_array = ALLOCATE_ARRAY(vm, Value, vec->count);
    if (!aux_array) {
        return false;
    }

    pyro_mergesort(vec->values, aux_array, 0, vec->count - 1, fn, vm);
    FREE_ARRAY(vm, Value, aux_array, vec->count);
    return true;
}


Value ObjVec_remove_last(ObjVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Cannot remove last item from empty vector.");
        return NULL_VAL();
    }
    vec->count--;
    return vec->values[vec->count];
}


Value ObjVec_remove_first(ObjVec* vec, PyroVM* vm) {
    if (vec->count == 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Cannot remove first item from empty vector.");
        return NULL_VAL();
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return NULL_VAL();
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
        pyro_panic(vm, ERR_VALUE_ERROR, "Index out of range.");
        return;
    }

    if (index == vec->count) {
        if (!ObjVec_append(vec, value, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        }
        return;
    }

    if (vec->count == vec->capacity) {
        size_t new_capacity = GROW_CAPACITY(vec->capacity);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, vec->values, vec->capacity, new_capacity);
        if (!new_array) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
    buf->obj.class = vm->buf_class;
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

    pyro_push(vm, OBJ_VAL(buf));
    uint8_t* new_array = ALLOCATE_ARRAY(vm, uint8_t, capacity + 1);
    pyro_pop(vm);

    if (!new_array) {
        return NULL;
    }

    buf->bytes = new_array;
    buf->capacity = capacity + 1;

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


bool ObjBuf_append_bytes(ObjBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm) {
    size_t req_capacity = buf->count + count + 1;

    if (req_capacity > buf->capacity) {
        size_t new_capacity = GROW_CAPACITY(buf->capacity);
        while (new_capacity < req_capacity) {
            new_capacity = GROW_CAPACITY(new_capacity);
        }
        uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity, new_capacity);
        if (!new_array) {
            return false;
        }
        buf->capacity = new_capacity;
        buf->bytes = new_array;
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


/* ----- */
/* Files */
/* ----- */


ObjFile* ObjFile_new(PyroVM* vm) {
    ObjFile* file = ALLOCATE_OBJECT(vm, ObjFile, OBJ_FILE);
    if (!file) {
        return NULL;
    }
    file->stream = NULL;
    file->obj.class = vm->file_class;
    return file;
}


/* --------- */
/* Iterators */
/* --------- */


ObjIter* ObjIter_new(Obj* source, IterType iter_type, PyroVM* vm) {
    ObjIter* iter = ALLOCATE_OBJECT(vm, ObjIter, OBJ_ITER);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->iter_class;
    iter->source = source;
    iter->iter_type = iter_type;
    iter->next_index = 0;
    iter->next_enum = 0;
    iter->callback = NULL;
    iter->range_next = 0;
    iter->range_stop = 0;
    iter->range_step = 0;
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
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_TUP: {
            ObjTup* tup = (ObjTup*)iter->source;
            if (iter->next_index < tup->count) {
                iter->next_index++;
                return tup->values[iter->next_index - 1];
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_STR_BYTES: {
            ObjStr* str = (ObjStr*)iter->source;
            if (iter->next_index < str->length) {
                int64_t byte_value = (uint8_t)str->bytes[iter->next_index];
                iter->next_index++;
                return I64_VAL(byte_value);
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_STR_CHARS: {
            ObjStr* str = (ObjStr*)iter->source;
            if (iter->next_index < str->length) {
                uint8_t* src = (uint8_t*)&str->bytes[iter->next_index];
                size_t src_len = str->length - iter->next_index;
                Utf8CodePoint cp;
                if (pyro_read_utf8_codepoint(src, src_len, &cp)) {
                    iter->next_index += cp.length;
                    return CHAR_VAL(cp.value);
                }
                pyro_panic(
                    vm, ERR_VALUE_ERROR,
                    "String contains invalid utf-8 at byte index %zu.", iter->next_index
                );
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_MAP_KEYS: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->capacity) {
                MapEntry* entry = &map->entries[iter->next_index];
                iter->next_index++;
                if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->key;
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_MAP_VALUES: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->capacity) {
                MapEntry* entry = &map->entries[iter->next_index];
                iter->next_index++;
                if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                return entry->value;
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_MAP_ENTRIES: {
            ObjMap* map = (ObjMap*)iter->source;
            while (iter->next_index < map->capacity) {
                MapEntry* entry = &map->entries[iter->next_index];
                iter->next_index++;
                if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
                    continue;
                }

                ObjTup* tup = ObjTup_new(2, vm);
                if (!tup) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    return OBJ_VAL(vm->empty_error);
                }

                tup->values[0] = entry->key;
                tup->values[1] = entry->value;
                return OBJ_VAL(tup);
            }
            return OBJ_VAL(vm->empty_error);
        }

        case ITER_MAP_FUNC: {
            ObjIter* src_iter = (ObjIter*)iter->source;
            Value next_value = ObjIter_next(src_iter, vm);
            if (IS_ERR(next_value)) {
                return next_value;
            }

            pyro_push(vm, OBJ_VAL(iter->callback));
            pyro_push(vm, next_value);
            Value result = pyro_call_fn(vm, OBJ_VAL(iter->callback), 1);
            if (vm->halt_flag) {
                return OBJ_VAL(vm->empty_error);
            }

            return result;
        }

        case ITER_FILTER_FUNC: {
            ObjIter* src_iter = (ObjIter*)iter->source;

            while (true) {
                Value next_value = ObjIter_next(src_iter, vm);
                if (IS_ERR(next_value)) {
                    return next_value;
                }

                pyro_push(vm, OBJ_VAL(iter->callback));
                pyro_push(vm, next_value);
                Value result = pyro_call_fn(vm, OBJ_VAL(iter->callback), 1);
                if (vm->halt_flag) {
                    return OBJ_VAL(vm->empty_error);
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

            pyro_push(vm, next_value);
            ObjTup* tup = ObjTup_new(2, vm);
            pyro_pop(vm);

            if (!tup) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return OBJ_VAL(vm->empty_error);
            }

            tup->values[0] = I64_VAL(iter->next_enum);
            tup->values[1] = next_value;
            iter->next_enum++;

            return OBJ_VAL(tup);
        }

        case ITER_GENERIC: {
            Value next_method = pyro_get_method(vm, OBJ_VAL(iter->source), vm->str_next);
            pyro_push(vm, OBJ_VAL(iter->source));
            Value result = pyro_call_method(vm, next_method, 0);
            if (vm->halt_flag) {
                return OBJ_VAL(vm->empty_error);
            }
            return result;
        }

        case ITER_RANGE: {
            if (iter->range_step > 0) {
                if (iter->range_next < iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return I64_VAL(range_next);
                }
                return OBJ_VAL(vm->empty_error);
            }

            if (iter->range_step < 0) {
                if (iter->range_next > iter->range_stop) {
                    int64_t range_next = iter->range_next;
                    iter->range_next += iter->range_step;
                    return I64_VAL(range_next);
                }
                return OBJ_VAL(vm->empty_error);
            }

            return OBJ_VAL(vm->empty_error);
        }

        default:
            assert(false);
            return OBJ_VAL(vm->empty_error);
    }
}


/* ------ */
/* Queues */
/* ------ */


ObjQueue* ObjQueue_new(PyroVM* vm) {
    ObjQueue* queue = ALLOCATE_OBJECT(vm, ObjQueue, OBJ_QUEUE);
    if (!queue) {
        return NULL;
    }
    queue->obj.class = vm->queue_class;
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
