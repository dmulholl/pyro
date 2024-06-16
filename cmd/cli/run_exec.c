#include "cli.h"


void pyro_cli_run_exec(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        exit(1);
    }

    pyro_cli_set_max_memory(vm, parser);

    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);

    const char* code = ap_get_str_value(parser, "exec");
    pyro_exec_code(vm, code, strlen(code), "<exec>", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
}
