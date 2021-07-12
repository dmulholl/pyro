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


static const char* VERSION = "0.2.0.dev";


static bool has_open_quote(const char* code, size_t code_count) {
    bool in_quotes = false;
    size_t index = 0;

    while (index < code_count) {
        char c = code[index++];

        if (c == '`') {
            in_quotes = true;
            while (index < code_count) {
                if (code[index++] == '`') {
                    in_quotes = false;
                    break;
                }
            }
        }

        else if (c == '"') {
            in_quotes = true;
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '"') {
                    in_quotes = false;
                    break;
                }
            }
        }

        else if (c == '\'') {
            in_quotes = true;
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '\'') {
                    in_quotes = false;
                    break;
                }
            }
        }
    }

    return in_quotes;
}


static int count_brackets(const char* code, size_t code_count) {
    int bracket_count = 0;
    size_t index = 0;

    while (index < code_count) {
        char c = code[index++];

        if (c == '(' || c == '[' || c == '{') {
            bracket_count++;
        }

        else if (c == ')' || c == ']' || c == '}') {
            bracket_count--;
        }

        else if (c == '`') {
            while (index < code_count && code[index++] != '`');
        }

        else if (c == '"') {
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '"') {
                    break;
                }
            }
        }

        else if (c == '\'') {
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '\'') {
                    break;
                }
            }
        }
    }

    return bracket_count;
}


static void run_repl(void) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "Error: Unable to initialize VM.\n");
        exit(1);
    }

    pyro_add_import_root(vm, ".");

    printf("Pyro %s -- Type 'exit' to quit.\n", VERSION);

    char* code = NULL;
    size_t code_count = 0;

    for (;;) {
        if (code) {
            printf("... ");
        } else {
            printf(">>> ");
        }

        char* line = NULL;
        size_t line_capacity = 0;
        ssize_t line_count = getline(&line, &line_capacity, stdin);

        if (line_count == -1) {
            if (feof(stdin)) {
                printf("EOF\n");
                exit(0);
            } else {
                fprintf(stderr, "Error: Failed to read input from STDIN.\n");
                exit(1);
            }
        }

        if (strcmp(line, "exit\n") == 0) {
            exit(0);
        }

        code = realloc(code, code_count + line_count);
        if (!code) {
            fprintf(stderr, "Error: Failed to allocate memory for input.\n");
            exit(1);
        }

        memcpy(&code[code_count], line, line_count);
        code_count += line_count;
        free(line);

        // If the code contains unclosed quotes or more opening brackets than closing brackets,
        // read and append another line of input.
        if (has_open_quote(code, code_count) || count_brackets(code, code_count) > 0) {
            continue;
        }

        if (code[code_count - 1] == '\n') {
            code_count--;
        }

        pyro_exec_code_as_main(vm, code, code_count, "repl");
        if (pyro_get_exit_flag(vm) || pyro_get_memory_error_flag(vm)) {
            exit(pyro_get_exit_code(vm));
        }

        free(code);
        code = NULL;
        code_count = 0;
    }

    pyro_free_vm(vm);
}


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

    double start_time = clock();

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

        if (!ap_found(cmd_parser, "verbose")) {
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

    double time = (clock() - start_time) / CLOCKS_PER_SEC;

    printf("  \x1B[7m %s \x1B[0m", files_failed > 0 ? "FAIL" : "PASS");
    printf("  Files: %d/%d", files_passed, files_passed + files_failed);
    printf(" · Tests: %d/%d", funcs_passed, funcs_passed + funcs_failed);
    printf(" · Time: %f secs\n", time);

    exit(files_failed > 0 ? 1 : 0);
}


static void cmd_time_callback(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }

    int iterations = ap_int_value(cmd_parser, "num-iterations");
    if (iterations < 1) {
        fprintf(stderr, "Error: invalid number of iterations.\n");
        exit(1);
    }

    double start_time = clock();

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        printf("## Timing: %s\n", path);

        PyroVM* vm = pyro_new_vm();
        if (!vm) {
            fprintf(stderr, "Error: Unable to initialize VM.\n");
            exit(1);
        }

        char* path_copy = strdup(path);
        pyro_add_import_root(vm, dirname(path_copy));
        free(path_copy);

        pyro_exec_file_as_main(vm, path);
        if (pyro_get_halt_flag(vm)) {
            exit(pyro_get_exit_code(vm));
        }

        pyro_run_time_funcs(vm, iterations);
        if (pyro_get_halt_flag(vm)) {
            exit(pyro_get_exit_code(vm));
        }

        pyro_free_vm(vm);
        printf("\n");
    }

    double time = (clock() - start_time) / CLOCKS_PER_SEC;
    printf("Total Runtime: %f secs\n", time);
}


int main(int argc, char* argv[]) {
    ArgParser* parser = ap_new();
    ap_helptext(parser, "Usage: pyro [file]");
    ap_version(parser, VERSION);

    ArgParser* test_cmd = ap_cmd(parser, "test");
    ap_helptext(test_cmd, "Usage: pyro test [files]");
    ap_callback(test_cmd, cmd_test_callback);
    ap_flag(test_cmd, "verbose v");

    ArgParser* time_cmd = ap_cmd(parser, "time");
    ap_helptext(time_cmd, "Usage: pyro time [files]");
    ap_callback(time_cmd, cmd_time_callback);
    ap_int_opt(time_cmd, "num-iterations n", 1000);

    ap_parse(parser, argc, argv);

    if (!ap_has_cmd(parser)) {
        if (ap_count_args(parser) > 0) {
            run_file(parser);
        } else {
            run_repl();
        }
    }

    ap_free(parser);
    return 0;
}
