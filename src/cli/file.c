#include "cli.h"


void pyro_run_file(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "Error: Out of memory, unable to initialize Pyro VM.\n");
        exit(1);
    }

    // Set the VM"s max memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add any import roots supplied on the command line.
    pyro_cli_add_import_roots(vm, parser);

    // Add the directory containing the script file to the list of import roots.
    char* path = ap_arg(parser, 0);
    char* path_copy = strdup(path);
    pyro_add_import_root(vm, dirname(path_copy));
    free(path_copy);

    // Add the command line args to the global $args variable.
    char** args = ap_args(parser);
    pyro_set_args(vm, ap_count_args(parser), args);
    free(args);

    // Compile and execute the script.
    pyro_exec_file_as_main(vm, path);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        pyro_free_vm(vm);
        exit(pyro_get_status_code(vm));
    }

    // Execute the $main() function if it exists.
    pyro_run_main_func(vm);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        pyro_free_vm(vm);
        exit(pyro_get_status_code(vm));
    }

    pyro_free_vm(vm);
}
