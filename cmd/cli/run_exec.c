#include "cli.h"


int pyro_cli_run_exec(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, failed to initialize the Pyro VM\n");
        return 1;
    }

    pyro_cli_set_max_memory(vm, parser);

    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);

    const char* code = ap_get_str_value(parser, "exec");
    pyro_exec_code(vm, code, strlen(code), "<exec>", NULL);

    int exit_code = (int)pyro_get_exit_code(vm);
    pyro_free_vm(vm);

    return exit_code;
}
