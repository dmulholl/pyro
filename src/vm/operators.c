#include "operators.h"
#include "objects.h"
#include "vm.h"


// Returns [a] + [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_plus(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return I64_VAL(a.as.i64 + b.as.i64);
                case VAL_F64:
                    return F64_VAL((double)a.as.i64 + b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                    return NULL_VAL();
            }
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return F64_VAL(a.as.f64 + (double)b.as.i64);
                case VAL_F64:
                    return F64_VAL(a.as.f64 + b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                    return NULL_VAL();
            }
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_STR: {
                    if (IS_STR(b)) {
                        vm->gc_disallows++;
                        ObjStr* result = ObjStr_concat(AS_STR(a), AS_STR(b), vm);
                        vm->gc_disallows--;
                        if (!result) {
                            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                            return NULL_VAL();
                        }
                        return OBJ_VAL(result);
                    } else if (IS_CHAR(b)) {
                        vm->gc_disallows++;
                        ObjStr* result = ObjStr_append_codepoint_as_utf8(AS_STR(a), b.as.u32, vm);
                        vm->gc_disallows--;
                        if (!result) {
                            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                            return NULL_VAL();
                        }
                        return OBJ_VAL(result);
                    } else {
                        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                        return NULL_VAL();
                    }
                }

                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_plus);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        return result;
                    } else {
                        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                        return NULL_VAL();
                    }
                }

                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                    return NULL_VAL();
            }
        }

        case VAL_CHAR: {
            if (IS_CHAR(b)) {
                ObjStr* result = ObjStr_concat_codepoints_as_utf8(a.as.u32, b.as.u32, vm);
                if (!result) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    return NULL_VAL();
                }
                return OBJ_VAL(result);
            } else if (IS_STR(b)) {
                vm->gc_disallows++;
                ObjStr* result = ObjStr_prepend_codepoint_as_utf8(AS_STR(b), a.as.u32, vm);
                vm->gc_disallows--;
                if (!result) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    return NULL_VAL();
                }
                return OBJ_VAL(result);
            } else {
                pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
                return NULL_VAL();
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '+'.");
            return NULL_VAL();
    }
}


// Returns [a] - [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_minus(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return I64_VAL(a.as.i64 - b.as.i64);
                case VAL_F64:
                    return F64_VAL((double)a.as.i64 - b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '-'.");
                    return NULL_VAL();
            }
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return F64_VAL(a.as.f64 - (double)b.as.i64);
                case VAL_F64:
                    return F64_VAL(a.as.f64 - b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '-'.");
                    return NULL_VAL();
            }
        }

        case VAL_OBJ: {
            if (IS_INSTANCE(a)) {
                Value method = pyro_get_method(vm, a, vm->str_op_binary_minus);
                if (!IS_NULL(method)) {
                    pyro_push(vm, a);
                    pyro_push(vm, b);
                    Value result = pyro_call_method(vm, method, 1);
                    return result;
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '-'.");
                    return NULL_VAL();
                }
            } else {
                pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '-'.");
                return NULL_VAL();
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '-'.");
            return NULL_VAL();
    }
}


// Returns [a] * [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_star(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return I64_VAL(a.as.i64 * b.as.i64);
                case VAL_F64:
                    return F64_VAL((double)a.as.i64 * b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                    return NULL_VAL();
            }
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return F64_VAL(a.as.f64 * (double)b.as.i64);
                case VAL_F64:
                    return F64_VAL(a.as.f64 * b.as.f64);
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                    return NULL_VAL();
            }
        }

        case VAL_OBJ: {
            if (IS_STR(a)) {
                if (IS_I64(b) && b.as.i64 >= 0) {
                    vm->gc_disallows++;
                    ObjStr* result = ObjStr_concat_n_copies(AS_STR(a), b.as.i64, vm);
                    vm->gc_disallows--;
                    if (!result) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return NULL_VAL();
                    }
                    return OBJ_VAL(result);
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                    return NULL_VAL();
                }
            } else if (IS_INSTANCE(a)) {
                Value method = pyro_get_method(vm, a, vm->str_op_binary_star);
                if (!IS_NULL(method)) {
                    pyro_push(vm, a);
                    pyro_push(vm, b);
                    Value result = pyro_call_method(vm, method, 1);
                    return result;
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                    return NULL_VAL();
                }
            } else {
                pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                return NULL_VAL();
            }
        }

        case VAL_CHAR: {
            if (IS_I64(b) && b.as.i64 >= 0) {
                ObjStr* result = ObjStr_concat_n_codepoints_as_utf8(a.as.u32, b.as.i64, vm);
                if (!result) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    return NULL_VAL();
                }
                return OBJ_VAL(result);
            } else {
                pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
                return NULL_VAL();
            }
        }

        default: {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid operand types to '*'.");
            return NULL_VAL();
        }
    }
}


/* ------------- */
/*  Comparisons  */
/* ------------- */


// Performs a lexicographic comparison using byte values.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
static int compare_strings(ObjStr* a, ObjStr* b) {
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


// Can call into Pyro code and set the panic or exit flags.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
static int compare_tuples(PyroVM* vm, ObjTup* a, ObjTup* b) {
    if (ObjTup_check_equal(a, b, vm)) {
        return 0;
    }
    if (vm->halt_flag) {
        return 0;
    }

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


// Returns true if [a] == [b].
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_eq(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return a.as.i64 == b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float(a.as.i64, b.as.f64) == 0;
                case VAL_CHAR:
                    return a.as.i64 == (int64_t)b.as.u32;
                default:
                    return false;
            }
            break;
        }

        case VAL_CHAR: {
            switch (b.type) {
                case VAL_I64:
                    return (int64_t)a.as.u32 == b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float((int64_t)a.as.u32, b.as.f64) == 0;
                case VAL_CHAR:
                    return a.as.u32 == b.as.u32;
                default:
                    return false;
            }
            break;
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return pyro_compare_int_and_float(b.as.i64, a.as.f64) == 0;
                case VAL_F64:
                    return a.as.f64 == b.as.f64;
                case VAL_CHAR:
                    return pyro_compare_int_and_float((int64_t)b.as.u32, a.as.f64) == 0;
                default:
                    return false;
            }
            break;
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_TUP:
                    return IS_TUP(b) && ObjTup_check_equal(AS_TUP(a), AS_TUP(b), vm);
                case OBJ_TUP_AS_ERR:
                    return IS_ERR(b) && ObjTup_check_equal(AS_TUP(a), AS_TUP(b), vm);
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_equals_equals);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        if (vm->halt_flag) {
                            return false;
                        }
                        return pyro_is_truthy(result);
                    }
                    return a.as.obj == b.as.obj;
                }
                default:
                    return a.as.obj == b.as.obj;
            }
        }

        case VAL_BOOL:
            return IS_BOOL(b) && a.as.boolean == b.as.boolean;

        case VAL_NULL:
            return IS_NULL(b);

        case VAL_TOMBSTONE:
            return IS_TOMBSTONE(b);

        case VAL_EMPTY:
            return IS_EMPTY(b);
    }
}


// Returns true if [a] < [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_lt(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return a.as.i64 < b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float(a.as.i64, b.as.f64) == -1;
                case VAL_CHAR:
                    return a.as.i64 < (int64_t)b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_CHAR: {
            switch (b.type) {
                case VAL_I64:
                    return (int64_t)a.as.u32 < b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float((int64_t)a.as.u32, b.as.f64) == -1;
                case VAL_CHAR:
                    return a.as.u32 < b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return pyro_compare_int_and_float(b.as.i64, a.as.f64) == 1;
                case VAL_F64:
                    return a.as.f64 < b.as.f64;
                case VAL_CHAR:
                    return pyro_compare_int_and_float((int64_t)b.as.u32, a.as.f64) == 1;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_STR: {
                    if (IS_STR(b)) {
                        return compare_strings(AS_STR(a), AS_STR(b)) == -1;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_TUP: {
                    if (IS_TUP(b)) {
                        return compare_tuples(vm, AS_TUP(a), AS_TUP(b)) == -1;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_less);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        if (vm->halt_flag) {
                            return false;
                        }
                        return pyro_is_truthy(result);
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
            return false;
    }
}


// Returns true if [a] <= [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_le(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return a.as.i64 <= b.as.i64;
                case VAL_F64: {
                    int result = pyro_compare_int_and_float(a.as.i64, b.as.f64);
                    return result == -1 || result == 0;
                }
                case VAL_CHAR:
                    return a.as.i64 <= (int64_t)b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_CHAR: {
            switch (b.type) {
                case VAL_I64:
                    return (int64_t)a.as.u32 <= b.as.i64;
                case VAL_F64: {
                    int result = pyro_compare_int_and_float((int64_t)a.as.u32, b.as.f64);
                    return result == -1 || result == 0;
                }
                case VAL_CHAR:
                    return a.as.u32 <= b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64: {
                    int result = pyro_compare_int_and_float(b.as.i64, a.as.f64);
                    return result == 0 || result == 1;
                }
                case VAL_F64:
                    return a.as.f64 <= b.as.f64;
                case VAL_CHAR: {
                    int result = pyro_compare_int_and_float((int64_t)b.as.u32, a.as.f64);
                    return result == 0 || result == 1;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_STR: {
                    if (IS_STR(b)) {
                        return compare_strings(AS_STR(a), AS_STR(b)) <= 0;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_TUP: {
                    if (IS_TUP(b)) {
                        return compare_tuples(vm, AS_TUP(a), AS_TUP(b)) <= 0;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_less_equals);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        if (vm->halt_flag) {
                            return false;
                        }
                        return pyro_is_truthy(result);
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
            return false;
    }
}


// Returns true if [a] > [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_gt(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return a.as.i64 > b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float(a.as.i64, b.as.f64) == 1;
                case VAL_CHAR:
                    return a.as.i64 > (int64_t)b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_CHAR: {
            switch (b.type) {
                case VAL_I64:
                    return (int64_t)a.as.u32 > b.as.i64;
                case VAL_F64:
                    return pyro_compare_int_and_float((int64_t)a.as.u32, b.as.f64) == 1;
                case VAL_CHAR:
                    return a.as.u32 > b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64:
                    return pyro_compare_int_and_float(b.as.i64, a.as.f64) == -1;
                case VAL_F64:
                    return a.as.f64 > b.as.f64;
                case VAL_CHAR:
                    return pyro_compare_int_and_float((int64_t)b.as.u32, a.as.f64) == -1;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_STR: {
                    if (IS_STR(b)) {
                        return compare_strings(AS_STR(a), AS_STR(b)) == 1;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_TUP: {
                    if (IS_TUP(b)) {
                        return compare_tuples(vm, AS_TUP(a), AS_TUP(b)) == 1;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_greater);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        if (vm->halt_flag) {
                            return false;
                        }
                        return pyro_is_truthy(result);
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
            return false;
    }
}


// Returns true if [a] >= [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_ge(PyroVM* vm, Value a, Value b) {
    switch (a.type) {
        case VAL_I64: {
            switch (b.type) {
                case VAL_I64:
                    return a.as.i64 >= b.as.i64;
                case VAL_F64: {
                    int result = pyro_compare_int_and_float(a.as.i64, b.as.f64);
                    return result == 1 || result == 0;
                }
                case VAL_CHAR:
                    return a.as.i64 >= (int64_t)b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_CHAR: {
            switch (b.type) {
                case VAL_I64:
                    return (int64_t)a.as.u32 >= b.as.i64;
                case VAL_F64: {
                    int result = pyro_compare_int_and_float((int64_t)a.as.u32, b.as.f64);
                    return result == 1 || result == 0;
                }
                case VAL_CHAR:
                    return a.as.u32 >= b.as.u32;
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_F64: {
            switch (b.type) {
                case VAL_I64: {
                    int result = pyro_compare_int_and_float(b.as.i64, a.as.f64);
                    return result == 0 || result == -1;
                }
                case VAL_F64:
                    return a.as.f64 >= b.as.f64;
                case VAL_CHAR: {
                    int result = pyro_compare_int_and_float((int64_t)b.as.u32, a.as.f64);
                    return result == 0 || result == -1;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
            break;
        }

        case VAL_OBJ: {
            switch (AS_OBJ(a)->type) {
                case OBJ_STR: {
                    if (IS_STR(b)) {
                        return compare_strings(AS_STR(a), AS_STR(b)) >= 0;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_TUP: {
                    if (IS_TUP(b)) {
                        return compare_tuples(vm, AS_TUP(a), AS_TUP(b)) >= 0;
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                case OBJ_INSTANCE: {
                    Value method = pyro_get_method(vm, a, vm->str_op_binary_greater_equals);
                    if (!IS_NULL(method)) {
                        pyro_push(vm, a);
                        pyro_push(vm, b);
                        Value result = pyro_call_method(vm, method, 1);
                        if (vm->halt_flag) {
                            return false;
                        }
                        return pyro_is_truthy(result);
                    }
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
                }
                default:
                    pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
                    return false;
            }
        }

        default:
            pyro_panic(vm, ERR_TYPE_ERROR, "Values are not comparable.");
            return false;
    }
}