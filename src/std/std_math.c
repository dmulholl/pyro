#include "std_math.h"

#include "../lib/values.h"
#include "../lib/vm.h"


static Value fn_abs(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        return I64_VAL(imaxabs(args[0].as.i64));
    } else if (IS_F64(args[0])) {
        return F64_VAL(fabs(args[0].as.f64));
    }
    pyro_panic(vm, "Argument to $std::math::abs() must be a number.");
    return NULL_VAL();
}


void pyro_load_std_math(PyroVM* vm) {
    ObjModule* mod_math = pyro_define_module_2(vm, "$std", "math");

    pyro_define_member(vm, mod_math, "pi", F64_VAL(PYRO_PI));
    pyro_define_member(vm, mod_math, "e", F64_VAL(PYRO_E));
    pyro_define_member(vm, mod_math, "nan", F64_VAL(NAN));
    pyro_define_member(vm, mod_math, "inf", F64_VAL(INFINITY));

    pyro_define_member_fn(vm, mod_math, "abs", fn_abs, 1);
}

