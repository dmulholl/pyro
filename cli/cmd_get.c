#include "cli.h"


int pyro_cli_cmd_get(char* cmd_name, ArgParser* cmd_parser) {
    size_t stack_size = 1024 * 1024;

    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        exit(1);
    }

    char** args = ap_get_args(cmd_parser);
    pyro_set_args(vm, ap_count_args(cmd_parser), args);
    free(args);

    const unsigned char* code;
    size_t count;

    if (!pyro_get_embedded("std/cli/get.pyro", &code, &count)) {
        fprintf(stderr, "error: failed to load std/cli/get.pyro\n");
        exit(1);
    }

    pyro_exec_code(vm, (const char*)code, count, "std/cli/get.pyro", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
