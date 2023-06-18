#include "cli.h"


int pyro_cli_cmd_check(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return 0;
    }

    size_t stack_size = 1024 * 1024;
    bool had_panic = false;

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
            exit(1);
        }

        char* path = ap_get_arg_at_index(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "error: invalid path '%s'\n", path);
            exit(1);
        }

        pyro_try_compile_file(vm, path, ap_found(cmd_parser, "disassemble"));
        if (pyro_get_panic_flag(vm)) {
            had_panic = true;
        }

        pyro_free_vm(vm);
    }

    if (had_panic) {
        exit(1);
    }

    return 0;
}
