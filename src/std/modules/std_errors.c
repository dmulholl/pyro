#include "../std_lib.h"

#include "../../vm/vm.h"
#include "../../vm/values.h"
#include "../../vm/setup.h"


void pyro_load_std_mod_errors(PyroVM* vm, ObjModule* module) {
    pyro_define_member(vm, module, "ok", MAKE_I64(ERR_OK));
    pyro_define_member(vm, module, "error", MAKE_I64(ERR_ERROR));
    pyro_define_member(vm, module, "out_of_memory", MAKE_I64(ERR_OUT_OF_MEMORY));
    pyro_define_member(vm, module, "os_error", MAKE_I64(ERR_OS_ERROR));
    pyro_define_member(vm, module, "args_error", MAKE_I64(ERR_ARGS_ERROR));
    pyro_define_member(vm, module, "assertion_failed", MAKE_I64(ERR_ASSERTION_FAILED));
    pyro_define_member(vm, module, "name_error", MAKE_I64(ERR_NAME_ERROR));
    pyro_define_member(vm, module, "value_error", MAKE_I64(ERR_VALUE_ERROR));
    pyro_define_member(vm, module, "type_error", MAKE_I64(ERR_TYPE_ERROR));
    pyro_define_member(vm, module, "module_not_found", MAKE_I64(ERR_MODULE_NOT_FOUND));
    pyro_define_member(vm, module, "syntax_error", MAKE_I64(ERR_SYNTAX_ERROR));
}
