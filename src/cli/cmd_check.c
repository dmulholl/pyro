#include "cli.h"


void pyro_cmd_check(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }

    size_t stack_size = pyro_cli_get_stack_size(cmd_parser);
    bool had_panic = false;

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "Error: Out of memory, unable to initialize Pyro VM.\n");
            exit(1);
        }

        char* path = ap_arg(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "Error: invalid path '%s'.\n", path);
            exit(1);
        }

        pyro_try_compile_file(vm, path);
        if (pyro_get_panic_flag(vm)) {
            had_panic = true;
        }

        pyro_free_vm(vm);
    }

    if (had_panic) {
        exit(1);
    }
}
