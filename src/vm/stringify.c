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






