#include "cli.h"


void pyro_cmd_time(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }

    int iterations = ap_int_value(cmd_parser, "num-runs");
    if (iterations < 1) {
        fprintf(stderr, "Error: invalid argument for --num-runs.\n");
        exit(1);
    }

    size_t stack_size = pyro_cli_get_stack_size(cmd_parser);
    double start_time = clock();

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "Error: invalid path '%s'.\n", path);
            exit(1);
        }

        printf("## Benchmarking: %s\n", path);

        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
            exit(2);
        }

        // Set the VM"s max memory allocation.
        pyro_cli_set_max_memory(vm, cmd_parser);

        // Add any import roots supplied on the command line.
        pyro_cli_add_command_line_import_roots(vm, cmd_parser);

        // Add the standard import roots.
        pyro_cli_add_import_roots_from_path(vm, path);

        pyro_exec_path_as_main(vm, path);
        if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_exit_code(vm));
        }

        pyro_run_time_funcs(vm, iterations);
        if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_exit_code(vm));
        }

        pyro_free_vm(vm);
        printf("\n");
    }

    double time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Total Runtime: %f secs\n", time);
}
