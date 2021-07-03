#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

#include <libgen.h>

#include "../pyro.h"
#include "args.h"


static void run_file(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "Error: Unable to initialize VM.\n");
        exit(1);
    }

    char** args = ap_args(parser);
    pyro_set_args(vm, ap_count_args(parser), args);
    free(args);

    char* path = ap_arg(parser, 0);
    char* path_copy = strdup(path);
    pyro_add_import_root(vm, dirname(path_copy));
    free(path_copy);

    pyro_exec_file_as_main(vm, path);
    if (pyro_get_halt_flag(vm)) {
        exit(pyro_get_exit_code(vm));
    }

    pyro_run_main_func(vm);
    if (pyro_get_halt_flag(vm)) {
        exit(pyro_get_exit_code(vm));
    }

    pyro_free_vm(vm);
}


static void cmd_test_callback(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }

    int funcs_passed = 0;
    int funcs_failed = 0;
    int files_passed = 0;
    int files_failed = 0;

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        printf("## Testing: %s\n", path);

        PyroVM* vm = pyro_new_vm();
        if (!vm) {
            fprintf(stderr, "Error: Unable to initialize VM.\n");
            exit(1);
        }

        char* path_copy = strdup(path);
        pyro_add_import_root(vm, dirname(path_copy));
        free(path_copy);

        if (!ap_found(cmd_parser, "errors")) {
            pyro_set_err_file(vm, NULL);
        }

        pyro_exec_file_as_main(vm, path);

        if (pyro_get_exit_flag(vm)) {
            printf("-- \x1B[31mEXIT\x1B[39m (%d)\n\n", pyro_get_exit_code(vm));
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        if (pyro_get_panic_flag(vm)) {
            printf("-- \x1B[31mFAIL\x1B[39m\n\n");
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        if (pyro_get_memory_error_flag(vm)) {
            printf("-- \x1B[31mMEMORY ERROR\x1B[39m\n\n");
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
            printf("-- PASS\n\n");
            files_passed += 1;
        } else if (failed > 0) {
            printf("-- \x1B[31mFAIL\x1B[39m (%d/%d)\n\n", passed, passed + failed);
            files_failed += 1;
        } else {
            printf("-- PASS (%d/%d)\n\n", passed, passed + failed);
            files_passed += 1;
        }

        pyro_free_vm(vm);
    }

    if (files_failed > 0) {
        printf("  \x1B[7m FAIL \x1B[0m");
    } else {
        printf("  \x1B[7m PASS \x1B[0m");
    }

    printf(
        "  Files: %d/%d · Tests: %d/%d\n",
        files_passed,
        files_passed + files_failed,
        funcs_passed,
        funcs_passed + funcs_failed
    );

    exit(files_failed > 0 ? 1 : 0);
}


int main(int argc, char* argv[]) {
    ArgParser* parser = ap_new();
    ap_helptext(parser, "Usage: pyro [file]");
    ap_version(parser, "0.2.0.dev");

    ArgParser* test_cmd = ap_cmd(parser, "test");
    ap_helptext(test_cmd, "Usage: pyro test [files]");
    ap_callback(test_cmd, cmd_test_callback);
    ap_flag(test_cmd, "errors e");

    ap_parse(parser, argc, argv);

    if (!ap_has_cmd(parser)) {
        if (ap_count_args(parser) > 0) {
            run_file(parser);
        } else {
            fprintf(stderr, "Error: Missing argument.\n");
            exit(1);
        }
    }

    ap_free(parser);
    return 0;
}
