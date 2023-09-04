#include "../includes/pyro.h"


/* ------------------ */
/*  Binary Operators  */
/* ------------------ */


// Returns [left] + [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic and/or exit flags.
PyroValue pyro_op_binary_plus(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_i64(left.as.i64 + right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64((double)left.as.i64 + right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_f64(left.as.f64 + (double)right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64(left.as.f64 + right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            if (PYRO_IS_CHAR(right)) {
                PyroStr* result = PyroStr_concat_codepoints_as_utf8(left.as.u32, right.as.u32, vm);
                if (!result) {
                    pyro_panic(vm, "out of memory");
                    return pyro_null();
                }
                return pyro_obj(result);
            }
            if (PYRO_IS_STR(right)) {
                PyroStr* result = PyroStr_prepend_codepoint_as_utf8(PYRO_AS_STR(right), left.as.u32, vm);
                if (!result) {
                    pyro_panic(vm, "out of memory");
                    return pyro_null();
                }
                return pyro_obj(result);
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left)) {
                if (PYRO_IS_STR(right)) {
                    PyroStr* result = PyroStr_concat(PYRO_AS_STR(left), PYRO_AS_STR(right), vm);
                    if (!result) {
                        pyro_panic(vm, "out of memory");
                        return pyro_null();
                    }
                    return pyro_obj(result);
                }
                if (PYRO_IS_CHAR(right)) {
                    PyroStr* result = PyroStr_append_codepoint_as_utf8(PYRO_AS_STR(left), right.as.u32, vm);
                    if (!result) {
                        pyro_panic(vm, "out of memory");
                        return pyro_null();
                    }
                    return pyro_obj(result);
                }
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_plus);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_plus);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '+' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] - [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_minus(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_i64(left.as.i64 - right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64((double)left.as.i64 - right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_f64(left.as.f64 - (double)right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64(left.as.f64 - right.as.f64);
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_minus);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_minus);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '-' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] * [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_star(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_i64(left.as.i64 * right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64((double)left.as.i64 * right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_f64(left.as.f64 * (double)right.as.i64);
                case PYRO_VALUE_F64:
                    return pyro_f64(left.as.f64 * right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            if (PYRO_IS_I64(right) && right.as.i64 >= 0) {
                PyroStr* result = PyroStr_concat_n_codepoints_as_utf8(left.as.u32, right.as.i64, vm);
                if (!result) {
                    pyro_panic(vm, "out of memory");
                    return pyro_null();
                }
                return pyro_obj(result);
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_I64(right) && right.as.i64 >= 0) {
                PyroStr* result = PyroStr_concat_n_copies(PYRO_AS_STR(left), right.as.i64, vm);
                if (!result) {
                    pyro_panic(vm, "out of memory");
                    return pyro_null();
                }
                return pyro_obj(result);
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_star);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_star);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '*' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] / [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_slash(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64((double)left.as.i64 / (double)right.as.i64);
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64((double)left.as.i64 / right.as.f64);
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64(left.as.f64 / (double)right.as.i64);
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64(left.as.f64 / right.as.f64);
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_slash);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_slash);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '/' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] // [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_slash_slash(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    if (left.as.i64 == INT64_MIN && right.as.i64 == -1) {
                        pyro_panic(vm, "division [i64_min // -1] would result in integer overflow");
                        return pyro_null();
                    }
                    return pyro_i64(left.as.i64 / right.as.i64);
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64(trunc((double)left.as.i64 / right.as.f64));
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64(trunc(left.as.f64 / (double)right.as.i64));
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "division by zero");
                        return pyro_null();
                    }
                    return pyro_f64(trunc(left.as.f64 / right.as.f64));
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_slash_slash);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_slash_slash);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '//' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] | [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_bar(PyroVM* vm, PyroValue left, PyroValue right) {
    if (PYRO_IS_I64(left) && PYRO_IS_I64(right)) {
        return pyro_i64(left.as.i64 | right.as.i64);
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_bar);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_bar);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '|' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] & [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_amp(PyroVM* vm, PyroValue left, PyroValue right) {
    if (PYRO_IS_I64(left) && PYRO_IS_I64(right)) {
        return pyro_i64(left.as.i64 & right.as.i64);
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_amp);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_amp);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '&' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] ^ [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_caret(PyroVM* vm, PyroValue left, PyroValue right) {
    if (PYRO_IS_I64(left) && PYRO_IS_I64(right)) {
        return pyro_i64(left.as.i64 ^ right.as.i64);
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_caret);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_caret);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '^' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] % [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_percent(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "modulo by zero");
                        return pyro_null();
                    }
                    return pyro_i64(left.as.i64 % right.as.i64);
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "modulo by zero");
                        return pyro_null();
                    }
                    return pyro_f64(fmod((double)left.as.i64, right.as.f64));
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    if (right.as.i64 == 0) {
                        pyro_panic(vm, "modulo by zero");
                        return pyro_null();
                    }
                    return pyro_f64(fmod(left.as.f64, (double)right.as.i64));
                case PYRO_VALUE_F64:
                    if (right.as.f64 == 0.0) {
                        pyro_panic(vm, "modulo by zero");
                        return pyro_null();
                    }
                    return pyro_f64(fmod(left.as.f64, right.as.f64));
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_percent);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_percent);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '%%' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] ** [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_star_star(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_f64(pow((double)left.as.i64, (double)right.as.i64));
                case PYRO_VALUE_F64:
                    return pyro_f64(pow((double)left.as.i64, right.as.f64));
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_f64(pow(left.as.f64, (double)right.as.i64));
                case PYRO_VALUE_F64:
                    return pyro_f64(pow(left.as.f64, right.as.f64));
                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_star_star);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_star_star);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '**' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] << [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_less_less(PyroVM* vm, PyroValue left, PyroValue right) {
    if (PYRO_IS_I64(left) && PYRO_IS_I64(right)) {
        return pyro_i64(left.as.i64 << right.as.i64);
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_less_less);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_less_less);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '<<' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


// Returns [left] >> [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_greater_greater(PyroVM* vm, PyroValue left, PyroValue right) {
    if (PYRO_IS_I64(left) && PYRO_IS_I64(right)) {
        return pyro_i64(left.as.i64 >> right.as.i64);
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_greater_greater);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return pyro_null();
        if (!pyro_push(vm, right)) return pyro_null();
        return pyro_call_method(vm, left_method, 1);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_greater_greater);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return pyro_null();
        if (!pyro_push(vm, left)) return pyro_null();
        return pyro_call_method(vm, right_method, 1);
    }

    pyro_panic(vm,
        "invalid operand types for '>>' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return pyro_null();
}


/* ----------------- */
/*  Unary Operators  */
/* ----------------- */


PyroValue pyro_op_unary_plus(PyroVM* vm, PyroValue operand) {
    switch (operand.type) {
        case PYRO_VALUE_I64:
            return operand;

        case PYRO_VALUE_F64:
            return operand;

        default:
            break;
    }

    PyroValue method = pyro_get_method(vm, operand, vm->str_op_unary_plus);
    if (!PYRO_IS_NULL(method)) {
        if (!pyro_push(vm, operand)) return pyro_null();
        return pyro_call_method(vm, method, 0);
    }

    pyro_panic(vm,
        "invalid operand type for unary '+' operator: '%s'",
        pyro_get_type_name(vm, operand)->bytes
    );

    return pyro_null();
}


PyroValue pyro_op_unary_minus(PyroVM* vm, PyroValue operand) {
    switch (operand.type) {
        case PYRO_VALUE_I64:
            return pyro_i64(-operand.as.i64);

        case PYRO_VALUE_F64:
            return pyro_f64(-operand.as.f64);

        default:
            break;
    }

    PyroValue method = pyro_get_method(vm, operand, vm->str_op_unary_minus);
    if (!PYRO_IS_NULL(method)) {
        if (!pyro_push(vm, operand)) return pyro_null();
        return pyro_call_method(vm, method, 0);
    }

    pyro_panic(vm,
        "invalid operand type for unary '-' operator: '%s'",
        pyro_get_type_name(vm, operand)->bytes
    );

    return pyro_null();
}


PyroValue pyro_op_unary_tilde(PyroVM* vm, PyroValue operand) {
    if (PYRO_IS_I64(operand)) {
        return pyro_i64(~operand.as.i64);
    }

    PyroValue method = pyro_get_method(vm, operand, vm->str_op_unary_tilde);
    if (!PYRO_IS_NULL(method)) {
        if (!pyro_push(vm, operand)) return pyro_null();
        return pyro_call_method(vm, method, 0);
    }

    pyro_panic(vm,
        "invalid operand type for unary '~' operator: '%s'",
        pyro_get_type_name(vm, operand)->bytes
    );

    return pyro_null();
}


/* ---------------------- */
/*  Comparison Operators  */
/* ---------------------- */


// Performs a lexicographic comparison using byte values.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
static int compare_strings(PyroStr* a, PyroStr* b) {
    if (a == b) {
        return 0;
    }

    size_t min_len = a->count < b->count ? a->count : b->count;

    for (size_t i = 0; i < min_len; i++) {
        if (a->bytes[i] < b->bytes[i]) return -1;
        if (a->bytes[i] > b->bytes[i]) return 1;
    }

    return a->count < b->count ? -1 : 1;
}


// Performs a lexicographic comparison using element values.
// Can call into Pyro code and set the panic or exit flags.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
static int compare_tuples(PyroVM* vm, PyroTup* a, PyroTup* b) {
    size_t min_len = a->count < b->count ? a->count : b->count;

    for (size_t i = 0; i < min_len; i++) {
        if (pyro_op_compare_lt(vm, a->values[i], b->values[i])) {
            return -1;
        }
        if (vm->halt_flag) {
            return 0;
        }
        if (pyro_op_compare_gt(vm, a->values[i], b->values[i])) {
            return 1;
        }
        if (vm->halt_flag) {
            return 0;
        }
    }

    if (a->count == b->count) {
        return 0;
    }

    return a->count < b->count ? -1 : 1;
}


// Comparing integers and floats is tricky. We can't just cast the i64 to a double (which is what
// C does by default) without losing precision. Only integers in the range [-(2^53), 2^53] can be
// represented exactly as 64-bit floats. Outside this range, multiple integer values convert to the
// same floating-point value, e.g.
//
//   2^53      -->  9007199254740992.0
//   2^53 + 1  -->  9007199254740992.0
//
// First we check if the float is outside the range [INT64_MIN, INT64_MAX].
// - If so, its value is higher/lower than any possible integer.
// - If not, then its integer part can be exactly represented as a 64-bit integer so we examine
//   the integer and fractional parts separately.
//
// Returns 0 if a == b.
// Returns -1 if a < b.
// Returns 1 if a > b.
// Returns 2 if b is NaN.
static int pyro_compare_int_and_float(int64_t a, double b) {
    if (isnan(b)) {
        return 2;
    }

    // 2^63. INT64_MAX is 2^63 - 1 --- as a double this gets rounded to 2^63.
    if (b >= 9223372036854775808.0) {
        return -1;
    }

    // -(2^63). INT64_MIN is -(2^63).
    if (b < -9223372036854775808.0) {
        return 1;
    }

    double b_whole_part = b >= 0.0 ? floor(b) : ceil(b);
    double b_fract_part = b - b_whole_part;

    if ((int64_t)b_whole_part == a) {
        if (b_fract_part == 0.0) {
            return 0;
        }
        return a > 0 ? -1 : 1;
    } else {
        return a < (int64_t)b_whole_part ? -1 : 1;
    }
}


// Returns true if [left] == [right].
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_eq(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return left.as.i64 == right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float(left.as.i64, right.as.f64) == 0;
                case PYRO_VALUE_CHAR:
                    return left.as.i64 == (int64_t)right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return (int64_t)left.as.u32 == right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float((int64_t)left.as.u32, right.as.f64) == 0;
                case PYRO_VALUE_CHAR:
                    return left.as.u32 == right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_compare_int_and_float(right.as.i64, left.as.f64) == 0;
                case PYRO_VALUE_F64:
                    return left.as.f64 == right.as.f64;
                case PYRO_VALUE_CHAR:
                    return pyro_compare_int_and_float((int64_t)right.as.u32, left.as.f64) == 0;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_BOOL: {
            if (PYRO_IS_BOOL(right)) {
                return left.as.boolean == right.as.boolean;
            }
            break;
        }

        case PYRO_VALUE_NULL: {
            if (PYRO_IS_NULL(right)) {
                return true;
            }
            break;
        }

        case PYRO_VALUE_TOMBSTONE: {
            return PYRO_IS_TOMBSTONE(right);
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_STR(right)) {
                return left.as.obj == right.as.obj;
            }

            if (PYRO_IS_TUP(left) && PYRO_IS_TUP(right)) {
                return PyroTup_check_equal(PYRO_AS_TUP(left), PYRO_AS_TUP(right), vm);
            }

            PyroValue method = pyro_get_method(vm, left, vm->str_op_binary_equals_equals);
            if (!PYRO_IS_NULL(method)) {
                if (!pyro_push(vm, left)) return false;
                if (!pyro_push(vm, right)) return false;
                PyroValue result = pyro_call_method(vm, method, 1);
                if (vm->halt_flag) {
                    return false;
                }
                return pyro_is_truthy(result);
            }

            break;
        }
    }

    if (PYRO_IS_OBJ(right)) {
        PyroValue method = pyro_get_method(vm, right, vm->str_op_binary_equals_equals);

        if (!PYRO_IS_NULL(method)) {
            if (!pyro_push(vm, right)) return false;
            if (!pyro_push(vm, left)) return false;
            PyroValue result = pyro_call_method(vm, method, 1);
            if (vm->halt_flag) {
                return false;
            }
            return pyro_is_truthy(result);
        }

        if (PYRO_IS_OBJ(left)) {
            return left.as.obj == right.as.obj;
        }
    }

    return false;
}


// Returns true if [left] < [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_lt(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return left.as.i64 < right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float(left.as.i64, right.as.f64) == -1;
                case PYRO_VALUE_CHAR:
                    return left.as.i64 < (int64_t)right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return (int64_t)left.as.u32 < right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float((int64_t)left.as.u32, right.as.f64) == -1;
                case PYRO_VALUE_CHAR:
                    return left.as.u32 < right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_compare_int_and_float(right.as.i64, left.as.f64) == 1;
                case PYRO_VALUE_F64:
                    return left.as.f64 < right.as.f64;
                case PYRO_VALUE_CHAR:
                    return pyro_compare_int_and_float((int64_t)right.as.u32, left.as.f64) == 1;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_STR(right)) {
                return compare_strings(PYRO_AS_STR(left), PYRO_AS_STR(right)) == -1;
            }
            if (PYRO_IS_TUP(left) && PYRO_IS_TUP(right)) {
                return compare_tuples(vm, PYRO_AS_TUP(left), PYRO_AS_TUP(right)) == -1;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_less);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return false;
        if (!pyro_push(vm, right)) return false;
        PyroValue result = pyro_call_method(vm, left_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_less);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return false;
        if (!pyro_push(vm, left)) return false;
        PyroValue result = pyro_call_method(vm, right_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    pyro_panic(vm,
        "invalid operand types for '<' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return false;
}


// Returns true if [left] <= [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_le(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return left.as.i64 <= right.as.i64;
                case PYRO_VALUE_F64: {
                    int result = pyro_compare_int_and_float(left.as.i64, right.as.f64);
                    return result == -1 || result == 0;
                }
                case PYRO_VALUE_CHAR:
                    return left.as.i64 <= (int64_t)right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return (int64_t)left.as.u32 <= right.as.i64;
                case PYRO_VALUE_F64: {
                    int result = pyro_compare_int_and_float((int64_t)left.as.u32, right.as.f64);
                    return result == -1 || result == 0;
                }
                case PYRO_VALUE_CHAR:
                    return left.as.u32 <= right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64: {
                    int result = pyro_compare_int_and_float(right.as.i64, left.as.f64);
                    return result == 0 || result == 1;
                }
                case PYRO_VALUE_F64:
                    return left.as.f64 <= right.as.f64;
                case PYRO_VALUE_CHAR: {
                    int result = pyro_compare_int_and_float((int64_t)right.as.u32, left.as.f64);
                    return result == 0 || result == 1;
                }
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_STR(right)) {
                return compare_strings(PYRO_AS_STR(left), PYRO_AS_STR(right)) <= 0;
            }
            if (PYRO_IS_TUP(left) && PYRO_IS_TUP(right)) {
                return compare_tuples(vm, PYRO_AS_TUP(left), PYRO_AS_TUP(right)) <= 0;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_less_equals);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return false;
        if (!pyro_push(vm, right)) return false;
        PyroValue result = pyro_call_method(vm, left_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_less_equals);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return false;
        if (!pyro_push(vm, left)) return false;
        PyroValue result = pyro_call_method(vm, right_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    pyro_panic(vm,
        "invalid operand types for '<=' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return false;
}


// Returns true if [left] > [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_gt(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return left.as.i64 > right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float(left.as.i64, right.as.f64) == 1;
                case PYRO_VALUE_CHAR:
                    return left.as.i64 > (int64_t)right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return (int64_t)left.as.u32 > right.as.i64;
                case PYRO_VALUE_F64:
                    return pyro_compare_int_and_float((int64_t)left.as.u32, right.as.f64) == 1;
                case PYRO_VALUE_CHAR:
                    return left.as.u32 > right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return pyro_compare_int_and_float(right.as.i64, left.as.f64) == -1;
                case PYRO_VALUE_F64:
                    return left.as.f64 > right.as.f64;
                case PYRO_VALUE_CHAR:
                    return pyro_compare_int_and_float((int64_t)right.as.u32, left.as.f64) == -1;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_STR(right)) {
                return compare_strings(PYRO_AS_STR(left), PYRO_AS_STR(right)) == 1;
            }
            if (PYRO_IS_TUP(left) && PYRO_IS_TUP(right)) {
                return compare_tuples(vm, PYRO_AS_TUP(left), PYRO_AS_TUP(right)) == 1;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_greater);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return false;
        if (!pyro_push(vm, right)) return false;
        PyroValue result = pyro_call_method(vm, left_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_greater);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return false;
        if (!pyro_push(vm, left)) return false;
        PyroValue result = pyro_call_method(vm, right_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    pyro_panic(vm,
        "invalid operand types for '>' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return false;
}


// Returns true if [left] >= [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_ge(PyroVM* vm, PyroValue left, PyroValue right) {
    switch (left.type) {
        case PYRO_VALUE_I64: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return left.as.i64 >= right.as.i64;
                case PYRO_VALUE_F64: {
                    int result = pyro_compare_int_and_float(left.as.i64, right.as.f64);
                    return result == 1 || result == 0;
                }
                case PYRO_VALUE_CHAR:
                    return left.as.i64 >= (int64_t)right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_CHAR: {
            switch (right.type) {
                case PYRO_VALUE_I64:
                    return (int64_t)left.as.u32 >= right.as.i64;
                case PYRO_VALUE_F64: {
                    int result = pyro_compare_int_and_float((int64_t)left.as.u32, right.as.f64);
                    return result == 1 || result == 0;
                }
                case PYRO_VALUE_CHAR:
                    return left.as.u32 >= right.as.u32;
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_F64: {
            switch (right.type) {
                case PYRO_VALUE_I64: {
                    int result = pyro_compare_int_and_float(right.as.i64, left.as.f64);
                    return result == 0 || result == -1;
                }
                case PYRO_VALUE_F64:
                    return left.as.f64 >= right.as.f64;
                case PYRO_VALUE_CHAR: {
                    int result = pyro_compare_int_and_float((int64_t)right.as.u32, left.as.f64);
                    return result == 0 || result == -1;
                }
                default:
                    break;
            }
            break;
        }

        case PYRO_VALUE_OBJ: {
            if (PYRO_IS_STR(left) && PYRO_IS_STR(right)) {
                return compare_strings(PYRO_AS_STR(left), PYRO_AS_STR(right)) >= 0;
            }
            if (PYRO_IS_TUP(left) && PYRO_IS_TUP(right)) {
                return compare_tuples(vm, PYRO_AS_TUP(left), PYRO_AS_TUP(right)) >= 0;
            }
            break;
        }

        default:
            break;
    }

    PyroValue left_method = pyro_get_method(vm, left, vm->str_op_binary_greater_equals);
    if (!PYRO_IS_NULL(left_method)) {
        if (!pyro_push(vm, left)) return false;
        if (!pyro_push(vm, right)) return false;
        PyroValue result = pyro_call_method(vm, left_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    PyroValue right_method = pyro_get_method(vm, right, vm->str_rop_binary_greater_equals);
    if (!PYRO_IS_NULL(right_method)) {
        if (!pyro_push(vm, right)) return false;
        if (!pyro_push(vm, left)) return false;
        PyroValue result = pyro_call_method(vm, right_method, 1);
        if (vm->halt_flag) {
            return false;
        }
        return pyro_is_truthy(result);
    }

    pyro_panic(vm,
        "invalid operand types for '>=' operator: '%s' and '%s'",
        pyro_get_type_name(vm, left)->bytes,
        pyro_get_type_name(vm, right)->bytes
    );

    return false;
}


/* ----------------- */
/*  Index Operators  */
/* ----------------- */


PyroValue pyro_op_get_index(PyroVM* vm, PyroValue receiver, PyroValue key) {
    if (!PYRO_IS_OBJ(receiver)) {
        pyro_panic(vm, "value does not support [] indexing");
        return pyro_null();
    }

    switch (PYRO_AS_OBJ(receiver)->type) {
        case PYRO_OBJECT_MAP: {
            PyroMap* map = PYRO_AS_MAP(receiver);
            PyroValue value;
            if (PyroMap_get(map, key, &value, vm)) {
                return value;
            }
            return pyro_obj(vm->error);
        }

        case PYRO_OBJECT_STR: {
            PyroStr* str = PYRO_AS_STR(receiver);
            if (PYRO_IS_I64(key)) {
                int64_t index = key.as.i64;
                if (index >= 0 && (size_t)index < str->count) {
                    PyroStr* new_str = PyroStr_copy(&str->bytes[index], 1, false, vm);
                    if (!new_str) {
                        pyro_panic(vm, "out of memory");
                        return pyro_null();
                    }
                    return pyro_obj(new_str);
                }
                pyro_panic(vm, "index is out of range");
                return pyro_null();
            }
            pyro_panic(vm, "invalid index type, expected an integer");
            return pyro_null();
        }

        case PYRO_OBJECT_VEC: {
            PyroVec* vec = PYRO_AS_VEC(receiver);
            if (PYRO_IS_I64(key)) {
                int64_t index = key.as.i64;
                if (index >= 0 && (size_t)index < vec->count) {
                    return vec->values[index];
                }
                pyro_panic(vm, "index is out of range");
                return pyro_null();
            }
            pyro_panic(vm, "invalid index type, expected an integer");
            return pyro_null();
        }

        case PYRO_OBJECT_TUP: {
            PyroTup* tup = PYRO_AS_TUP(receiver);
            if (PYRO_IS_I64(key)) {
                int64_t index = key.as.i64;
                if (index >= 0 && (size_t)index < tup->count) {
                    return tup->values[index];
                }
                pyro_panic(vm, "index is out of range");
                return pyro_null();
            }
            pyro_panic(vm, "invalid index type, expected an integer");
            return pyro_null();
        }

        default: {
            PyroValue method = pyro_get_method(vm, receiver, vm->str_dollar_get);
            if (!PYRO_IS_NULL(method)) {
                if (!pyro_push(vm, receiver)) return pyro_null();
                if (!pyro_push(vm, key)) return pyro_null();
                return pyro_call_method(vm, method, 1);
            }
            pyro_panic(vm, "object does not support [] indexing");
            return pyro_null();
        }
    }
}


PyroValue pyro_op_set_index(PyroVM* vm, PyroValue receiver, PyroValue key, PyroValue value) {
    if (!PYRO_IS_OBJ(receiver)) {
        pyro_panic(vm, "value does not support [] indexing");
        return pyro_null();
    }

    switch (PYRO_AS_OBJ(receiver)->type) {
        case PYRO_OBJECT_MAP: {
            PyroMap* map = PYRO_AS_MAP(receiver);
            if (!PyroMap_set(map, key, value, vm)) {
                pyro_panic(vm, "out of memory");
            }
            return value;
        }

        case PYRO_OBJECT_VEC: {
            PyroVec* vec = PYRO_AS_VEC(receiver);
            if (PYRO_IS_I64(key)) {
                int64_t index = key.as.i64;
                if (index >= 0 && (size_t)index < vec->count) {
                    vec->values[index] = value;
                    return value;
                }
                pyro_panic(vm, "index is out of range");
                return pyro_null();
            }
            pyro_panic(vm, "invalid index type, expected an integer");
            return pyro_null();
        }

        default: {
            PyroValue method = pyro_get_method(vm, receiver, vm->str_dollar_set);
            if (!PYRO_IS_NULL(method)) {
                if (!pyro_push(vm, receiver)) return pyro_null();
                if (!pyro_push(vm, key)) return pyro_null();
                if (!pyro_push(vm, value)) return pyro_null();
                return pyro_call_method(vm, method, 2);
            }
            pyro_panic(vm, "object does not support [] index assignment");
            return pyro_null();
        }
    }
}