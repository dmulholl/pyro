#include "cli.h"


void pyro_cmd_test(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }

    int funcs_passed = 0;
    int funcs_failed = 0;
    int files_passed = 0;
    int files_failed = 0;

    double start_time = clock();

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        printf("## Testing: %s\n", path);

        PyroVM* vm = pyro_new_vm();
        if (!vm) {
            fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
            exit(2);
        }

        // Swallow error output unless the --verbose flag has been set.
        if (!ap_found(cmd_parser, "verbose")) {
            pyro_set_err_file(vm, NULL);
        }

        // Add the directory containing the script file to the list of import roots.
        char* path_copy = strdup(path);
        if (!path_copy) {
            fprintf(stderr, "Error: Out of memory.\n");
            exit(2);
        }
        pyro_add_import_root(vm, pyro_dirname(path_copy));
        free(path_copy);

        // Set the VM"s max memory allocation.
        pyro_cli_set_max_memory(vm, cmd_parser);

        // Add any import roots supplied on the command line.
        pyro_cli_add_import_roots(vm, cmd_parser);

        // Compile and execute the script.
        pyro_exec_file_as_main(vm, path);

        if (pyro_get_exit_flag(vm)) {
            printf(" · \x1B[1;31mEXIT\x1B[0m (%lld)\n\n", pyro_get_status_code(vm));
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        if (pyro_get_panic_flag(vm)) {
            printf(" · \x1B[1;31mFAIL\x1B[0m\n\n");
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        int passed = 0;
        int failed = 0;
        pyro_run_test_funcs(vm, &passed, &failed);

        funcs_passed += passed;
        funcs_failed += failed;

        if (passed + failed == 0) {
            printf(" · PASS\n\n");
            files_passed += 1;
        } else if (failed > 0) {
            printf(" · \x1B[1;31mFAIL\x1B[0m (%d/%d)\n\n", passed, passed + failed);
            files_failed += 1;
        } else {
            printf(" · PASS (%d/%d)\n\n", passed, passed + failed);
            files_passed += 1;
        }

        pyro_free_vm(vm);
    }

    double time = (double)(clock() - start_time) / CLOCKS_PER_SEC;

    /* printf("  \x1B[7m %s \x1B[0m", files_failed > 0 ? "FAIL" : "PASS"); */
    printf("  \x1B[1;7m %s \x1B[0m", files_failed > 0 ? "FAIL" : "PASS");
    printf("  Files: %d/%d", files_passed, files_passed + files_failed);
    printf(" · Funcs: %d/%d", funcs_passed, funcs_passed + funcs_failed);
    printf(" · Time: %f secs\n", time);

    exit(files_failed > 0 ? 1 : 0);
}
