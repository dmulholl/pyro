#include "../../inc/pyro.h"


static PyroValue fn_abs(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 1) {
        if (PYRO_IS_I64(args[0])) {
            return pyro_i64(imaxabs(args[0].as.i64));
        } else if (PYRO_IS_F64(args[0])) {
            return pyro_f64(fabs(args[0].as.f64));
        }
        pyro_panic(vm, "abs(): invalid argument, expected a number");
        return pyro_null();
    }

    if (arg_count == 2) {
        if (PYRO_IS_I64(args[0])) {
            if (args[0].as.i64 == INT64_MIN) {
                return args[1];
            } else {
                return pyro_i64(imaxabs(args[0].as.i64));
            }
        } else if (PYRO_IS_F64(args[0])) {
            return pyro_f64(fabs(args[0].as.f64));
        }
        pyro_panic(vm, "abs(): invalid argument, expected a number");
        return pyro_null();
    }

    pyro_panic(vm, "abs(): expected 1 or 2 arguments, found %zu", arg_count);
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


void pyro_load_std_mod_math(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member(vm, module, "PI", pyro_f64(PYRO_PI));
    pyro_define_pub_member(vm, module, "E", pyro_f64(PYRO_E));
    pyro_define_pub_member(vm, module, "NAN", pyro_f64(NAN));
    pyro_define_pub_member(vm, module, "INF", pyro_f64(INFINITY));
    pyro_define_pub_member(vm, module, "I64_MAX", pyro_i64(INT64_MAX));
    pyro_define_pub_member(vm, module, "I64_MIN", pyro_i64(INT64_MIN));

    pyro_define_pub_member_fn(vm, module, "abs", fn_abs, -1);
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
}

