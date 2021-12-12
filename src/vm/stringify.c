#include "stringify.h"
#include "utils.h"
#include "vm.h"
#include "exec.h"
#include "heap.h"
#include "utf8.h"


// Returns a quoted, escaped string or NULL if memory allocation fails.
static ObjStr* make_debug_string_for_string(PyroVM* vm, ObjStr* input_string) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '"', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    for (size_t i = 0; i < input_string->length; i++) {
        bool okay;
        char c = input_string->bytes[i];

        switch (c) {
            case '"':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\"", vm);
                break;
            case '\\':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\\", vm);
                break;
            case '\0':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\0", vm);
                break;
            case '\b':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\b", vm);
                break;
            case '\n':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\n", vm);
                break;
            case '\r':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\r", vm);
                break;
            case '\t':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\t", vm);
                break;
            default:
                if (c < 32 || c == 127) {
                    okay = ObjBuf_append_hex_escaped_byte(buf, c, vm);
                } else {
                    okay = ObjBuf_append_byte(buf, c, vm);
                }
                break;
        }

        if (!okay) {
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


// Returns a quoted, escaped string or NULL if memory allocation fails.
static ObjStr* make_debug_string_for_char(PyroVM* vm, Value value) {
    uint8_t utf8_buffer[4];
    size_t count = pyro_write_utf8_codepoint(value.as.u32, utf8_buffer);

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '\'', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    bool okay;

    if (count == 1) {
        switch (utf8_buffer[0]) {
            case '\'':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\'", vm);
                break;
            case '\\':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\\", vm);
                break;
            case '\0':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\0", vm);
                break;
            case '\b':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\b", vm);
                break;
            case '\n':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\n", vm);
                break;
            case '\r':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\r", vm);
                break;
            case '\t':
                okay = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\t", vm);
                break;
            default:
                if (utf8_buffer[0] < 32 || utf8_buffer[0] == 127) {
                    okay = ObjBuf_append_hex_escaped_byte(buf, utf8_buffer[0], vm);
                } else {
                    okay = ObjBuf_append_byte(buf, utf8_buffer[0], vm);
                }
                break;
        }
    } else {
        okay = ObjBuf_append_bytes(buf, count, utf8_buffer, vm);
    }

    if (!okay) {
        pyro_pop(vm);
        return NULL;
    }

    if (!ObjBuf_append_byte(buf, '\'', vm)) {
        pyro_pop(vm);
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm);
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_tuple(PyroVM* vm, ObjTup* tup) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '(', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    for (size_t i = 0; i < tup->count; i++) {
        Value item = tup->values[i];
        ObjStr* item_string = pyro_stringify_debug(vm, item);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(item_string));
        if (!ObjBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // item_string

        if (i + 1 < tup->count) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
        }
    }

    if (!ObjBuf_append_byte(buf, ')', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm); // buf

    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_vector(PyroVM* vm, ObjVec* vec) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '[', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    for (size_t i = 0; i < vec->count; i++) {
        Value item = vec->values[i];
        ObjStr* item_string = pyro_stringify_debug(vm, item);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(item_string));
        if (!ObjBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // item_string

        if (i + 1 < vec->count) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
        }
    }

    if (!ObjBuf_append_byte(buf, ']', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm); // buf

    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_map(PyroVM* vm, ObjMap* map) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '{', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
        }
        is_first_entry = false;

        ObjStr* key_string = pyro_stringify_debug(vm, entry->key);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(key_string));
        if (!ObjBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // key_string

        if (!ObjBuf_append_bytes(buf, 3, (uint8_t*)" = ", vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }

        ObjStr* value_string = pyro_stringify_debug(vm, entry->value);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(value_string));
        if (!ObjBuf_append_bytes(buf, value_string->length, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // value_string
    }

    if (!ObjBuf_append_byte(buf, '}', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm); // buf

    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_map_as_set(PyroVM* vm, ObjMap* map) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '{', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
        }
        is_first_entry = false;

        ObjStr* key_string = pyro_stringify_debug(vm, entry->key);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(key_string));
        if (!ObjBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // key_string
    }

    if (!ObjBuf_append_byte(buf, '}', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm); // buf

    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_queue(PyroVM* vm, ObjQueue* queue) {
    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    pyro_push(vm, OBJ_VAL(buf));

    if (!ObjBuf_append_byte(buf, '[', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    QueueItem* next_item = queue->head;
    bool is_first_item = true;

    while (next_item) {
        if (!is_first_item) {
            if (!ObjBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
        }

        ObjStr* item_string = pyro_stringify_debug(vm, next_item->value);
        if (vm->halt_flag) {
            return NULL;
        }

        pyro_push(vm, OBJ_VAL(item_string));
        if (!ObjBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_pop(vm); // item_string

        is_first_item = false;
        next_item = next_item->next;
    }

    if (!ObjBuf_append_byte(buf, ']', vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    ObjStr* output_string =  ObjBuf_to_str(buf, vm);
    pyro_pop(vm); // buf

    if (!output_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static ObjStr* stringify_object(PyroVM* vm, Obj* object) {
    Value method = pyro_get_method(vm, OBJ_VAL(object), vm->str_str);
    if (!IS_NULL(method)) {
        pyro_push(vm, OBJ_VAL(object));
        Value result = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!IS_STR(result)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid type returned by :$str() method.");
            return NULL;
        }
        return AS_STR(result);
    }

    switch (object->type) {
        case OBJ_STR:
            return (ObjStr*)object;

        case OBJ_MODULE:
            return pyro_sprintf_to_obj(vm, "<module>");

        case OBJ_UPVALUE:
            return pyro_sprintf_to_obj(vm, "<upvalue>");

        case OBJ_FILE:
            return pyro_sprintf_to_obj(vm, "<file>");

        case OBJ_FN:
            return pyro_sprintf_to_obj(vm, "<fn>");

        case OBJ_BOUND_METHOD:
            return pyro_sprintf_to_obj(vm, "<method>");

        case OBJ_ITER:
            return pyro_sprintf_to_obj(vm, "<iter>");

        case OBJ_QUEUE: {
            ObjQueue* queue = (ObjQueue*)object;
            return stringify_queue(vm, queue);
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)object;
            ObjStr* string = ObjStr_copy_raw((char*)buf->bytes, buf->count, vm);
            if (!string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
            return string;
        }

        case OBJ_TUP_AS_ERR:
        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            return stringify_tuple(vm, tup);
        }

        case OBJ_VEC_AS_STACK:
        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            return stringify_vector(vm, vec);
        }

        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            return stringify_map(vm, map);
        }

        case OBJ_MAP_AS_SET: {
            ObjMap* map = (ObjMap*)object;
            return stringify_map_as_set(vm, map);
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            return pyro_sprintf_to_obj(vm, "<fn %s>", native->name->bytes);
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            return pyro_sprintf_to_obj(vm, "<fn %s>", closure->fn->name->bytes);
        }

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            if (class->name) {
                return pyro_sprintf_to_obj(vm, "<class %s>", class->name->bytes);
            }
            return pyro_sprintf_to_obj(vm, "<class>");
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            if (instance->obj.class->name) {
                return pyro_sprintf_to_obj(vm, "<instance %s>", instance->obj.class->name->bytes);
            }
            return pyro_sprintf_to_obj(vm, "<instance>");
        }

        default:
            return pyro_sprintf_to_obj(vm, "<object>");
    }
}


char* pyro_stringify_object_type(ObjType type) {
    switch (type) {
        case OBJ_BOUND_METHOD: return "<method>";
        case OBJ_BUF: return "<buf>";
        case OBJ_CLASS: return "<class>";
        case OBJ_CLOSURE: return "<closure>";
        case OBJ_TUP_AS_ERR: return "<err>";
        case OBJ_FILE: return "<file>";
        case OBJ_FN: return "<fn>";
        case OBJ_INSTANCE: return "<instance>";
        case OBJ_MAP: return "<map>";
        case OBJ_MAP_AS_SET: return "<set>";
        case OBJ_MAP_AS_WEAKREF: return "<weakref map>";
        case OBJ_MODULE: return "<module>";
        case OBJ_NATIVE_FN: return "<native fn>";
        case OBJ_STR: return "<str>";
        case OBJ_TUP: return "<tup>";
        case OBJ_UPVALUE: return "<upvalue>";
        case OBJ_VEC: return "<vec>";
        case OBJ_VEC_AS_STACK: return "<stack>";
        case OBJ_ITER: return "<iter>";
        case OBJ_QUEUE: return "<queue>";
        default: assert(false); return "<unknown>";
    }
}


char* pyro_sprintf(PyroVM* vm, const char* format_string, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [length] will be the output string length,
    // not counting the terminating null, so we'll need to allocate [length + 1] bytes.
    // Possible optimization: print to a static buffer here, then if the output fits memcpy it to
    // the output array.
    va_start(args, format_string);
    int length = vsnprintf(NULL, 0, format_string, args);
    va_end(args);

    // If [length] is negative, an encoding error occurred.
    if (length < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Invalid format string '%s'.", format_string);
        return NULL;
    }

    char* array = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!array) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    va_start(args, format_string);
    vsnprintf(array, length + 1, format_string, args);
    va_end(args);

    return array;
}


ObjStr* pyro_sprintf_to_obj(PyroVM* vm, const char* format_string, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [length] will be the output string length,
    // not counting the terminating null, so we'll need to allocate [length + 1] bytes.
    // Possible optimization: print to a static buffer here, then if the output fits memcpy it to
    // the output array.
    va_start(args, format_string);
    int length = vsnprintf(NULL, 0, format_string, args);
    va_end(args);

    // If [length] is negative, an encoding error occurred.
    if (length < 0) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Invalid format string '%s'.", format_string);
        return NULL;
    }

    char* array = ALLOCATE_ARRAY(vm, char, length + 1);
    if (!array) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    va_start(args, format_string);
    vsnprintf(array, length + 1, format_string, args);
    va_end(args);

    ObjStr* string = ObjStr_take(array, length, vm);
    if (!string) {
        FREE_ARRAY(vm, char, array, length + 1);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    return string;
}


ObjStr* pyro_stringify(PyroVM* vm, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            return value.as.boolean ? vm->str_true : vm->str_false;

        case VAL_NULL:
            return vm->str_null;

        case VAL_I64:
            return pyro_sprintf_to_obj(vm, "%lld", value.as.i64);

        case VAL_F64: {
            char* array = pyro_sprintf(vm, "%.6f", value.as.f64);
            if (vm->halt_flag) {
                return NULL;
            }

            size_t original_length = strlen(array);
            size_t trimmed_length = original_length;

            while (array[trimmed_length - 1] == '0') {
                trimmed_length--;
            }

            if (array[trimmed_length - 1] == '.') {
                trimmed_length++;
            }

            ObjStr* string = ObjStr_copy_raw(array, trimmed_length, vm);
            FREE_ARRAY(vm, char, array, original_length + 1);
            if (!string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
            return string;
        }

        case VAL_CHAR: {
            char buffer[4];
            size_t count = pyro_write_utf8_codepoint(value.as.u32, (uint8_t*)buffer);
            ObjStr* string = ObjStr_copy_raw(buffer, count, vm);
            if (!string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL;
            }
            return string;
        }

        case VAL_OBJ:
            return stringify_object(vm, AS_OBJ(value));

        default:
            return pyro_sprintf_to_obj(vm, "<value>");
    }
}


ObjStr* pyro_stringify_debug(PyroVM* vm, Value value) {
    Value method = pyro_get_method(vm, value, vm->str_debug);
    if (!IS_NULL(method)) {
        pyro_push(vm, value);
        Value result = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!IS_STR(result)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid type returned by :$debug() method.");
            return NULL;
        }
        return AS_STR(result);
    }

    if (IS_STR(value)) {
        ObjStr* string = make_debug_string_for_string(vm, AS_STR(value));
        if (!string) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        return string;
    }

    if (IS_CHAR(value)) {
        ObjStr* string = make_debug_string_for_char(vm, value);
        if (!string) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        return string;
    }

    return pyro_stringify(vm, value);
}


ObjStr* pyro_stringify_formatted(PyroVM* vm, Value value, const char* format_string) {
    if (IS_I64(value)) {
        return pyro_sprintf_to_obj(vm, format_string, value.as.i64);
    }

    if (IS_F64(value)) {
        return pyro_sprintf_to_obj(vm, format_string, value.as.f64);
    }

    if (IS_CHAR(value)) {
        return pyro_sprintf_to_obj(vm, format_string, value.as.u32);
    }

    Value method = pyro_get_method(vm, value, vm->str_fmt);
    if (!IS_NULL(method)) {
        ObjStr* format_string_object = STR(format_string);
        if (!format_string_object) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        pyro_push(vm, value);
        pyro_push(vm, OBJ_VAL(format_string_object));
        Value result = pyro_call_method(vm, method, 1);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!IS_STR(result)) {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid type returned by :$fmt() method.");
            return NULL;
        }
        return AS_STR(result);
    }

    pyro_panic(vm, ERR_VALUE_ERROR, "No handler for format specifier '%s'.", format_string);
    return NULL;
}
