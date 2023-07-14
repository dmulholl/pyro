#include "../../inc/pyro.h"


void pyro_load_std_mod_constants(PyroVM* vm, PyroMod* module) {
    pyro_define_pub_member(vm, module, "pi", pyro_f64(PYRO_PI));
    pyro_define_pub_member(vm, module, "tau", pyro_f64(PYRO_PI * 2.0));
    pyro_define_pub_member(vm, module, "e", pyro_f64(PYRO_E));

    pyro_define_pub_member(vm, module, "nan", pyro_f64(NAN));
    pyro_define_pub_member(vm, module, "inf", pyro_f64(INFINITY));

    pyro_define_pub_member(vm, module, "i8_max", pyro_i64(INT8_MAX));
    pyro_define_pub_member(vm, module, "i16_max", pyro_i64(INT16_MAX));
    pyro_define_pub_member(vm, module, "i32_max", pyro_i64(INT32_MAX));
    pyro_define_pub_member(vm, module, "i64_max", pyro_i64(INT64_MAX));

    pyro_define_pub_member(vm, module, "i8_min", pyro_i64(INT8_MIN));
    pyro_define_pub_member(vm, module, "i16_min", pyro_i64(INT16_MIN));
    pyro_define_pub_member(vm, module, "i32_min", pyro_i64(INT32_MIN));
    pyro_define_pub_member(vm, module, "i64_min", pyro_i64(INT64_MIN));

    pyro_define_pub_member(vm, module, "u8_max", pyro_i64(UINT8_MAX));
    pyro_define_pub_member(vm, module, "u16_max", pyro_i64(UINT16_MAX));
    pyro_define_pub_member(vm, module, "u32_max", pyro_i64(UINT32_MAX));
}

