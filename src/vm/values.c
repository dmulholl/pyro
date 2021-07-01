#include "values.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "utf8.h"


// Returns the value's class if it exists, otherwise NULL;
ObjClass* pyro_get_class(Value value) {
    if ((IS_OBJ(value) || IS_ERR(value)) && AS_OBJ(value)->class) {
        return AS_OBJ(value)->class;
    }
    return NULL;
}


// Returns the method if it exists, otherwise NULL_VAL().
Value pyro_get_method(PyroVM* vm, Value receiver, ObjStr* method_name) {
    if ((IS_OBJ(receiver) || IS_ERR(receiver)) && AS_OBJ(receiver)->class) {
        Value method;
        if (ObjMap_get(AS_OBJ(receiver)->class->methods, OBJ_VAL(method_name), &method)) {
            return method;
        }
    }
    return NULL_VAL();
}


bool pyro_has_method(PyroVM* vm, Value receiver, ObjStr* method_name) {
    return !IS_NULL(pyro_get_method(vm, receiver, method_name));
}


bool pyro_check_equal(Value a, Value b) {
    if (a.type == b.type) {
        switch (a.type) {
            case VAL_BOOL:
                return a.as.boolean == b.as.boolean;
            case VAL_NULL:
                return true;
            case VAL_I64:
                return a.as.i64 == b.as.i64;
            case VAL_F64:
                return a.as.f64 == b.as.f64;
            case VAL_OBJ:
                if (a.as.obj->type == OBJ_TUP && b.as.obj->type == OBJ_TUP) {
                    return ObjTup_check_equal(AS_TUP(a), AS_TUP(b));
                }
                return a.as.obj == b.as.obj;
            case VAL_ERR:
                return ObjTup_check_equal(AS_TUP(a), AS_TUP(b));
            case VAL_TOMBSTONE:
                return true;
            case VAL_EMPTY:
                return true;
            case VAL_CHAR:
                return a.as.u32 == b.as.u32;
        }
    }
    return false;
}


uint64_t pyro_hash(Value value) {
    switch (value.type) {
        case VAL_NULL:
            return 123;
        case VAL_BOOL:
            return value.as.boolean ? 456 : 789;
        case VAL_I64:
            return value.as.u64;
        case VAL_F64:
            return value.as.u64;
        case VAL_OBJ:
            switch (value.as.obj->type) {
                case OBJ_STR: return AS_STR(value)->hash;
                case OBJ_TUP: return ObjTup_hash(AS_TUP(value));
                default: return (uint64_t)value.as.obj;
            }
        case VAL_CHAR:
            return value.as.u32;
        case VAL_ERR:
            return ObjTup_hash(AS_TUP(value));
        default:
            return 0;
    }
}


ObjStr* pyro_stringify_object(PyroVM* vm, Obj* object, bool call_method) {
    if (call_method && object->class) {
        Value method;
        if (ObjMap_get(object->class->methods, OBJ_VAL(vm->str_str), &method)) {
            pyro_push(vm, OBJ_VAL(object));
            Value stringified = pyro_call_method(vm, method, 0);
            if (vm->panic_flag || vm->exit_flag) {
                return vm->empty_string;
            }
            if (IS_STR(stringified)) {
                return AS_STR(stringified);
            }
            pyro_panic(vm, "Invalid type returned by $str() method.");
            return vm->empty_string;
        }
    }

    switch (object->type) {
        case OBJ_STR: {
            return (ObjStr*)object;
        }

        case OBJ_STR_ITER: {
            return STR_OBJ("<iterator>");
        }

        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            ObjStr* name;
            if (bound->method->type == OBJ_CLOSURE) {
                name = ((ObjClosure*)bound->method)->fn->name;
            } else {
                name = ((ObjNativeFn*)bound->method)->name;
            }
            char* string = pyro_str_fmt(vm, "<method %s>", name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            if (class->name == NULL) {
                return STR_OBJ("<class>");
            }
            char* string = pyro_str_fmt(vm, "<class %s>", class->name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (closure->fn->name == NULL) {
                return STR_OBJ("<fn>");
            }
            char* string = pyro_str_fmt(vm, "<fn %s>", closure->fn->name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_FN: {
            ObjFn* fn = (ObjFn*)object;
            if (fn->name == NULL) {
                return STR_OBJ("<fn>");
            }
            char* string = pyro_str_fmt(vm, "<fn %s>", fn->name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            char* string = pyro_str_fmt(vm, "<instance %s>", instance->obj.class->name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_MAP: {
            char* string = pyro_str_fmt(vm, "<map %p>", object);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_MAP_ITER: {
            return STR_OBJ("<iterator>");
        }

        case OBJ_MODULE: {
            char* string = pyro_str_fmt(vm, "<module %p>", object);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            char* string = pyro_str_fmt(vm, "<fn %s>", native->name->bytes);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            char* string = pyro_str_fmt(vm, "<%d-tuple>", tup->count);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_TUP_ITER: {
            return STR_OBJ("<iterator>");
        }

        case OBJ_UPVALUE: {
            return STR_OBJ("<upvalue>");
        }

        case OBJ_VEC: {
            char* string = pyro_str_fmt(vm, "<vec %p>", object);
            return ObjStr_take(string, strlen(string), vm);
        }

        case OBJ_VEC_ITER: {
            return STR_OBJ("<iterator>");
        }

        case OBJ_WEAKREF_MAP: {
            return STR_OBJ("<weakref map>");
        }

        default: {
            char* string = pyro_str_fmt(vm, "<object %p>", object);
            return ObjStr_take(string, strlen(string), vm);
        }
    }
}


// 1 - This function allocates memory and can trigger the GC.
// 2 - This function assumes that any object passed to it has been fully initialized.
// 3 - This function can call into Pyro code.
// 4 - This function can trigger a panic or an exit.
ObjStr* pyro_stringify_value(PyroVM* vm, Value value, bool call_method) {
    switch (value.type) {
        case VAL_BOOL:
            return value.as.boolean ? vm->str_true : vm->str_false;

        case VAL_NULL:
            return vm->str_null;

        case VAL_ERR:
            return STR_OBJ("<error>");

        case VAL_I64: {
            char* string = pyro_str_fmt(vm, "%lld", value.as.i64);
            return ObjStr_take(string, strlen(string), vm);
        }

        case VAL_F64: {
            char* formatted = pyro_str_fmt(vm, "%.6f", value.as.f64);
            size_t orig_len = strlen(formatted);
            size_t trim_len = orig_len;

            while (formatted[trim_len - 1] == '0') {
                trim_len--;
            }
            if (formatted[trim_len - 1] == '.') {
                trim_len++;
            }

            ObjStr* string = ObjStr_copy_raw(formatted, trim_len, vm);
            FREE_ARRAY(vm, char, formatted, orig_len + 1);
            return string;
        }

        case VAL_OBJ:
            return pyro_stringify_object(vm, AS_OBJ(value), call_method);

        case VAL_CHAR: {
            char buffer[4];
            size_t count = pyro_write_utf8_codepoint(value.as.u32, (uint8_t*)buffer);
            return ObjStr_copy_raw(buffer, count, vm);
        }

        default:
            return STR_OBJ("<value>");
    }
}



void pyro_debug_object(PyroVM* vm, Obj* object) {
    switch (object->type) {

        case OBJ_STR: {
            ObjStr* string = (ObjStr*)object;
            pyro_out(vm, "\"%s\"", string->bytes);
            break;
        }

        case OBJ_STR_ITER: {
            pyro_out(vm, "<iter>");
            break;
        }

        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            if (bound->method->type == OBJ_CLOSURE) {
                ObjClosure* closure = (ObjClosure*)bound->method;
                if (closure->fn->name == NULL) {
                    pyro_out(vm, "<method>");
                } else {
                    pyro_out(vm, "<method %s>", closure->fn->name->bytes);
                }
            } else {
                ObjNativeFn* native = (ObjNativeFn*)bound->method;
                pyro_out(vm, "<method %s>", native->name->bytes);
            }
            break;
        }

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            if (class->name == NULL) {
                pyro_out(vm, "<class>");
            } else {
                pyro_out(vm, "<class %s>", class->name->bytes);
            }
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (closure->fn->name == NULL) {
                pyro_out(vm, "<fn>");
            } else {
                pyro_out(vm, "<fn %s>", closure->fn->name->bytes);
            }
            break;
        }

        case OBJ_FN: {
            ObjFn* fn = (ObjFn*)object;
            if (fn->name == NULL) {
                pyro_out(vm, "<fn>");
            } else {
                pyro_out(vm, "<fn %s>", fn->name->bytes);
            }
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            pyro_out(vm, "<instance %s>", instance->obj.class->name->bytes);
            break;
        }

        case OBJ_MAP: {
            pyro_out(vm, "<map>");
            break;
        }

        case OBJ_MAP_ITER: {
            pyro_out(vm, "<iter>");
            break;
        }

        case OBJ_MODULE: {
            pyro_out(vm, "<module>");
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            pyro_out(vm, "<fn nat %s>", native->name->bytes);
            break;
        }

        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            pyro_out(vm, "<%d-tuple>", tup->count);
            break;
        }

        case OBJ_TUP_ITER: {
            pyro_out(vm, "<iter>");
            break;
        }

        case OBJ_UPVALUE: {
            pyro_out(vm, "<upvalue>");
            break;
        }

        case OBJ_VEC: {
            pyro_out(vm, "<vec>");
            break;
        }

        case OBJ_VEC_ITER: {
            pyro_out(vm, "<iter>");
            break;
        }

        case OBJ_WEAKREF_MAP: {
            pyro_out(vm, "<weakref map>");
            break;
        }

        default: {
            pyro_out(vm, "<object>");
            break;
        }
    }
}


void pyro_debug_value(PyroVM* vm, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            pyro_out(vm, "%s", value.as.boolean ? "true" : "false");
            break;

        case VAL_NULL:
            pyro_out(vm, "null");
            break;

        case VAL_ERR:
            pyro_out(vm, "<error>");
            break;

        case VAL_I64:
            pyro_out(vm, "%lld", value.as.i64);
            break;

        case VAL_F64:
            pyro_out(vm, "%.2f", value.as.f64);
            break;

        case VAL_OBJ:
            pyro_debug_object(vm, AS_OBJ(value));
            break;

        default:
            pyro_out(vm, "<value>");
            break;
    }
}


ObjStr* pyro_format_value(PyroVM* vm, Value value, const char* format) {
    if (IS_I64(value)) {
        char* formatted = pyro_str_fmt(vm, format, value.as.i64);
        if (formatted == NULL) {
            pyro_panic(vm, "Error applying format specifier {%s}.", format);
            return vm->empty_string;
        }
        return ObjStr_take(formatted, strlen(formatted), vm);
    }

    if (IS_F64(value)) {
        char* formatted = pyro_str_fmt(vm, format, value.as.f64);
        if (formatted == NULL) {
            pyro_panic(vm, "Error applying format specifier {%s}.", format);
            return vm->empty_string;
        }
        return ObjStr_take(formatted, strlen(formatted), vm);
    }

    Value fmt_method = pyro_get_method(vm, value, vm->str_fmt);
    if (!IS_NULL(fmt_method)) {
        pyro_push(vm, value);
        pyro_push(vm, OBJ_VAL(ObjStr_copy_raw(format, strlen(format), vm)));
        Value formatted = pyro_call_method(vm, fmt_method, 1);
        if (vm->exit_flag || vm->panic_flag) {
            return vm->empty_string;
        }
        if (IS_STR(formatted)) {
            return AS_STR(formatted);
        }
        pyro_panic(vm, "Invalid type returned by $fmt() method.");
        return vm->empty_string;
    }

    pyro_panic(vm, "No handler for format specifier {%s}.", format);
    return vm->empty_string;
}


// Returns a pointer to a static string. Can be safely called from the GC.
char* pyro_stringify_obj_type(ObjType type) {
    switch (type) {
        case OBJ_BOUND_METHOD: return "<method>";
        case OBJ_CLASS: return "<class>";
        case OBJ_CLOSURE: return "<closure>";
        case OBJ_FN: return "<fn>";
        case OBJ_INSTANCE: return "<instance>";
        case OBJ_MAP: return "<map>";
        case OBJ_MAP_ITER: return "<map iter>";
        case OBJ_MODULE: return "<module>";
        case OBJ_NATIVE_FN: return "<native fn>";
        case OBJ_STR: return "<str>";
        case OBJ_STR_ITER: return "<str iter>";
        case OBJ_TUP: return "<tup>";
        case OBJ_TUP_ITER: return "<tup iter>";
        case OBJ_UPVALUE: return "<upvalue>";
        case OBJ_VEC: return "<vec>";
        case OBJ_VEC_ITER: return "<vec iter>";
        case OBJ_WEAKREF_MAP: return "<weakref map>";
    }
}

