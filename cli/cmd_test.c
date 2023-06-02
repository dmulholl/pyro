#include "cli.h"


int pyro_cli_cmd_test(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return 0;
    }

    size_t stack_size = 1024 * 1024;
    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        exit(1);
    }

    pyro_cli_add_import_roots_from_command_line(vm, cmd_parser);
    pyro_cli_add_import_roots_from_environment(vm);

    bool errors = ap_found(cmd_parser, "errors");
    if (!pyro_define_global(vm, "ERRORS", pyro_bool(errors))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        exit(1);
    }

    char** args = ap_get_args(cmd_parser);
    if (!pyro_set_args(vm, ap_count_args(cmd_parser), args)) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        free(args);
        exit(1);
    }
    free(args);

    const unsigned char* code;
    size_t code_length;

    if (!pyro_get_embedded("std/cmd/test.pyro", &code, &code_length)) {
        fprintf(stderr, "error: failed to load std/cmd/test.pyro\n");
        pyro_free_vm(vm);
        exit(1);
    }

    pyro_exec_code(vm, (const char*)code, code_length, "std::cmd::test", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
