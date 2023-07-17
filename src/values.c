#include "../inc/pyro.h"


PyroClass* pyro_get_class(PyroVM* vm, PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_CHAR:
            return vm->class_char;
        case PYRO_VALUE_OBJ:
            return PYRO_AS_OBJ(value)->class;
        default:
            return NULL;
    }
}


PyroValue pyro_get_method(PyroVM* vm, PyroValue receiver, PyroStr* method_name) {
    if (PYRO_IS_CLASS(receiver)) {
        PyroValue method;
        if (PyroMap_get(PYRO_AS_CLASS(receiver)->static_methods, pyro_obj(method_name), &method, vm)) {
            return method;
        }
        return pyro_null();
    }

    PyroClass* class = pyro_get_class(vm, receiver);
    if (class) {
        if (class->all_instance_methods_cached_name == method_name) {
            return class->all_instance_methods_cached_value;
        }
        PyroValue method;
        if (PyroMap_get(class->all_instance_methods, pyro_obj(method_name), &method, vm)) {
            class->all_instance_methods_cached_name = method_name;
            class->all_instance_methods_cached_value = method;
            return method;
        }
    }

    return pyro_null();
}


PyroValue pyro_get_pub_method(PyroVM* vm, PyroValue receiver, PyroStr* method_name) {
    if (PYRO_IS_CLASS(receiver)) {
        PyroValue method;
        if (PyroMap_get(PYRO_AS_CLASS(receiver)->static_methods, pyro_obj(method_name), &method, vm)) {
            return method;
        }
        return pyro_null();
    }

    PyroClass* class = pyro_get_class(vm, receiver);
    if (class) {
        if (class->pub_instance_methods_cached_name == method_name) {
            return class->pub_instance_methods_cached_value;
        }
        PyroValue method;
        if (PyroMap_get(class->pub_instance_methods, pyro_obj(method_name), &method, vm)) {
            class->pub_instance_methods_cached_name = method_name;
            class->pub_instance_methods_cached_value = method;
            return method;
        }
    }

    return pyro_null();
}


bool pyro_has_method(PyroVM* vm, PyroValue receiver, PyroStr* method_name) {
    return !PYRO_IS_NULL(pyro_get_method(vm, receiver, method_name));
}


bool pyro_has_pub_method(PyroVM* vm, PyroValue receiver, PyroStr* method_name) {
    return !PYRO_IS_NULL(pyro_get_pub_method(vm, receiver, method_name));
}


bool pyro_compare_eq_strict(PyroValue a, PyroValue b) {
    if (a.type == b.type) {
        switch (a.type) {
            case PYRO_VALUE_BOOL:
                return a.as.boolean == b.as.boolean;
            case PYRO_VALUE_I64:
                return a.as.i64 == b.as.i64;
            case PYRO_VALUE_F64:
                return a.as.f64 == b.as.f64;
            case PYRO_VALUE_CHAR:
                return a.as.u32 == b.as.u32;
            case PYRO_VALUE_OBJ:
                return a.as.obj == b.as.obj;
            case PYRO_VALUE_NULL:
                return true;
            case PYRO_VALUE_TOMBSTONE:
                return true;
        }
    }
    return false;
}


inline static bool is_numerically_equal_to_i64(double value) {
    if (value >= -9223372036854775808.0    // -2^63 == I64_MIN
        && value < 9223372036854775808.0   // 2^63 == I64_MAX + 1
        && floor(value) == value           // is a whole number
    ) {
        return true;
    }
    return false;
}


// All builtin types follow the rule that values that compare as equal also hash as equal.
uint64_t pyro_hash_value(PyroVM* vm, PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_NULL:
            return 123;

        case PYRO_VALUE_BOOL:
            return value.as.boolean ? 456 : 789;

        case PYRO_VALUE_I64:
            return value.as.u64;

        case PYRO_VALUE_CHAR:
            return (uint64_t)value.as.u32;

        case PYRO_VALUE_F64: {
            if (is_numerically_equal_to_i64(value.as.f64)) {
                return (uint64_t)(int64_t)value.as.f64;
            }
            if (isinf(value.as.f64)) {
                return 123456789;
            }
            if (isnan(value.as.f64)) {
                return 0;
            }
            return value.as.u64;
        }

        case PYRO_VALUE_OBJ:
            switch (value.as.obj->type) {
                case PYRO_OBJECT_STR:
                    return PYRO_AS_STR(value)->hash;

                case PYRO_OBJECT_TUP: {
                    uint64_t hash = 0;
                    PyroTup* tup = PYRO_AS_TUP(value);

                    for (size_t i = 0; i < tup->count; i++) {
                        hash ^= pyro_hash_value(vm, tup->values[i]);
                    }

                    return hash;
                }

                case PYRO_OBJECT_MAP_AS_SET: {
                    uint64_t hash = 0;
                    PyroMap* map = PYRO_AS_MAP(value);

                    for (size_t i = 0; i < map->entry_array_count; i++) {
                        PyroMapEntry* entry = &map->entry_array[i];
                        if (PYRO_IS_TOMBSTONE(entry->key)) {
                            continue;
                        }
                        hash ^= pyro_hash_value(vm, entry->key);
                    }

                    return hash;
                }

                default: {
                    PyroValue method = pyro_get_method(vm, value, vm->str_dollar_hash);
                    if (!PYRO_IS_NULL(method)) {
                        if (!pyro_push(vm, value)) return 0;
                        PyroValue result = pyro_call_method(vm, method, 0);
                        if (vm->halt_flag) {
                            return 0;
                        }
                        return result.as.u64;
                    }
                    return (uint64_t)value.as.obj;
                }
            }

        default:
            return 0;
    }
}


static void pyro_dump_object(PyroVM* vm, PyroObject* object) {
    switch (object->type) {
        case PYRO_OBJECT_STR: {
            PyroStr* string = (PyroStr*)object;
            pyro_stdout_write_f(vm, "\"%s\"", string->bytes);
            break;
        }

        case PYRO_OBJECT_BOUND_METHOD:
            pyro_stdout_write(vm, "<method>");
            break;

        case PYRO_OBJECT_CLASS: {
            PyroClass* class = (PyroClass*)object;
            if (class->name == NULL) {
                pyro_stdout_write(vm, "<class>");
            } else {
                pyro_stdout_write_f(vm, "<class %s>", class->name->bytes);
            }
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroClosure* closure = (PyroClosure*)object;
            if (closure->fn->name == NULL) {
                pyro_stdout_write(vm, "<fn>");
            } else {
                pyro_stdout_write_f(vm, "<fn %s>", closure->fn->name->bytes);
            }
            break;
        }

        case PYRO_OBJECT_FN: {
            PyroFn* fn = (PyroFn*)object;
            if (fn->name == NULL) {
                pyro_stdout_write(vm, "<fn_obj>");
            } else {
                pyro_stdout_write_f(vm, "<fn_obj %s>", fn->name->bytes);
            }
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroInstance* instance = (PyroInstance*)object;
            pyro_stdout_write_f(vm, "<instance %s>", instance->obj.class->name->bytes);
            break;
        }

        case PYRO_OBJECT_MAP: {
            pyro_stdout_write(vm, "<map>");
            break;
        }

        case PYRO_OBJECT_MAP_AS_SET: {
            pyro_stdout_write(vm, "<set>");
            break;
        }

        case PYRO_OBJECT_MODULE: {
            pyro_stdout_write(vm, "<module>");
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            PyroNativeFn* native = (PyroNativeFn*)object;
            pyro_stdout_write_f(vm, "<fn_nat %s>", native->name->bytes);
            break;
        }

        case PYRO_OBJECT_ERR:
            pyro_stdout_write(vm, "<err>");
            break;

        case PYRO_OBJECT_TUP:
            pyro_stdout_write(vm, "<tup>");
            break;

        case PYRO_OBJECT_UPVALUE:
            pyro_stdout_write(vm, "<upvalue>");
            break;

        case PYRO_OBJECT_VEC:
            pyro_stdout_write(vm, "<vec>");
            break;

        case PYRO_OBJECT_VEC_AS_STACK:
            pyro_stdout_write(vm, "<stack>");
            break;

        case PYRO_OBJECT_FILE:
            pyro_stdout_write(vm, "<file>");
            break;

        case PYRO_OBJECT_BUF:
            pyro_stdout_write(vm, "<buf>");
            break;

        case PYRO_OBJECT_QUEUE:
            pyro_stdout_write(vm, "<queue>");
            break;

        case PYRO_OBJECT_ITER:
            pyro_stdout_write(vm, "<iter>");
            break;

        case PYRO_OBJECT_MAP_AS_WEAKREF:
            pyro_stdout_write(vm, "<weakref map>");
            break;

        default:
            pyro_stdout_write(vm, "<object>");
            break;
    }
}


void pyro_dump_value(PyroVM* vm, PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_BOOL:
            pyro_stdout_write_f(vm, "%s", value.as.boolean ? "true" : "false");
            break;

        case PYRO_VALUE_NULL:
            pyro_stdout_write(vm, "null");
            break;

        case PYRO_VALUE_I64:
            pyro_stdout_write_f(vm, "%lld", value.as.i64);
            break;

        case PYRO_VALUE_F64:
            pyro_stdout_write_f(vm, "%.2f", value.as.f64);
            break;

        case PYRO_VALUE_OBJ:
            pyro_dump_object(vm, PYRO_AS_OBJ(value));
            break;

        default:
            pyro_stdout_write(vm, "<value>");
            break;
    }
}


PyroStr* pyro_get_type_name(PyroVM* vm, PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_BOOL:
            return vm->str_bool;

        case PYRO_VALUE_NULL:
            return vm->str_null;

        case PYRO_VALUE_I64:
            return vm->str_i64;

        case PYRO_VALUE_F64:
            return vm->str_f64;

        case PYRO_VALUE_CHAR:
            return vm->str_char;

        case PYRO_VALUE_OBJ: {
            switch (PYRO_AS_OBJ(value)->type) {
                case PYRO_OBJECT_BOUND_METHOD:
                    return vm->str_method;

                case PYRO_OBJECT_BUF:
                    return vm->str_buf;

                case PYRO_OBJECT_CLASS:
                    return vm->str_class;

                case PYRO_OBJECT_INSTANCE: {
                    PyroStr* class_name = PYRO_AS_OBJ(value)->class->name;
                    if (class_name) {
                        return class_name;
                    }
                    return vm->str_instance;
                }

                case PYRO_OBJECT_CLOSURE:
                case PYRO_OBJECT_NATIVE_FN:
                case PYRO_OBJECT_FN:
                    return vm->str_func;

                case PYRO_OBJECT_FILE:
                    return vm->str_file;

                case PYRO_OBJECT_ITER:
                    return vm->str_iter;

                case PYRO_OBJECT_MAP:
                    return vm->str_map;

                case PYRO_OBJECT_MAP_AS_SET:
                    return vm->str_set;

                case PYRO_OBJECT_VEC:
                    return vm->str_vec;

                case PYRO_OBJECT_VEC_AS_STACK:
                    return vm->str_stack;

                case PYRO_OBJECT_QUEUE:
                    return vm->str_queue;

                case PYRO_OBJECT_STR:
                    return vm->str_str;

                case PYRO_OBJECT_MODULE:
                    return vm->str_module;

                case PYRO_OBJECT_TUP:
                    return vm->str_tup;

                case PYRO_OBJECT_ERR:
                    return vm->str_err;

                default:
                    return vm->empty_string;
            }
        }

        default:
            return vm->empty_string;
    }
}
