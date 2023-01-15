#include "../inc/stringify.h"
#include "../inc/utils.h"
#include "../inc/vm.h"
#include "../inc/exec.h"
#include "../inc/heap.h"
#include "../inc/utf8.h"
#include "../inc/panics.h"


// Returns a quoted, escaped string. Panics and returns NULL if memory allocation fails.
static PyroStr* make_debug_string_for_string(PyroVM* vm, PyroStr* input_string) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    if (!PyroBuf_append_byte(buf, '"', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < input_string->length; i++) {
        bool ok;
        char c = input_string->bytes[i];

        switch (c) {
            case '"':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\\"", vm);
                break;
            case '\0':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\0", vm);
                break;
            case '\b':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\b", vm);
                break;
            case '\n':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\n", vm);
                break;
            case '\r':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\r", vm);
                break;
            case '\t':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\t", vm);
                break;
            default:
                if (c < 32 || c == 127) {
                    ok = PyroBuf_append_hex_escaped_byte(buf, c, vm);
                } else {
                    ok = PyroBuf_append_byte(buf, c, vm);
                }
                break;
        }

        if (!ok) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }
    }

    if (!PyroBuf_append_byte(buf, '"', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return PyroBuf_to_str(buf, vm);
}


// Returns a <buf "content"> string containing a quoted, escaped string representation of the
// buffer's content. Panics and returns NULL if memory allocation fails.
static PyroStr* make_debug_string_for_buf(PyroVM* vm, PyroBuf* buf) {
    if (buf->count == 0) {
        return pyro_sprintf_to_obj(vm, "<buf>");
    }

    PyroStr* raw_content = PyroStr_copy_raw((char*)buf->bytes, buf->count, vm);
    if (!raw_content) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* debug_string = make_debug_string_for_string(vm, raw_content);
    if (!debug_string) {
        return NULL;
    }

    return pyro_sprintf_to_obj(vm, "<buf %s>", debug_string->bytes);
}


// Returns a quoted, escaped string. Panics and returns NULL if memory allocation fails.
static PyroStr* make_debug_string_for_char(PyroVM* vm, PyroValue value) {
    uint8_t utf8_buffer[4];
    size_t count = pyro_write_utf8_codepoint(value.as.u32, utf8_buffer);

    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    if (!PyroBuf_append_byte(buf, '\'', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    bool ok;

    if (count == 1) {
        switch (utf8_buffer[0]) {
            case '\'':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\\'", vm);
                break;
            case '\\':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\\\", vm);
                break;
            case '\0':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\0", vm);
                break;
            case '\b':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\b", vm);
                break;
            case '\n':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\n", vm);
                break;
            case '\r':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\r", vm);
                break;
            case '\t':
                ok = PyroBuf_append_bytes(buf, 2, (uint8_t*)"\\t", vm);
                break;
            default:
                if (utf8_buffer[0] < 32 || utf8_buffer[0] == 127) {
                    ok = PyroBuf_append_hex_escaped_byte(buf, utf8_buffer[0], vm);
                } else {
                    ok = PyroBuf_append_byte(buf, utf8_buffer[0], vm);
                }
                break;
        }
    } else {
        ok = PyroBuf_append_bytes(buf, count, utf8_buffer, vm);
    }

    if (!ok) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    if (!PyroBuf_append_byte(buf, '\'', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return PyroBuf_to_str(buf, vm);
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_tuple(PyroVM* vm, PyroTup* tup) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    if (!PyroBuf_append_byte(buf, '(', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < tup->count; i++) {
        PyroValue item = tup->values[i];
        PyroStr* item_string = pyro_debugify_value(vm, item);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        if (i + 1 < tup->count) {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }
    }

    if (!PyroBuf_append_byte(buf, ')', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* output_string = PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_vector(PyroVM* vm, PyroVec* vec) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    if (!PyroBuf_append_byte(buf, '[', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    for (size_t i = 0; i < vec->count; i++) {
        PyroValue item = vec->values[i];
        PyroStr* item_string = pyro_debugify_value(vm, item);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        if (i + 1 < vec->count) {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }
    }

    if (!PyroBuf_append_byte(buf, ']', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* output_string =  PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_map(PyroVM* vm, PyroMap* map) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    if (!PyroBuf_append_byte(buf, '{', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    bool is_first_entry = true;

    for (size_t i = 0; i < map->entry_array_count; i++) {
        PyroMapEntry* entry = &map->entry_array[i];

        if (PYRO_IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!is_first_entry) {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }
        is_first_entry = false;

        PyroStr* key_string = pyro_debugify_value(vm, entry->key);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, 3, (uint8_t*)" = ", vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        PyroStr* value_string = pyro_debugify_value(vm, entry->value);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, value_string->length, (uint8_t*)value_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }
    }

    if (!PyroBuf_append_byte(buf, '}', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* output_string =  PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_map_as_set(PyroVM* vm, PyroMap* map) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    if (!PyroBuf_append_byte(buf, '{', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    bool is_first_entry = true;

    for (size_t i = 0; i < map->entry_array_count; i++) {
        PyroMapEntry* entry = &map->entry_array[i];

        if (PYRO_IS_TOMBSTONE(entry->key)) {
            continue;
        }

        if (!is_first_entry) {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }
        is_first_entry = false;

        PyroStr* key_string = pyro_debugify_value(vm, entry->key);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, key_string->length, (uint8_t*)key_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }
    }

    if (!PyroBuf_append_byte(buf, '}', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* output_string =  PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_queue(PyroVM* vm, PyroQueue* queue) {
    PyroBuf* buf = PyroBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    pyro_push(vm, pyro_obj(buf)); // Protect from GC in case we call into Pyro code.

    if (!PyroBuf_append_byte(buf, '[', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroQueueItem* next_item = queue->head;
    bool is_first_item = true;

    while (next_item) {
        if (!is_first_item) {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)", ", vm)) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
        }

        PyroStr* item_string = pyro_debugify_value(vm, next_item->value);
        if (vm->halt_flag) {
            return NULL;
        }

        if (!PyroBuf_append_bytes(buf, item_string->length, (uint8_t*)item_string->bytes, vm)) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        is_first_item = false;
        next_item = next_item->next;
    }

    if (!PyroBuf_append_byte(buf, ']', vm)) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    PyroStr* output_string =  PyroBuf_to_str(buf, vm);
    if (!output_string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    pyro_pop(vm); // buf
    return output_string;
}


// Panics and returns NULL if an error occurs. May call into Pyro code and set the exit flag.
static PyroStr* stringify_object(PyroVM* vm, PyroObject* object) {
    PyroValue method = pyro_get_method(vm, pyro_obj(object), vm->str_dollar_str);
    if (!PYRO_IS_NULL(method)) {
        pyro_push(vm, pyro_obj(object));
        PyroValue result = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!PYRO_IS_STR(result)) {
            pyro_panic(vm, "invalid return type for :$str(), expected a string");
            return NULL;
        }
        return PYRO_AS_STR(result);
    }

    switch (object->type) {
        case PYRO_OBJECT_STR:
            return (PyroStr*)object;

        case PYRO_OBJECT_MODULE:
            return pyro_sprintf_to_obj(vm, "<module>");

        case PYRO_OBJECT_UPVALUE:
            return pyro_sprintf_to_obj(vm, "<upvalue>");

        case PYRO_OBJECT_FILE: {
            PyroFile* file = (PyroFile*)object;

            if (file->stream == NULL) {
                return pyro_sprintf_to_obj(vm, "<file CLOSED>");
            }

            if (file->path) {
                PyroStr* path = make_debug_string_for_string(vm, file->path);
                if (!path) {
                    return NULL;
                }
                return pyro_sprintf_to_obj(vm, "<file %s>", path->bytes);
            }

            if (file->stream == stdin) {
                return pyro_sprintf_to_obj(vm, "<file STDIN>");
            }
            if (file->stream == stdout) {
                return pyro_sprintf_to_obj(vm, "<file STDOUT>");
            }
            if (file->stream == stderr) {
                return pyro_sprintf_to_obj(vm, "<file STDERR>");
            }

            return pyro_sprintf_to_obj(vm, "<file>");
        }

        case PYRO_OBJECT_FN:
            return pyro_sprintf_to_obj(vm, "<fn>");

        case PYRO_OBJECT_BOUND_METHOD: {
            PyroBoundMethod* bound_method = (PyroBoundMethod*)object;
            PyroObject* method = bound_method->method;

            switch (method->type) {
                case PYRO_OBJECT_NATIVE_FN:
                    return pyro_sprintf_to_obj(vm, "<method %s>", ((PyroNativeFn*)method)->name->bytes);
                case PYRO_OBJECT_CLOSURE:
                    return pyro_sprintf_to_obj(vm, "<method %s>", ((PyroClosure*)method)->fn->name->bytes);
                default:
                    return pyro_sprintf_to_obj(vm, "<method>");
            }
        }

        case PYRO_OBJECT_ITER:
            return pyro_sprintf_to_obj(vm, "<iter>");

        case PYRO_OBJECT_QUEUE: {
            PyroQueue* queue = (PyroQueue*)object;
            return stringify_queue(vm, queue);
        }

        case PYRO_OBJECT_BUF: {
            PyroBuf* buf = (PyroBuf*)object;
            PyroStr* string = PyroStr_copy_raw((char*)buf->bytes, buf->count, vm);
            if (!string) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
            return string;
        }

        case PYRO_OBJECT_TUP: {
            PyroTup* tup = (PyroTup*)object;
            return stringify_tuple(vm, tup);
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            PyroVec* vec = (PyroVec*)object;
            return stringify_vector(vm, vec);
        }

        case PYRO_OBJECT_MAP: {
            PyroMap* map = (PyroMap*)object;
            return stringify_map(vm, map);
        }

        case PYRO_OBJECT_MAP_AS_SET: {
            PyroMap* map = (PyroMap*)object;
            return stringify_map_as_set(vm, map);
        }

        case PYRO_OBJECT_NATIVE_FN: {
            PyroNativeFn* fn = (PyroNativeFn*)object;
            return pyro_sprintf_to_obj(vm, "<fn %s>", fn->name->bytes);
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroClosure* closure = (PyroClosure*)object;
            return pyro_sprintf_to_obj(vm, "<fn %s>", closure->fn->name->bytes);
        }

        case PYRO_OBJECT_CLASS: {
            PyroClass* class = (PyroClass*)object;
            if (class->name) {
                return pyro_sprintf_to_obj(vm, "<class %s>", class->name->bytes);
            }
            return pyro_sprintf_to_obj(vm, "<class>");
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroInstance* instance = (PyroInstance*)object;
            if (instance->obj.class->name) {
                return pyro_sprintf_to_obj(vm, "<instance %s>", instance->obj.class->name->bytes);
            }
            return pyro_sprintf_to_obj(vm, "<instance>");
        }

        case PYRO_OBJECT_ERR: {
            PyroErr* err = (PyroErr*)object;
            return err->message;
        }

        default:
            return pyro_sprintf_to_obj(vm, "<object>");
    }
}


PyroStr* pyro_stringify_f64(PyroVM* vm, double value, size_t precision) {
    char* array = pyro_sprintf(vm, "%.*f", precision, value);
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

    PyroStr* string = PyroStr_copy_raw(array, trimmed_length, vm);
    PYRO_FREE_ARRAY(vm, char, array, original_length + 1);
    if (!string) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    return string;
}


char* pyro_stringify_object_type(PyroObjectType type) {
    switch (type) {
        case PYRO_OBJECT_BOUND_METHOD: return "<method>";
        case PYRO_OBJECT_BUF: return "<buf>";
        case PYRO_OBJECT_CLASS: return "<class>";
        case PYRO_OBJECT_CLOSURE: return "<closure>";
        case PYRO_OBJECT_FILE: return "<file>";
        case PYRO_OBJECT_FN: return "<fn>";
        case PYRO_OBJECT_INSTANCE: return "<instance>";
        case PYRO_OBJECT_MAP: return "<map>";
        case PYRO_OBJECT_MAP_AS_SET: return "<set>";
        case PYRO_OBJECT_MAP_AS_WEAKREF: return "<weakref map>";
        case PYRO_OBJECT_MODULE: return "<module>";
        case PYRO_OBJECT_NATIVE_FN: return "<native fn>";
        case PYRO_OBJECT_STR: return "<str>";
        case PYRO_OBJECT_TUP: return "<tup>";
        case PYRO_OBJECT_UPVALUE: return "<upvalue>";
        case PYRO_OBJECT_VEC: return "<vec>";
        case PYRO_OBJECT_VEC_AS_STACK: return "<stack>";
        case PYRO_OBJECT_ITER: return "<iter>";
        case PYRO_OBJECT_QUEUE: return "<queue>";
        case PYRO_OBJECT_ERR: return "<err>";
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
        pyro_panic(vm, "invalid format string '%s'", format_string);
        return NULL;
    }

    char* array = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!array) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    va_start(args, format_string);
    vsprintf(array, format_string, args);
    va_end(args);

    return array;
}


PyroStr* pyro_sprintf_to_obj(PyroVM* vm, const char* format_string, ...) {
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
        pyro_panic(vm, "invalid format string '%s'", format_string);
        return NULL;
    }

    char* array = PYRO_ALLOCATE_ARRAY(vm, char, length + 1);
    if (!array) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    va_start(args, format_string);
    vsprintf(array, format_string, args);
    va_end(args);

    PyroStr* string = PyroStr_take(array, length, vm);
    if (!string) {
        PYRO_FREE_ARRAY(vm, char, array, length + 1);
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return string;
}


PyroStr* pyro_stringify_value(PyroVM* vm, PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_BOOL:
            return value.as.boolean ? vm->str_true : vm->str_false;

        case PYRO_VALUE_NULL:
            return vm->str_null;

        case PYRO_VALUE_I64:
            return pyro_sprintf_to_obj(vm, "%" PRId64, value.as.i64);

        case PYRO_VALUE_F64:
            return pyro_stringify_f64(vm, value.as.f64, 6);

        case PYRO_VALUE_CHAR: {
            char buffer[4];
            size_t count = pyro_write_utf8_codepoint(value.as.u32, (uint8_t*)buffer);
            PyroStr* string = PyroStr_copy_raw(buffer, count, vm);
            if (!string) {
                pyro_panic(vm, "out of memory");
                return NULL;
            }
            return string;
        }

        case PYRO_VALUE_OBJ:
            return stringify_object(vm, PYRO_AS_OBJ(value));

        default:
            return pyro_sprintf_to_obj(vm, "<value>");
    }
}


PyroStr* pyro_debugify_value(PyroVM* vm, PyroValue value) {
    PyroValue method = pyro_get_method(vm, value, vm->str_dollar_debug);
    if (!PYRO_IS_NULL(method)) {
        pyro_push(vm, value);
        PyroValue result = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!PYRO_IS_STR(result)) {
            pyro_panic(vm, "invalid return type for :$debug(), expected a string");
            return NULL;
        }
        return PYRO_AS_STR(result);
    }

    if (PYRO_IS_STR(value)) {
        PyroStr* string = make_debug_string_for_string(vm, PYRO_AS_STR(value));
        if (!string) {
            return NULL;
        }
        return string;
    }

    if (PYRO_IS_BUF(value)) {
        PyroStr* string = make_debug_string_for_buf(vm, PYRO_AS_BUF(value));
        if (!string) {
            return NULL;
        }
        return string;
    }

    if (PYRO_IS_CHAR(value)) {
        PyroStr* string = make_debug_string_for_char(vm, value);
        if (!string) {
            return NULL;
        }
        return string;
    }

    if (PYRO_IS_ERR(value)) {
        PyroErr* err = PYRO_AS_ERR(value);
        PyroStr* debug_message = make_debug_string_for_string(vm, err->message);
        if (!debug_message) {
            return NULL;
        }
        return pyro_sprintf_to_obj(vm, "<err %s>", debug_message->bytes);
    }

    if (PYRO_IS_F64(value)) {
        // 17 is the minimum number of significant digits that guarantees that any two distinct
        // IEEE-754 64-bit floats will have distinct representations when converted to decimal.
        return pyro_stringify_f64(vm, value.as.f64, 17);
    }

    return pyro_stringify_value(vm, value);
}


static PyroStr* format_i64(PyroVM* vm, PyroValue value, const char* format_string) {
    char buffer[24] = {'%'};
    size_t buffer_count = 1;

    size_t format_string_length = strlen(format_string);
    if (format_string_length > 16) {
        pyro_panic(vm, "format string is too long (max: 16)");
        return NULL;
    }

    memcpy(&buffer[1], format_string, format_string_length - 1);
    buffer_count += format_string_length - 1;

    switch (format_string[format_string_length - 1]) {
        case 'd':
            memcpy(&buffer[buffer_count], PRId64, strlen(PRId64));
            return pyro_sprintf_to_obj(vm, buffer, value.as.i64);

        case 'o':
            memcpy(&buffer[buffer_count], PRIo64, strlen(PRIo64));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u64);

        case 'x':
            memcpy(&buffer[buffer_count], PRIx64, strlen(PRIx64));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u64);

        case 'X':
            memcpy(&buffer[buffer_count], PRIX64, strlen(PRIX64));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u64);

        default:
            pyro_panic(vm, "format string '%s' is invalid for i64 value", format_string);
            return NULL;
    }
}


static PyroStr* format_char(PyroVM* vm, PyroValue value, const char* format_string) {
    char buffer[24] = {'%'};
    size_t buffer_count = 1;

    size_t format_string_length = strlen(format_string);
    if (format_string_length > 16) {
        pyro_panic(vm, "format string is too long (max: 16)");
        return NULL;
    }

    memcpy(&buffer[1], format_string, format_string_length - 1);
    buffer_count += format_string_length - 1;

    switch (format_string[format_string_length - 1]) {
        case 'd':
            memcpy(&buffer[buffer_count], PRIu32, strlen(PRIu32));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u32);

        case 'o':
            memcpy(&buffer[buffer_count], PRIo32, strlen(PRIo32));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u32);

        case 'x':
            memcpy(&buffer[buffer_count], PRIx32, strlen(PRIx32));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u32);

        case 'X':
            memcpy(&buffer[buffer_count], PRIX32, strlen(PRIX32));
            return pyro_sprintf_to_obj(vm, buffer, value.as.u32);

        default:
            pyro_panic(vm, "format string '%s' is invalid for char value", format_string);
            return NULL;
    }
}


static PyroStr* format_f64(PyroVM* vm, PyroValue value, const char* format_string) {
    char buffer[24] = {'%'};

    size_t format_string_length = strlen(format_string);
    if (format_string_length > 16) {
        pyro_panic(vm, "format string is too long (max: 16)");
        return NULL;
    }

    memcpy(&buffer[1], format_string, format_string_length);

    switch (format_string[format_string_length - 1]) {
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            return pyro_sprintf_to_obj(vm, buffer, value.as.f64);

        default:
            pyro_panic(vm, "format string '%s' is invalid for f64 value", format_string);
            return NULL;
    }
}


static PyroStr* format_str_obj(PyroVM* vm, PyroStr* string, const char* format_string) {
    char buffer[16] = {0};
    size_t buffer_count = 0;

    size_t format_string_length = strlen(format_string);
    if (format_string_length > 15) {
        pyro_panic(vm, "format string is too long (max: 15)");
        return NULL;
    }

    bool is_right_aligned = true;
    size_t index = 0;

    if (format_string[0] == '-') {
        is_right_aligned = false;
        index++;
    }

    while (index < format_string_length) {
        if (isdigit(format_string[index])) {
            buffer[buffer_count++] = format_string[index++];
        } else {
            pyro_panic(vm, "invalid format string '%s'", format_string);
            return NULL;
        }
    }

    if (buffer_count == 0) {
        pyro_panic(vm, "invalid format string '%s'", format_string);
        return NULL;
    }

    errno = 0;
    size_t target_length = (size_t)strtoll(buffer, NULL, 10);
    if (errno != 0) {
        pyro_panic(vm, "integer value in format string is out of range");
        return NULL;
    }

    if (string->length >= target_length) {
        return string;
    }

    char* array = PYRO_ALLOCATE_ARRAY(vm, char, target_length + 1);
    if (!array) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }
    size_t array_index = 0;

    size_t padding_length = target_length - string->length;
    if (is_right_aligned) {
        while (array_index < padding_length) {
            array[array_index] = ' ';
            array_index++;
        }
    }

    memcpy(&array[array_index], string->bytes, string->length);
    array_index += string->length;

    while (array_index < target_length) {
        array[array_index] = ' ';
        array_index++;
    }

    array[array_index] = '\0';

    PyroStr* result = PyroStr_take(array, target_length, vm);
    if (!result) {
        PYRO_FREE_ARRAY(vm, char, array, target_length + 1);
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    return result;
}


PyroStr* pyro_format_value(PyroVM* vm, PyroValue value, const char* format_string) {
    if (PYRO_IS_I64(value)) {
        if (format_string[0] == '%') {
            return pyro_sprintf_to_obj(vm, format_string, value.as.i64);
        }
        return format_i64(vm, value, format_string);
    }

    if (PYRO_IS_F64(value)) {
        if (format_string[0] == '%') {
            return pyro_sprintf_to_obj(vm, format_string, value.as.f64);
        }
        return format_f64(vm, value, format_string);
    }

    if (PYRO_IS_CHAR(value)) {
        if (format_string[0] == '%') {
            return pyro_sprintf_to_obj(vm, format_string, value.as.u32);
        }
        return format_char(vm, value, format_string);
    }

    if (PYRO_IS_STR(value)) {
        return format_str_obj(vm, PYRO_AS_STR(value), format_string);
    }

    PyroValue method = pyro_get_method(vm, value, vm->str_dollar_fmt);
    if (!PYRO_IS_NULL(method)) {
        PyroStr* format_string_object = PyroStr_new(format_string, vm);
        if (!format_string_object) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }
        pyro_push(vm, value);
        pyro_push(vm, pyro_obj(format_string_object));
        PyroValue result = pyro_call_method(vm, method, 1);
        if (vm->halt_flag) {
            return NULL;
        }
        if (!PYRO_IS_STR(result)) {
            pyro_panic(vm, "invalid return type for :$fmt(), expected a string");
            return NULL;
        }
        return PYRO_AS_STR(result);
    }

    pyro_panic(vm, "no handler for format specifier '%s'", format_string);
    return NULL;
}
