#include "../includes/pyro.h"


static PyroValue fn_abs(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        if (args[0].as.i64 == INT64_MIN) {
            pyro_panic(vm, "abs(): invalid argument i64_min, result would overflow");
            return pyro_null();
        }
        return pyro_i64(imaxabs(args[0].as.i64));
    }

    if (PYRO_IS_F64(args[0])) {
        return pyro_f64(fabs(args[0].as.f64));
    }

    pyro_panic(vm, "abs(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_acos(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(acos((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(acos(args[0].as.f64));
    }
    pyro_panic(vm, "acos(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_asin(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(asin((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(asin(args[0].as.f64));
    }
    pyro_panic(vm, "asin(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_atan(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(atan((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(atan(args[0].as.f64));
    }
    pyro_panic(vm, "atan(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_cos(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(cos((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(cos(args[0].as.f64));
    }
    pyro_panic(vm, "cos(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_sin(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(sin((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(sin(args[0].as.f64));
    }
    pyro_panic(vm, "sin(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_tan(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(tan((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(tan(args[0].as.f64));
    }
    pyro_panic(vm, "tan(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_ln(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(log((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(log(args[0].as.f64));
    }
    pyro_panic(vm, "ln(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_log10(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(log10((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(log10(args[0].as.f64));
    }
    pyro_panic(vm, "log10(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_log2(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(log((double)args[0].as.i64) / log(2.0));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(log(args[0].as.f64) / log(2.0));
    }
    pyro_panic(vm, "log2(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_log(PyroVM* vm, size_t arg_count, PyroValue* args) {
    double base;
    double operand;

    if (PYRO_IS_F64(args[0])) {
        base = args[0].as.f64;
    } else if (PYRO_IS_I64(args[0])) {
        base = (double)args[0].as.i64;
    } else {
        pyro_panic(vm, "log(): invalid argument, expected a number");
        return pyro_null();
    }

    if (PYRO_IS_F64(args[1])) {
        operand = args[1].as.f64;
    } else if (PYRO_IS_I64(args[1])) {
        operand = (double)args[1].as.i64;
    } else {
        pyro_panic(vm, "log(): invalid argument, expected a number");
        return pyro_null();
    }

    return pyro_f64(log(operand) / log(base));
}


static PyroValue fn_atan2(PyroVM* vm, size_t arg_count, PyroValue* args) {
    double y;
    double x;

    if (PYRO_IS_F64(args[0])) {
        y = args[0].as.f64;
    } else if (PYRO_IS_I64(args[0])) {
        y = (double)args[0].as.i64;
    } else {
        pyro_panic(vm, "atan2(): invalid argument, expected a number");
        return pyro_null();
    }

    if (PYRO_IS_F64(args[1])) {
        x = args[1].as.f64;
    } else if (PYRO_IS_I64(args[1])) {
        x = (double)args[1].as.i64;
    } else {
        pyro_panic(vm, "atan2(): invalid argument, expected a number");
        return pyro_null();
    }

    return pyro_f64(atan2(y, x));
}


static PyroValue fn_exp(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(exp((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(exp(args[0].as.f64));
    }
    pyro_panic(vm, "exp(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_sqrt(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(sqrt((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(sqrt(args[0].as.f64));
    }
    pyro_panic(vm, "sqrt(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_cbrt(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(cbrt((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(cbrt(args[0].as.f64));
    }
    pyro_panic(vm, "cbrt(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_ceil(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(ceil((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(ceil(args[0].as.f64));
    }
    pyro_panic(vm, "ceil(): invalid argument, expected a number");
    return pyro_null();
}


static PyroValue fn_floor(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        return pyro_f64(floor((double)args[0].as.i64));
    } else if (PYRO_IS_F64(args[0])) {
        return pyro_f64(floor(args[0].as.f64));
    }
    pyro_panic(vm, "floor(): invalid argument, must be a number");
    return pyro_null();
}


static PyroValue fn_trunc_div(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "div(): invalid argument [numerator]: expected i64, found %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[1])) {
        pyro_panic(vm,
            "div(): invalid argument [denominator]: expected i64, found %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
        return pyro_null();
    }

    if (args[1].as.i64 == 0) {
        pyro_panic(vm, "div(): invalid argument [denominator]: value cannot be zero");
        return pyro_null();
    }

    lldiv_t result = lldiv(args[0].as.i64, args[1].as.i64);

    PyroTup* tup = PyroTup_new(2, vm);
    if (!tup) {
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    tup->values[0] = pyro_i64(result.quot);
    tup->values[1] = pyro_i64(result.rem);

    return pyro_obj(tup);
}


static PyroValue fn_floor_div(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "floor_div(): invalid argument [numerator]: expected i64, found %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[1])) {
        pyro_panic(vm,
            "floor_div(): invalid argument [denominator]: expected i64, found %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
        return pyro_null();
    }

    if (args[1].as.i64 == 0) {
        pyro_panic(vm, "floor_div(): invalid argument [denominator]: value cannot be zero");
        return pyro_null();
    }

    int64_t numerator = args[0].as.i64;
    int64_t denominator = args[1].as.i64;

    PyroTup* tup = PyroTup_new(2, vm);
    if (!tup) {
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    lldiv_t div_result = lldiv(numerator, denominator);

    if (div_result.rem != 0 && ((numerator < 0) ^ (denominator < 0))) {
        tup->values[0] = pyro_i64(div_result.quot - 1);
    } else {
        tup->values[0] = pyro_i64(div_result.quot);
    }

    tup->values[1] = pyro_i64(
        pyro_modulo(numerator, denominator)
    );

    return pyro_obj(tup);
}


static PyroValue fn_modulo(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm,
            "modulo(): invalid argument [x]: expected i64, found %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[1])) {
        pyro_panic(vm,
            "modulo(): invalid argument [y]: expected i64, found %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
        return pyro_null();
    }

    if (args[1].as.i64 == 0) {
        pyro_panic(vm, "modulo(): invalid argument [y]: value cannot be zero");
        return pyro_null();
    }

    return pyro_i64(
        pyro_modulo(args[0].as.i64, args[1].as.i64)
    );
}


void pyro_load_stdlib_module_math(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member_fn(vm, module, "abs", fn_abs, 1);
    pyro_define_pub_member_fn(vm, module, "acos", fn_acos, 1);
    pyro_define_pub_member_fn(vm, module, "asin", fn_asin, 1);
    pyro_define_pub_member_fn(vm, module, "atan", fn_atan, 1);
    pyro_define_pub_member_fn(vm, module, "cos", fn_cos, 1);
    pyro_define_pub_member_fn(vm, module, "sin", fn_sin, 1);
    pyro_define_pub_member_fn(vm, module, "tan", fn_tan, 1);
    pyro_define_pub_member_fn(vm, module, "ln", fn_ln, 1);
    pyro_define_pub_member_fn(vm, module, "log", fn_log, 2);
    pyro_define_pub_member_fn(vm, module, "log10", fn_log10, 1);
    pyro_define_pub_member_fn(vm, module, "log2", fn_log2, 1);
    pyro_define_pub_member_fn(vm, module, "atan2", fn_atan2, 2);
    pyro_define_pub_member_fn(vm, module, "exp", fn_exp, 1);
    pyro_define_pub_member_fn(vm, module, "sqrt", fn_sqrt, 1);
    pyro_define_pub_member_fn(vm, module, "cbrt", fn_cbrt, 1);
    pyro_define_pub_member_fn(vm, module, "ceil", fn_ceil, 1);
    pyro_define_pub_member_fn(vm, module, "floor", fn_floor, 1);
    pyro_define_pub_member_fn(vm, module, "trunc_div", fn_trunc_div, 2);
    pyro_define_pub_member_fn(vm, module, "floor_div", fn_floor_div, 2);
    pyro_define_pub_member_fn(vm, module, "modulo", fn_modulo, 2);
}
