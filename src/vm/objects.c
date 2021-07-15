#include "values.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "opcodes.h"


#define ALLOCATE_OBJECT(vm, type, type_enum) \
    (type*)allocate_object(vm, sizeof(type), type_enum)


#define ALLOCATE_FLEX_OBJECT(vm, type, type_enum, value_count, value_type) \
    (type*)allocate_object(vm, sizeof(type) + value_count * sizeof(value_type), type_enum)


// This function creates a new heap-allocated object and adds it to the VM's linked list.
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
        pyro_out(vm, ">> %p allocate %zu bytes for %s\n", (void*)object, size, pyro_stringify_obj_type(type));
    #endif

    return object;
}


// ------ //
// ObjTup //
// ------ //


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
    tup->obj.type = OBJ_ERR;
    return tup;
}


uint64_t ObjTup_hash(ObjTup* tup) {
    uint64_t hash = 0;
    for (size_t i = 0; i < tup->count; i++) {
        hash ^= pyro_hash(tup->values[i]);
    }
    return hash;
}


bool ObjTup_check_equal(ObjTup* a, ObjTup* b) {
    if (a->count == b->count) {
        for (size_t i = 0; i < a->count; i++) {
            if (!pyro_check_equal(a->values[i], b->values[i])) {
                return false;
            }
        }
        return true;
    }
    return false;
}


ObjStr* ObjTup_stringify(ObjTup* tup, PyroVM* vm) {
    ObjBuf* buf = ObjBuf_new(vm);
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '(', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < tup->count; i++) {
        if (IS_STR(tup->values[i]) && !ObjBuf_append_byte(buf, '"', vm)) {
            pyro_pop(vm);
            return NULL;
        } else if (IS_CHAR(tup->values[i]) && !ObjBuf_append_byte(buf, '\'', vm)) {
            pyro_pop(vm);
            return NULL;
        }

        ObjStr* string = pyro_stringify_value(vm, tup->values[i]);
        if (vm->halt_flag || !string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(string));
        if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);

        if (IS_STR(tup->values[i]) && !ObjBuf_append_byte(buf, '"', vm)) {
            pyro_pop(vm);
            return NULL;
        } else if (IS_CHAR(tup->values[i]) && !ObjBuf_append_byte(buf, '\'', vm)) {
            pyro_pop(vm);
            return NULL;
        }

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

    ObjStr* string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return string;
}


ObjTupIter* ObjTupIter_new(ObjTup* tup, PyroVM* vm) {
    ObjTupIter* iter = ALLOCATE_OBJECT(vm, ObjTupIter, OBJ_TUP_ITER);
    if (iter == NULL) {
        return NULL;
    }

    iter->obj.class = vm->tup_iter_class;
    iter->tup = tup;
    iter->next_index = 0;
    return iter;
}


// ---------- //
// ObjClosure //
// ---------- //


ObjClosure* ObjClosure_new(PyroVM* vm, ObjFn* fn) {
    ObjClosure* closure = ALLOCATE_OBJECT(vm, ObjClosure, OBJ_CLOSURE);
    if (closure == NULL) {
        return NULL;
    }

    closure->fn = fn;
    closure->upvalues = NULL;
    closure->upvalue_count = 0;

    if (fn->upvalue_count > 0) {
        pyro_push(vm, OBJ_VAL(closure));
        closure->upvalues = ALLOCATE_ARRAY(vm, ObjUpvalue*, fn->upvalue_count);
        pyro_pop(vm);

        if (closure->upvalues == NULL) {
            pyro_memory_error(vm);
            return NULL;
        }

        closure->upvalue_count = fn->upvalue_count;
        for (size_t i = 0; i < fn->upvalue_count; i++) {
            closure->upvalues[i] = NULL;
        }
    }

    return closure;
}


// ---------- //
// ObjUpvalue //
// ---------- //


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


// -------- //
// ObjClass //
// -------- //


ObjClass* ObjClass_new(PyroVM* vm) {
    ObjClass* class = ALLOCATE_OBJECT(vm, ObjClass, OBJ_CLASS);
    if (!class) {
        return NULL;
    }

    class->name = NULL;
    class->methods = NULL;
    class->field_initializers = NULL;
    class->field_indexes = NULL;

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


// ----------- //
// ObjInstance //
// ----------- //


ObjInstance* ObjInstance_new(PyroVM* vm, ObjClass* class) {
    size_t num_fields = class->field_initializers->count;
    ObjInstance* instance = ALLOCATE_FLEX_OBJECT(vm, ObjInstance, OBJ_INSTANCE, num_fields, Value);
    if (!instance) {
        return NULL;
    }
    instance->obj.class = class;
    memcpy(instance->fields, class->field_initializers->values, sizeof(Value) * num_fields);
    return instance;
}


// ------ //
// ObjMap //
// ------ //


static MapEntry* find_entry(MapEntry* entries, size_t capacity, Value key) {
    // Capacity is always a power of 2 so we can use bitwise-AND as a fast
    // modulo operator, i.e. this is equivalent to: index = key_hash % capacity.
    size_t index = (size_t)pyro_hash(key) & (capacity - 1);
    MapEntry* tombstone = NULL;

    for (;;) {
        MapEntry* entry = &entries[index];

        if (IS_EMPTY(entry->key)) {
            return tombstone == NULL ? entry : tombstone;
        } else if (IS_TOMBSTONE(entry->key)) {
            if (tombstone == NULL) tombstone = entry;
        } else if (pyro_check_equal(key, entry->key)) {
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
        MapEntry* dst = find_entry(new_entries, new_capacity, src->key);
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


ObjMap* ObjMap_new_weakref(PyroVM* vm) {
    ObjMap* map = ObjMap_new(vm);
    if (!map) {
        return NULL;
    }
    map->obj.type = OBJ_WEAKREF_MAP;
    return map;
}


int ObjMap_set(ObjMap* map, Value key, Value value, PyroVM* vm) {
    if (map->capacity == 0) {
        size_t new_capacity = GROW_CAPACITY(map->capacity);
        if (!resize_map(map, new_capacity, vm)) {
            return 0;
        }
    }

    MapEntry* entry = find_entry(map->entries, map->capacity, key);

    if (IS_EMPTY(entry->key)) {
        if (map->count == map->max_load_threshold) {
            size_t new_capacity = GROW_CAPACITY(map->capacity);
            if (!resize_map(map, new_capacity, vm)) {
                return 0;
            }
            entry = find_entry(map->entries, map->capacity, key);
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

    MapEntry* entry = find_entry(map->entries, map->capacity, key);
    if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
        return false;
    }

    entry->key = key;
    entry->value = value;
    return true;
}


bool ObjMap_get(ObjMap* map, Value key, Value* value) {
    if (map->count == 0) {
        return false;
    }

    MapEntry* entry = find_entry(map->entries, map->capacity, key);
    if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
        return false;
    }

    *value = entry->value;
    return true;
}


bool ObjMap_remove(ObjMap* map, Value key) {
    if (map->count == 0) {
        return false;
    }

    MapEntry* entry = find_entry(map->entries, map->capacity, key);
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
        if (!ObjMap_set(dst, entry->key, entry->value, vm)) {
            return false;
        }
    }
    return true;
}


ObjMapIter* ObjMapIter_new(ObjMap* map, MapIterType iter_type, PyroVM* vm) {
    ObjMapIter* iter = ALLOCATE_OBJECT(vm, ObjMapIter, OBJ_MAP_ITER);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->map_iter_class;
    iter->map = map;
    iter->iter_type = iter_type;
    iter->next_index = 0;
    return iter;
}


Value ObjMapIter_next(ObjMapIter* iterator, PyroVM* vm) {
    while (iterator->next_index < iterator->map->capacity) {
        MapEntry* entry = &iterator->map->entries[iterator->next_index];
        iterator->next_index++;

        if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
            continue;
        }

        switch (iterator->iter_type) {
            case MAP_ITER_KEYS:
                return entry->key;

            case MAP_ITER_VALUES:
                return entry->value;

            case MAP_ITER_ENTRIES: {
                ObjTup* tup = ObjTup_new(2, vm);
                tup->values[0] = entry->key;
                tup->values[1] = entry->value;
                return OBJ_VAL(tup);
            }
        }
    }

    return OBJ_VAL(vm->empty_error);
}


// ------ //
// ObjStr //
// ------ //


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


ObjStrIter* ObjStrIter_new(ObjStr* string, StrIterType iter_type, PyroVM* vm) {
    ObjStrIter* iter = ALLOCATE_OBJECT(vm, ObjStrIter, OBJ_STR_ITER);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->str_iter_class;
    iter->string = string;
    iter->iter_type = iter_type;
    iter->next_index = 0;
    return iter;
}


// ----- //
// ObjFn //
// ----- //


ObjFn* ObjFn_new(PyroVM* vm) {
    ObjFn* fn = ALLOCATE_OBJECT(vm, ObjFn, OBJ_FN);
    if (!fn) {
        return NULL;
    }

    fn->arity = 0;
    fn->upvalue_count = 0;
    fn->name = NULL;
    fn->source = NULL;
    fn->module = NULL;

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


// This method returns the source code line number corresponding to the bytecode instruction at
// index [ip].
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


// This method adds a value to the function's constant table and returns its index. If an identical
// value is already present in the table it avoids adding a duplicate and returns the index of
// the existing entry instead. Returns SIZE_MAX if the operation failed because sufficient memory
// could not be allocated for the constant table.
size_t ObjFn_add_constant(ObjFn* fn, Value value, PyroVM* vm) {
    for (size_t i = 0; i < fn->constants_count; i++) {
        if (pyro_check_equal(value, fn->constants[i])) {
            return i;
        }
    }

    if (fn->constants_count == fn->constants_capacity) {
        size_t new_capacity = GROW_CAPACITY(fn->constants_capacity);
        pyro_push(vm, value);
        Value* new_array = REALLOCATE_ARRAY(vm, Value, fn->constants, fn->constants_capacity, new_capacity);
        pyro_pop(vm);

        if (!new_array) {
            return SIZE_MAX;
        }

        fn->constants_capacity = new_capacity;
        fn->constants = new_array;
    }

    fn->constants[fn->constants_count++] = value;
    return fn->constants_count - 1;
}


// Returns the length in bytes of the arguments for the opcode at the specified index.
size_t ObjFn_opcode_argcount(ObjFn* fn, size_t ip) {
    switch (fn->code[ip]) {
        case OP_ASSERT:
        case OP_ADD:
        case OP_CLOSE_UPVALUE:
        case OP_DUP:
        case OP_EQUAL:
        case OP_FLOAT_DIV:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_INHERIT:
        case OP_ITER:
        case OP_ITER_NEXT:
        case OP_LESS:
        case OP_LESS_EQUAL:
        case OP_LOAD_FALSE:
        case OP_LOAD_NULL:
        case OP_LOAD_TRUE:
        case OP_MULTIPLY:
        case OP_NEGATE:
        case OP_LOGICAL_NOT:
        case OP_NOT_EQUAL:
        case OP_POP:
        case OP_RETURN:
        case OP_SUBTRACT:
        case OP_TRUNC_DIV:
        case OP_TRY:
        case OP_GET_INDEX:
        case OP_SET_INDEX:
        case OP_DUP2:
        case OP_POWER:
        case OP_MODULO:
        case OP_BITWISE_NOT:
        case OP_BITWISE_XOR:
        case OP_BITWISE_AND:
        case OP_BITWISE_OR:
        case OP_LSHIFT:
        case OP_RSHIFT:
            return 0;

        case OP_CALL:
        case OP_ECHO:
        case OP_GET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_IMPORT:
        case OP_SET_LOCAL:
        case OP_SET_UPVALUE:
            return 1;

        case OP_BREAK:
        case OP_CLASS:
        case OP_DEFINE_GLOBAL:
        case OP_FIELD:
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
        case OP_METHOD:
        case OP_POP_JUMP_IF_FALSE:
        case OP_SET_FIELD:
        case OP_SET_GLOBAL:
            return 2;

        case OP_INVOKE_METHOD:
        case OP_INVOKE_SUPER_METHOD:
            return 3;

        // 2 bytes for the constant index, plus two for each upvalue.
        case OP_CLOSURE: {
            uint16_t const_index = (fn->code[ip + 1] << 8) | fn->code[ip + 2];
            ObjFn* closure_fn = AS_FN(fn->constants[const_index]);
            return 2 + closure_fn->upvalue_count * 2;
        }

        default:
            printf("Compiler Error: unhandled opcode.\n");
            assert(false);
            exit(1);
    }
}


// ----------- //
// ObjNativeFn //
// ----------- //


ObjNativeFn* ObjNativeFn_new(PyroVM* vm, NativeFn fn_ptr, ObjStr* name, int arity) {
    ObjNativeFn* native = ALLOCATE_OBJECT(vm, ObjNativeFn, OBJ_NATIVE_FN);
    if (!native) {
        return NULL;
    }
    native->fn_ptr = fn_ptr;
    native->name = name;
    native->arity = arity;
    return native;
}


// -------------- //
// ObjBoundMethod //
// -------------- //


ObjBoundMethod* ObjBoundMethod_new(PyroVM* vm, Value receiver, Obj* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJECT(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
    if (!bound) {
        return NULL;
    }
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}


// --------- //
// ObjModule //
// --------- //


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


// ------ //
// ObjVec //
// ------ //


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
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '[', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < vec->count; i++) {
        if (IS_STR(vec->values[i]) && !ObjBuf_append_byte(buf, '"', vm)) {
            pyro_pop(vm);
            return NULL;
        } else if (IS_CHAR(vec->values[i]) && !ObjBuf_append_byte(buf, '\'', vm)) {
            pyro_pop(vm);
            return NULL;
        }

        ObjStr* string = pyro_stringify_value(vm, vec->values[i]);
        if (vm->halt_flag || !string) {
            pyro_pop(vm);
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(string));
        if (!ObjBuf_append_bytes(buf, string->length, (uint8_t*)string->bytes, vm)) {
            pyro_pop(vm);
            pyro_pop(vm);
            return NULL;
        }
        pyro_pop(vm);

        if (IS_STR(vec->values[i]) && !ObjBuf_append_byte(buf, '"', vm)) {
            pyro_pop(vm);
            return NULL;
        } else if (IS_CHAR(vec->values[i]) && !ObjBuf_append_byte(buf, '\'', vm)) {
            pyro_pop(vm);
            return NULL;
        }

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

    ObjStr* string = ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return string;
}


ObjVecIter* ObjVecIter_new(ObjVec* vec, PyroVM* vm) {
    ObjVecIter* iter = ALLOCATE_OBJECT(vm, ObjVecIter, OBJ_VEC_ITER);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->vec_iter_class;
    iter->vec = vec;
    iter->next_index = 0;
    return iter;
}


// -------- //
// ObjRange //
// -------- //


ObjRange* ObjRange_new(int64_t start, int64_t stop, int64_t step, PyroVM* vm) {
    ObjRange* iter = ALLOCATE_OBJECT(vm, ObjRange, OBJ_RANGE);
    if (!iter) {
        return NULL;
    }
    iter->obj.class = vm->range_class;
    iter->next = start;
    iter->stop = stop;
    iter->step = step;
    return iter;
}


// ------ //
// ObjBuf //
// ------ //


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


// ------- //
// ObjFile //
// ------- //


ObjFile* ObjFile_new(PyroVM* vm) {
    ObjFile* file = ALLOCATE_OBJECT(vm, ObjFile, OBJ_FILE);
    if (!file) {
        return NULL;
    }
    file->stream = NULL;
    file->obj.class = vm->file_class;
    return file;
}
