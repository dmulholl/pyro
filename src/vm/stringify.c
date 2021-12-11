#include "stringify.h"
#include "utils.h"
#include "vm.h"
#include "exec.h"
#include "heap.h"


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
        ObjStr* string = ObjStr_debug_str(AS_STR(value), vm);
        if (!string) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL;
        }
        return string;
    }

    if (IS_CHAR(value)) {
        ObjStr* string = pyro_char_to_debug_str(vm, value);
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





