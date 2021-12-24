#include "values.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "utf8.h"
#include "exec.h"
#include "panics.h"


bool pyro_is_truthy(Value value) {
    switch (value.type) {
        case VAL_NULL:
            return false;
        case VAL_BOOL:
            return value.as.boolean;
        case VAL_I64:
            return value.as.i64 != 0;
        case VAL_F64:
            return value.as.f64 != 0.0;
        case VAL_OBJ:
            switch (value.as.obj->type) {
                case OBJ_TUP_AS_ERR: return false;
                default: return true;
            }
        case VAL_CHAR:
            return value.as.u32 != 0;
        default:
            return true;
    }
}


ObjClass* pyro_get_class(Value value) {
    if (IS_OBJ(value)) {
        return AS_OBJ(value)->class;
    }
    return NULL;
}


Value pyro_get_method(PyroVM* vm, Value receiver, ObjStr* method_name) {
    if (IS_OBJ(receiver) && AS_OBJ(receiver)->class) {
        Value method;
        if (ObjMap_get(AS_OBJ(receiver)->class->methods, OBJ_VAL(method_name), &method, vm)) {
            return method;
        }
    }
    return NULL_VAL();
}


bool pyro_has_method(PyroVM* vm, Value receiver, ObjStr* method_name) {
    return !IS_NULL(pyro_get_method(vm, receiver, method_name));
}


bool pyro_compare_eq_strict(Value a, Value b) {
    if (a.type == b.type) {
        switch (a.type) {
            case VAL_BOOL:
                return a.as.boolean == b.as.boolean;
            case VAL_I64:
                return a.as.i64 == b.as.i64;
            case VAL_F64:
                return a.as.f64 == b.as.f64;
            case VAL_CHAR:
                return a.as.u32 == b.as.u32;
            case VAL_OBJ:
                return a.as.obj == b.as.obj;
            case VAL_NULL:
                return true;
            case VAL_TOMBSTONE:
                return true;
            case VAL_EMPTY:
                return true;
        }
    }
    return false;
}


// Values which compare as equal should also hash as equal. This is why we cast floats that compare
// equal to an integer to that integer first.
uint64_t pyro_hash_value(PyroVM* vm, Value value) {
    switch (value.type) {
        case VAL_NULL:
            return 123;

        case VAL_BOOL:
            return value.as.boolean ? 456 : 789;

        case VAL_I64:
            return value.as.u64;

        case VAL_CHAR:
            return value.as.u32;

        case VAL_F64:
            if (value.as.f64 >= -9223372036854775808.0    // -2^63 == I64_MIN
                && value.as.f64 < 9223372036854775808.0   // 2^63 == I64_MAX + 1
                && floor(value.as.f64) == value.as.f64    // is a whole number
            ) {
                return (uint64_t)(int64_t)value.as.f64;
            } else {
                return value.as.u64;
            }

        case VAL_OBJ:
            switch (value.as.obj->type) {
                case OBJ_STR:
                    return AS_STR(value)->hash;
                case OBJ_TUP:
                    return ObjTup_hash(vm, AS_TUP(value));
                case OBJ_TUP_AS_ERR:
                    return ObjTup_hash(vm, AS_TUP(value));
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, value, vm->str_hash);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, value);
                        Value result = pyro_call_method(vm, method, 0);
                        if (vm->halt_flag) {
                            return 0;
                        }
                        return result.as.u64;
                    }
                    return (uint64_t)value.as.obj;
                }
                default:
                    return (uint64_t)value.as.obj;
            }

        default:
            return 0;
    }
}


static void pyro_dump_object(PyroVM* vm, Obj* object) {
    switch (object->type) {
        case OBJ_STR: {
            ObjStr* string = (ObjStr*)object;
            pyro_out(vm, "\"%s\"", string->bytes);
            break;
        }

        case OBJ_BOUND_METHOD:
            pyro_out(vm, "<method>");
            break;

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
                pyro_out(vm, "<fn_obj>");
            } else {
                pyro_out(vm, "<fn_obj %s>", fn->name->bytes);
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

        case OBJ_MAP_AS_SET: {
            pyro_out(vm, "<set>");
            break;
        }

        case OBJ_MODULE: {
            pyro_out(vm, "<module>");
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            pyro_out(vm, "<fn_nat %s>", native->name->bytes);
            break;
        }

        case OBJ_TUP_AS_ERR:
            pyro_out(vm, "<err>");
            break;

        case OBJ_TUP:
            pyro_out(vm, "<tup>");
            break;

        case OBJ_UPVALUE:
            pyro_out(vm, "<upvalue>");
            break;

        case OBJ_VEC:
            pyro_out(vm, "<vec>");
            break;

        case OBJ_VEC_AS_STACK:
            pyro_out(vm, "<stack>");
            break;

        case OBJ_FILE:
            pyro_out(vm, "<file>");
            break;

        case OBJ_BUF:
            pyro_out(vm, "<buf>");
            break;

        case OBJ_QUEUE:
            pyro_out(vm, "<queue>");
            break;

        case OBJ_ITER:
            pyro_out(vm, "<iter>");
            break;

        case OBJ_MAP_AS_WEAKREF:
            pyro_out(vm, "<weakref map>");
            break;

        default:
            pyro_out(vm, "<object>");
            break;
    }
}


void pyro_dump_value(PyroVM* vm, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            pyro_out(vm, "%s", value.as.boolean ? "true" : "false");
            break;

        case VAL_NULL:
            pyro_out(vm, "null");
            break;

        case VAL_I64:
            pyro_out(vm, "%lld", value.as.i64);
            break;

        case VAL_F64:
            pyro_out(vm, "%.2f", value.as.f64);
            break;

        case VAL_OBJ:
            pyro_dump_object(vm, AS_OBJ(value));
            break;

        default:
            pyro_out(vm, "<value>");
            break;
    }
}
