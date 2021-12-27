#include "std_lib.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/setup.h"
#include "../vm/panics.h"


static Value fn_abs(PyroVM* vm, size_t arg_count, Value* args) {
    if (arg_count == 1) {
        if (IS_I64(args[0])) {
            return I64_VAL(imaxabs(args[0].as.i64));
        } else if (IS_F64(args[0])) {
            return F64_VAL(fabs(args[0].as.f64));
        }
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::abs(), must be a number.");
        return NULL_VAL();
    }

    if (arg_count == 2) {
        if (IS_I64(args[0])) {
            if (args[0].as.i64 == INT64_MIN) {
                return args[1];
            } else {
                return I64_VAL(imaxabs(args[0].as.i64));
            }
        } else if (IS_F64(args[0])) {
            return F64_VAL(fabs(args[0].as.f64));
        }
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::abs(), must be a number.");
        return NULL_VAL();
    }

    pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or 2 arguments for $std::math::abs(), found 0.");
    return NULL_VAL();
}


static Value fn_acos(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(acos((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(acos(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::acos(), must be a number.");
    return NULL_VAL();
}


static Value fn_asin(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(asin((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(asin(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::asin(), must be a number.");
    return NULL_VAL();
}


static Value fn_atan(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(atan((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(atan(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::atan(), must be a number.");
    return NULL_VAL();
}


static Value fn_cos(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(cos((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(cos(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::cos(), must be a number.");
    return NULL_VAL();
}


static Value fn_sin(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(sin((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(sin(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::sin(), must be a number.");
    return NULL_VAL();
}


static Value fn_tan(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(tan((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(tan(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::tan(), must be a number.");
    return NULL_VAL();
}


static Value fn_ln(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(log((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(log(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::ln(), must be a number.");
    return NULL_VAL();
}


static Value fn_log10(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(log10((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(log10(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::log10(), must be a number.");
    return NULL_VAL();
}


static Value fn_log2(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(log((double)args[0].as.i64) / log(2.0));
    } else if (IS_F64(args[0])) {
        return F64_VAL(log(args[0].as.f64) / log(2.0));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::log2(), must be a number.");
    return NULL_VAL();
}


static Value fn_log(PyroVM* vm, size_t arg_count, Value* args) {
    double base;
    double operand;

    if (IS_F64(args[0])) {
        base = args[0].as.f64;
    } else if (IS_I64(args[0])) {
        base = (double)args[0].as.i64;
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::log(), base must be a number.");
        return NULL_VAL();
    }

    if (IS_F64(args[1])) {
        operand = args[1].as.f64;
    } else if (IS_I64(args[1])) {
        operand = (double)args[1].as.i64;
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::log(), operand must be a number.");
        return NULL_VAL();
    }

    return F64_VAL(log(operand) / log(base));
}


static Value fn_atan2(PyroVM* vm, size_t arg_count, Value* args) {
    double y;
    double x;

    if (IS_F64(args[0])) {
        y = args[0].as.f64;
    } else if (IS_I64(args[0])) {
        y = (double)args[0].as.i64;
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::atan2(), y must be a number.");
        return NULL_VAL();
    }

    if (IS_F64(args[1])) {
        x = args[1].as.f64;
    } else if (IS_I64(args[1])) {
        x = (double)args[1].as.i64;
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::atan2(), x must be a number.");
        return NULL_VAL();
    }

    return F64_VAL(atan2(y, x));
}


static Value fn_exp(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(exp((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(exp(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::exp(), must be a number.");
    return NULL_VAL();
}


static Value fn_sqrt(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(sqrt((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(sqrt(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::sqrt(), must be a number.");
    return NULL_VAL();
}


static Value fn_cbrt(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(cbrt((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(cbrt(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::cbrt(), must be a number.");
    return NULL_VAL();
}


static Value fn_ceil(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(ceil((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(ceil(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::ceil(), must be a number.");
    return NULL_VAL();
}


static Value fn_floor(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return F64_VAL(floor((double)args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(floor(args[0].as.f64));
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to $std::math::floor(), must be a number.");
    return NULL_VAL();
}


void pyro_load_std_math(PyroVM* vm) {
    ObjModule* mod_math = pyro_define_module_2(vm, "$std", "math");
    if (!mod_math) {
        return;
    }

    pyro_define_member(vm, mod_math, "pi", F64_VAL(PYRO_PI));
    pyro_define_member(vm, mod_math, "e", F64_VAL(PYRO_E));
    pyro_define_member(vm, mod_math, "nan", F64_VAL(NAN));
    pyro_define_member(vm, mod_math, "inf", F64_VAL(INFINITY));
    pyro_define_member(vm, mod_math, "i64_max", I64_VAL(INT64_MAX));
    pyro_define_member(vm, mod_math, "i64_min", I64_VAL(INT64_MIN));

    pyro_define_member_fn(vm, mod_math, "abs", fn_abs, -1);
    pyro_define_member_fn(vm, mod_math, "acos", fn_acos, 1);
    pyro_define_member_fn(vm, mod_math, "asin", fn_asin, 1);
    pyro_define_member_fn(vm, mod_math, "atan", fn_atan, 1);
    pyro_define_member_fn(vm, mod_math, "cos", fn_cos, 1);
    pyro_define_member_fn(vm, mod_math, "sin", fn_sin, 1);
    pyro_define_member_fn(vm, mod_math, "tan", fn_tan, 1);
    pyro_define_member_fn(vm, mod_math, "ln", fn_ln, 1);
    pyro_define_member_fn(vm, mod_math, "log", fn_log, 2);
    pyro_define_member_fn(vm, mod_math, "log10", fn_log10, 1);
    pyro_define_member_fn(vm, mod_math, "log2", fn_log2, 1);
    pyro_define_member_fn(vm, mod_math, "atan2", fn_atan2, 2);
    pyro_define_member_fn(vm, mod_math, "exp", fn_exp, 1);
    pyro_define_member_fn(vm, mod_math, "sqrt", fn_sqrt, 1);
    pyro_define_member_fn(vm, mod_math, "cbrt", fn_cbrt, 1);
    pyro_define_member_fn(vm, mod_math, "ceil", fn_ceil, 1);
    pyro_define_member_fn(vm, mod_math, "floor", fn_floor, 1);
}
