#include "cli.h"


void pyro_run_file(ArgParser* parser) {
    size_t stack_size = pyro_cli_get_stack_size(parser);

    char* path = ap_arg(parser, 0);
    if (!pyro_exists(path)) {
        fprintf(stderr, "Error: cannot locate %s.\n", path);
        exit(1);
    }

    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
        exit(1);
    }

    // Set the VM's maximum memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add any import roots supplied on the command line.
    pyro_cli_add_command_line_import_roots(vm, parser);

    // Add the standard import roots.
    pyro_cli_add_import_roots_from_path(vm, path);

    // Add the command line args to the global $args variable.
    char** args = ap_args(parser);
    pyro_set_args(vm, ap_count_args(parser), args);
    free(args);

    // Compile and execute the script.
    pyro_exec_path_as_main(vm, path);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        pyro_free_vm(vm);
        exit(pyro_get_exit_code(vm));
    }

    // Execute the $main() function if it exists.
    pyro_run_main_func(vm);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        pyro_free_vm(vm);
        exit(pyro_get_exit_code(vm));
    }

    pyro_free_vm(vm);
}
