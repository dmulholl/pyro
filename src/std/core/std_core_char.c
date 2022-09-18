#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/stringify.h"
#include "../../inc/panics.h"
#include "../../inc/exec.h"


static Value fn_char(PyroVM* vm, size_t arg_count, Value* args) {
    if (IS_I64(args[0])) {
        int64_t arg = args[0].as.i64;
        if (arg >= 0 && arg <= UINT32_MAX) {
            return pyro_make_char((uint32_t)arg);
        } else {
            pyro_panic(vm, "$char(): invalid argument, integer is out of range");
            return pyro_make_null();
        }
    }

    pyro_panic(vm, "$char(): invalid argument");
    return pyro_make_null();
}


static Value fn_is_char(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_CHAR(args[0]));
}


static Value char_is_ascii(PyroVM* vm, size_t arg_count, Value* args) {
    return args[-1].as.u32 > 127 ? pyro_make_bool(false) : pyro_make_bool(true);
}


static Value char_is_ascii_ws(PyroVM* vm, size_t arg_count, Value* args) {
    switch (args[-1].as.u32) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\v':
        case '\f':
            return pyro_make_bool(true);
        default:
            return pyro_make_bool(false);
    }
}


static Value char_is_unicode_ws(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(pyro_is_unicode_whitespace(args[-1].as.u32));
}


static Value char_is_ascii_decimal(PyroVM* vm, size_t arg_count, Value* args) {
    return (args[-1].as.u32 < '0' || args[-1].as.u32 > '9') ? pyro_make_bool(false) : pyro_make_bool(true);
}


static Value char_is_ascii_octal(PyroVM* vm, size_t arg_count, Value* args) {
    return (args[-1].as.u32 < '0' || args[-1].as.u32 > '7') ? pyro_make_bool(false) : pyro_make_bool(true);
}


static Value char_is_ascii_hex(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(isxdigit(args[-1].as.u32));
}


void pyro_load_std_core_char(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$char", fn_char, 1);
    pyro_define_global_fn(vm, "$is_char", fn_is_char, 1);

    // Methods.
    pyro_define_pub_method(vm, vm->class_char, "is_ascii", char_is_ascii, 0);
    pyro_define_pub_method(vm, vm->class_char, "is_ascii_ws", char_is_ascii_ws, 0);
    pyro_define_pub_method(vm, vm->class_char, "is_unicode_ws", char_is_unicode_ws, 0);
    pyro_define_pub_method(vm, vm->class_char, "is_ascii_decimal", char_is_ascii_decimal, 0);
    pyro_define_pub_method(vm, vm->class_char, "is_ascii_octal", char_is_ascii_octal, 0);
    pyro_define_pub_method(vm, vm->class_char, "is_ascii_hex", char_is_ascii_hex, 0);
}
