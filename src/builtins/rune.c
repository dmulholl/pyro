#include "../includes/pyro.h"


static PyroValue fn_rune(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (PYRO_IS_I64(args[0])) {
        int64_t arg = args[0].as.i64;
        if (arg >= 0 && arg <= UINT32_MAX) {
            return pyro_rune((uint32_t)arg);
        } else {
            pyro_panic(vm, "$rune(): invalid argument, integer is out of range");
            return pyro_null();
        }
    }

    pyro_panic(vm, "$rune(): invalid argument");
    return pyro_null();
}


static PyroValue fn_is_rune(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_RUNE(args[0]));
}


static PyroValue rune_is_ascii(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(args[-1].as.u32 < 128);
}


static PyroValue rune_is_ascii_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_ws((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_ascii_decimal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_decimal_digit((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_ascii_octal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_octal_digit((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_ascii_hex(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_hex_digit((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_ascii_alpha(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_alpha((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_ascii_printable(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(
        args[-1].as.u32 < 128 && pyro_is_ascii_printable((char)args[-1].as.u32)
    );
}


static PyroValue rune_is_unicode_ws(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(pyro_is_unicode_whitespace(args[-1].as.u32));
}


void pyro_load_std_builtins_rune(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$rune", fn_rune, 1);
    pyro_define_superglobal_fn(vm, "$is_rune", fn_is_rune, 1);

    // Methods.
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii", rune_is_ascii, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_ws", rune_is_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_unicode_ws", rune_is_unicode_ws, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_decimal", rune_is_ascii_decimal, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_octal", rune_is_ascii_octal, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_hex", rune_is_ascii_hex, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_alpha", rune_is_ascii_alpha, 0);
    pyro_define_pub_method(vm, vm->class_rune, "is_ascii_printable", rune_is_ascii_printable, 0);
}
