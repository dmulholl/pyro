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

    ObjStr* string = pyro_stringify_value(vm, value);
    if (!string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }
    return string;
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





