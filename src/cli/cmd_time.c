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

    double start_time = clock();

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        printf("## Benchmarking: %s\n", path);

        PyroVM* vm = pyro_new_vm();
        if (!vm) {
            fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
            exit(2);
        }

        char* path_copy = strdup(path);
        if (!path_copy) {
            fprintf(stderr, "Error: Out of memory.\n");
            exit(2);
        }
        pyro_add_import_root(vm, pyro_dirname(path_copy));
        free(path_copy);

        printf("testing.....\n");
        pyro_exec_file_as_main(vm, path);
        if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_status_code(vm));
        }

        pyro_run_time_funcs(vm, iterations);
        if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_status_code(vm));
        }

        pyro_free_vm(vm);
        printf("\n");
    }

    double time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("Total Runtime: %f secs\n", time);
}
