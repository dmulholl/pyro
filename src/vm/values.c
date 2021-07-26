#include "values.h"
#include "heap.h"
#include "vm.h"
#include "utils.h"
#include "utf8.h"


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
                case OBJ_ERR: return false;
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


Value pyro_get_method(Value receiver, ObjStr* method_name) {
    if (IS_OBJ(receiver) && AS_OBJ(receiver)->class) {
        Value method;
        if (ObjMap_get(AS_OBJ(receiver)->class->methods, OBJ_VAL(method_name), &method)) {
            return method;
        }
    }
    return NULL_VAL();
}


bool pyro_has_method(Value receiver, ObjStr* method_name) {
    return !IS_NULL(pyro_get_method(receiver, method_name));
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
                if (a.as.obj->type == OBJ_ERR && b.as.obj->type == OBJ_ERR) {
                    return ObjTup_check_equal(AS_TUP(a), AS_TUP(b));
                }
                return a.as.obj == b.as.obj;
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
                case OBJ_ERR: return ObjTup_hash(AS_TUP(value));
                default: return (uint64_t)value.as.obj;
            }
        case VAL_CHAR:
            return value.as.u32;
        default:
            return 0;
    }
}


ObjStr* pyro_stringify_object(PyroVM* vm, Obj* object) {
    Value method;
    if (object->class && ObjMap_get(object->class->methods, OBJ_VAL(vm->str_str), &method)) {
        pyro_push(vm, OBJ_VAL(object));
        Value stringified = pyro_call_method(vm, method, 0);
        if (vm->halt_flag) {
            return NULL;
        }
        if (IS_STR(stringified)) {
            return AS_STR(stringified);
        }
        pyro_panic(vm, "Invalid type returned by $str() method.");
        return NULL;
    }

    switch (object->type) {
        case OBJ_STR: return (ObjStr*)object;
        case OBJ_MODULE: return STR_OBJ("<module>");
        case OBJ_UPVALUE: return STR_OBJ("<upvalue>");
        case OBJ_MAP: return STR_OBJ("<map>");
        case OBJ_FILE: return STR_OBJ("<file>");
        case OBJ_FN: return STR_OBJ("<fn>");
        case OBJ_BOUND_METHOD: return STR_OBJ("<method>");
        case OBJ_STR_ITER: return STR_OBJ("<iterator>");
        case OBJ_MAP_ITER: return STR_OBJ("<iterator>");
        case OBJ_VEC_ITER: return STR_OBJ("<iterator>");
        case OBJ_TUP_ITER: return STR_OBJ("<iterator>");

        case OBJ_ERR:
        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            return ObjTup_stringify(tup, vm);
        }

        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            return ObjVec_stringify(vec, vm);
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)object;
            return ObjStr_copy_raw((char*)buf->bytes, buf->count, vm);
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            char* array = pyro_str_fmt(vm, "<fn %s>", native->name->bytes);
            if (!array) {
                return NULL;
            }

            ObjStr* string = ObjStr_take(array, strlen(array), vm);
            if (!string) {
                FREE_ARRAY(vm, char, array, strlen(array) + 1);
                return NULL;
            }

            return string;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            if (closure->fn->name == NULL) {
                return STR_OBJ("<fn>");
            }

            char* array = pyro_str_fmt(vm, "<fn %s>", closure->fn->name->bytes);
            if (!array) {
                return NULL;
            }

            ObjStr* string = ObjStr_take(array, strlen(array), vm);
            if (!string) {
                FREE_ARRAY(vm, char, array, strlen(array) + 1);
                return NULL;
            }

            return string;
        }

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            if (class->name == NULL) {
                return STR_OBJ("<class>");
            }

            char* array = pyro_str_fmt(vm, "<class %s>", class->name->bytes);
            if (!array) {
                return NULL;
            }

            ObjStr* string = ObjStr_take(array, strlen(array), vm);
            if (!string) {
                FREE_ARRAY(vm, char, array, strlen(array) + 1);
                return NULL;
            }

            return string;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            if (instance->obj.class->name == NULL) {
                return STR_OBJ("<instance>");
            }

            char* array = pyro_str_fmt(vm, "<instance %s>", instance->obj.class->name->bytes);
            if (!array) {
                return NULL;
            }

            ObjStr* string = ObjStr_take(array, strlen(array), vm);
            if (!string) {
                FREE_ARRAY(vm, char, array, strlen(array) + 1);
                return NULL;
            }

            return string;
        }

        default:
            return STR_OBJ("<object>");
    }
}


ObjStr* pyro_stringify_value(PyroVM* vm, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            return value.as.boolean ? vm->str_true : vm->str_false;

        case VAL_NULL:
            return vm->str_null;

        case VAL_I64: {
            char* array = pyro_str_fmt(vm, "%lld", value.as.i64);
            if (!array) {
                return NULL;
            }

            ObjStr* string = ObjStr_take(array, strlen(array), vm);
            if (!string) {
                FREE_ARRAY(vm, char, array, strlen(array) + 1);
                return NULL;
            }

            return string;
        }

        case VAL_F64: {
            char* array = pyro_str_fmt(vm, "%.6f", value.as.f64);
            if (!array) {
                return NULL;
            }

            size_t orig_len = strlen(array);
            size_t trim_len = orig_len;

            while (array[trim_len - 1] == '0') {
                trim_len--;
            }
            if (array[trim_len - 1] == '.') {
                trim_len++;
            }

            ObjStr* string = ObjStr_copy_raw(array, trim_len, vm);
            FREE_ARRAY(vm, char, array, orig_len + 1);
            return string;
        }

        case VAL_OBJ:
            return pyro_stringify_object(vm, AS_OBJ(value));

        case VAL_CHAR: {
            char buffer[4];
            size_t count = pyro_write_utf8_codepoint(value.as.u32, (uint8_t*)buffer);
            return ObjStr_copy_raw(buffer, count, vm);
        }

        default:
            return STR_OBJ("<value>");
    }
}


void pyro_dump_object(PyroVM* vm, Obj* object) {
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
                pyro_out(vm, "<closure>");
            } else {
                pyro_out(vm, "<closure %s>", closure->fn->name->bytes);
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

        case OBJ_MODULE: {
            pyro_out(vm, "<module>");
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            pyro_out(vm, "<fn nat %s>", native->name->bytes);
            break;
        }

        case OBJ_ERR:
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

        case OBJ_FILE:
            pyro_out(vm, "<file>");
            break;

        case OBJ_BUF:
            pyro_out(vm, "<buf>");
            break;

        case OBJ_MAP_ITER:
        case OBJ_TUP_ITER:
        case OBJ_VEC_ITER:
        case OBJ_STR_ITER:
            pyro_out(vm, "<iter>");
            break;

        case OBJ_WEAKREF_MAP:
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


ObjStr* pyro_format_value(PyroVM* vm, Value value, const char* format) {
    if (IS_I64(value)) {
        char* array = pyro_str_fmt(vm, format, value.as.i64);
        if (array == NULL) {
            pyro_panic(vm, "Error applying format specifier {%s}.", format);
            return NULL;
        }

        ObjStr* string = ObjStr_take(array, strlen(array), vm);
        if (!string) {
            FREE_ARRAY(vm, char, array, strlen(array) + 1);
            return NULL;
        }

        return string;
    }

    if (IS_F64(value)) {
        char* array = pyro_str_fmt(vm, format, value.as.f64);
        if (array == NULL) {
            pyro_panic(vm, "Error applying format specifier {%s}.", format);
            return NULL;
        }

        ObjStr* string = ObjStr_take(array, strlen(array), vm);
        if (!string) {
            FREE_ARRAY(vm, char, array, strlen(array) + 1);
            return NULL;
        }

        return string;
    }

    Value fmt_method = pyro_get_method(value, vm->str_fmt);
    if (!IS_NULL(fmt_method)) {
        pyro_push(vm, value);
        pyro_push(vm, OBJ_VAL(ObjStr_copy_raw(format, strlen(format), vm)));
        Value result = pyro_call_method(vm, fmt_method, 1);

        if (vm->halt_flag) {
            return NULL;
        }

        if (IS_STR(result)) {
            return AS_STR(result);
        }

        pyro_panic(vm, "Invalid type returned by $fmt() method.");
        return NULL;
    }

    pyro_panic(vm, "No handler for format specifier {%s}.", format);
    return NULL;
}


char* pyro_stringify_obj_type(ObjType type) {
    switch (type) {
        case OBJ_BOUND_METHOD: return "<method>";
        case OBJ_BUF: return "<buf>";
        case OBJ_CLASS: return "<class>";
        case OBJ_CLOSURE: return "<closure>";
        case OBJ_ERR: return "<err>";
        case OBJ_FILE: return "<file>";
        case OBJ_FN: return "<fn>";
        case OBJ_INSTANCE: return "<instance>";
        case OBJ_MAP: return "<map>";
        case OBJ_MAP_ITER: return "<map iter>";
        case OBJ_MODULE: return "<module>";
        case OBJ_NATIVE_FN: return "<native fn>";
        case OBJ_RANGE: return "<range>";
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


int pyro_compare_strings(ObjStr* a, ObjStr* b) {
    if (a == b) {
        return 0;
    }

    size_t min_len = a->length < b->length ? a->length : b->length;

    for (size_t i = 0; i < min_len; i++) {
        if (a->bytes[i] < b->bytes[i]) return -1;
        if (a->bytes[i] > b->bytes[i]) return 1;
    }

    return a->length < b->length ? -1 : 1;
}


int pyro_compare(Value a, Value b) {
    if (a.type == b.type) {
        switch (a.type) {
            case VAL_I64:
                if (a.as.i64 == b.as.i64) return 0;
                return a.as.i64 < b.as.i64 ? -1 : 1;
            case VAL_F64:
                if (a.as.f64 == b.as.f64) return 0;
                return a.as.f64 < b.as.f64 ? -1 : 1;
            case VAL_CHAR:
                if (a.as.u32 == b.as.u32) return 0;
                return a.as.u32 < b.as.u32 ? -1 : 1;
            case VAL_OBJ:
                if (IS_STR(a) && IS_STR(b)) {
                    return pyro_compare_strings(AS_STR(a), AS_STR(b));
                } else {
                    return 2;
                }
            default:
                return 2;
        }
    } else {
        return 2;
    }
}


ObjStr* pyro_char_to_debug_str(PyroVM* vm, Value value) {
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

    bool result;

    if (count == 1) {
        switch (utf8_buffer[0]) {
            case '\'':
                result = ObjBuf_append_bytes(buf, 2, (uint8_t*)"\\\'", vm);
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
                result = ObjBuf_append_byte(buf, utf8_buffer[0], vm);
                break;
        }
    } else {
        result = ObjBuf_append_bytes(buf, count, utf8_buffer, vm);
    }

    if (!result) {
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
