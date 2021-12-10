#include "std_errors.h"

#include "../vm/vm.h"
#include "../vm/values.h"
#include "../vm/setup.h"


void pyro_load_std_errors(PyroVM* vm) {
    ObjModule* mod_errors = pyro_define_module_2(vm, "$std", "errors");
    if (!mod_errors) {
        return;
    }

    pyro_define_member(vm, mod_errors, "ok", I64_VAL(ERR_OK));
    pyro_define_member(vm, mod_errors, "error", I64_VAL(ERR_ERROR));
    pyro_define_member(vm, mod_errors, "out_of_memory", I64_VAL(ERR_OUT_OF_MEMORY));
    pyro_define_member(vm, mod_errors, "os_error", I64_VAL(ERR_OS_ERROR));
    pyro_define_member(vm, mod_errors, "args_error", I64_VAL(ERR_ARGS_ERROR));
    pyro_define_member(vm, mod_errors, "assertion_failed", I64_VAL(ERR_ASSERTION_FAILED));
    pyro_define_member(vm, mod_errors, "name_error", I64_VAL(ERR_NAME_ERROR));
    pyro_define_member(vm, mod_errors, "value_error", I64_VAL(ERR_VALUE_ERROR));
    pyro_define_member(vm, mod_errors, "type_error", I64_VAL(ERR_TYPE_ERROR));
    pyro_define_member(vm, mod_errors, "module_not_found", I64_VAL(ERR_MODULE_NOT_FOUND));
    pyro_define_member(vm, mod_errors, "syntax_error", I64_VAL(ERR_SYNTAX_ERROR));
}
