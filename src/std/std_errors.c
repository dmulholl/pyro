#include "std_lib.h"

#include "../vm/vm.h"
#include "../vm/values.h"
#include "../vm/setup.h"


void pyro_load_std_errors(PyroVM* vm) {
    ObjModule* mod_errors = pyro_define_module_2(vm, "$std", "errors");
    if (!mod_errors) {
        return;
    }

    pyro_define_member(vm, mod_errors, "ok", MAKE_I64(ERR_OK));
    pyro_define_member(vm, mod_errors, "error", MAKE_I64(ERR_ERROR));
    pyro_define_member(vm, mod_errors, "out_of_memory", MAKE_I64(ERR_OUT_OF_MEMORY));
    pyro_define_member(vm, mod_errors, "os_error", MAKE_I64(ERR_OS_ERROR));
    pyro_define_member(vm, mod_errors, "args_error", MAKE_I64(ERR_ARGS_ERROR));
    pyro_define_member(vm, mod_errors, "assertion_failed", MAKE_I64(ERR_ASSERTION_FAILED));
    pyro_define_member(vm, mod_errors, "name_error", MAKE_I64(ERR_NAME_ERROR));
    pyro_define_member(vm, mod_errors, "value_error", MAKE_I64(ERR_VALUE_ERROR));
    pyro_define_member(vm, mod_errors, "type_error", MAKE_I64(ERR_TYPE_ERROR));
    pyro_define_member(vm, mod_errors, "module_not_found", MAKE_I64(ERR_MODULE_NOT_FOUND));
    pyro_define_member(vm, mod_errors, "syntax_error", MAKE_I64(ERR_SYNTAX_ERROR));
}
