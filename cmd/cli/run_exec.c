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
    size_t code_length = strlen(code);

    char* buffer = malloc(code_length + 2);
    if (!buffer) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    memcpy(buffer, code, code_length);
    buffer[code_length] = ';';
    buffer[code_length + 1] = '\0';

    pyro_exec_code(vm, buffer, code_length + 1, "<exec>", NULL);

    int exit_code = (int)pyro_get_exit_code(vm);
    pyro_free_vm(vm);
    free(buffer);

    return exit_code;
}
