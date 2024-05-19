#include "cli.h"


int pyro_cli_cmd_time(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return 0;
    }

    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        return 1;
    }

    pyro_cli_add_import_roots_from_command_line(vm, cmd_parser);
    pyro_cli_add_import_roots_from_environment(vm);

    int num_runs = ap_get_int_value(cmd_parser, "num-runs");
    if (!pyro_define_superglobal(vm, "NUM_RUNS", pyro_i64(num_runs))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    char** args = ap_get_args(cmd_parser);
    if (!pyro_set_args(vm, ap_count_args(cmd_parser), args)) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        free(args);
        return 1;
    }
    free(args);

    PyroBuf* code = pyro_load_embedded_file(vm, "std/cli/time.pyro");
    if (!code) {
        fprintf(stderr, "error: failed to load 'embed/std/cli/time.pyro'\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_push(vm, pyro_obj(code))) {
        pyro_free_vm(vm);
        return 1;
    }

    pyro_exec_code(vm, (const char*)code->bytes, code->count, "std::cli::time", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
