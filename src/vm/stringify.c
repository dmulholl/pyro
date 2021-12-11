#include "stringify.h"
#include "utils.h"
#include "vm.h"
#include "exec.h"


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





