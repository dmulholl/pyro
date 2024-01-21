#include "cli.h"


void pyro_cli_run_path(ArgParser* parser) {
    char* path = ap_get_arg_at_index(parser, 0);
    if (!pyro_exists(path)) {
        fprintf(stderr, "error: invalid path '%s'\n", path);
        exit(1);
    }

    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        exit(1);
    }

    // Set the VM's maximum memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add import roots.
    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);
    pyro_cli_add_import_roots_from_path(vm, path);

    // Add the command line arguments to the global $args variable.
    char** args = ap_get_args(parser);
    if (!pyro_set_args(vm, ap_count_args(parser), args)) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        free(args);
        exit(1);
    }
    free(args);

    // Set the trace-execution flag.
    pyro_set_trace_execution_flag(vm, ap_found(parser, "trace-execution"));

    // Compile and execute the script.
    pyro_exec_path(vm, path, NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    // Execute the $main() function if it exists.
    pyro_run_main_func(vm);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
}
