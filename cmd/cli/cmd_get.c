#include "cli.h"


int pyro_cli_cmd_get(char* cmd_name, ArgParser* cmd_parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, failed to initialize the Pyro VM\n");
        return 1;
    }

    char** args = ap_get_args(cmd_parser);
    if (!args) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_append_args(vm, args, ap_count_args(cmd_parser))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        free(args);
        return 1;
    }

    free(args);

    PyroBuf* code = pyro_load_embedded_file(vm, "std/cli/get.pyro");
    if (!code) {
        fprintf(stderr, "error: failed to load 'embed/std/cli/get.pyro'\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_push(vm, pyro_obj(code))) {
        pyro_free_vm(vm);
        return 1;
    }

    pyro_exec_code(vm, (const char*)code->bytes, code->count, "std::cli::get", NULL);

    int exit_code = (int)pyro_get_exit_code(vm);
    pyro_free_vm(vm);

    return exit_code;
}
