#include "cli.h"
#include "../inc/exec.h"
#include "../inc/vm.h"


static void run_verbose_tests(ArgParser* cmd_parser) {
    double start_time = clock();
    int funcs_passed = 0;
    int funcs_failed = 0;
    int files_passed = 0;
    int files_failed = 0;

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "Error: invalid path '%s'.\n", path);
            exit(1);
        }

        printf("[  TEST  ]  %s\n", path);

        size_t stack_size = pyro_cli_get_stack_size(cmd_parser);
        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "Error: out of memory, unable to initialize Pyro VM.\n");
            exit(1);
        }

        pyro_cli_set_max_memory(vm, cmd_parser);
        pyro_cli_add_import_roots_from_command_line(vm, cmd_parser);
        pyro_cli_add_import_roots_from_environment(vm);
        pyro_cli_add_import_roots_from_path(vm, path);

        pyro_exec_path_as_main(vm, path);
        if (pyro_get_panic_flag(vm) || pyro_get_exit_code(vm) != 0) {
            printf("            - \x1B[1;31mFAIL\x1B[0m\n");
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        bool had_failed_test_func = false;

        for (size_t i = 0; i < vm->main_module->all_member_indexes->entry_array_count; i++) {
            PyroMapEntry* entry = &vm->main_module->all_member_indexes->entry_array[i];
            if (IS_TOMBSTONE(entry->key)) {
                continue;
            }

            Value member_name = entry->key;
            Value member_index = entry->value;
            Value member_value = vm->main_module->members->values[member_index.as.i64];

            if (IS_CLOSURE(member_value)) {
                ObjStr* name = AS_STR(member_name);
                if (name->length > 6 && memcmp(name->bytes, "$test_", 6) == 0) {
                    pyro_reset_vm(vm);
                    pyro_push(vm, member_value);
                    pyro_call_function(vm, 0);
                    if (pyro_get_panic_flag(vm) || pyro_get_exit_code(vm) != 0) {
                        had_failed_test_func = true;
                        funcs_failed += 1;
                        printf("            - \x1B[1;31m%s\x1B[0m\n", name->bytes);
                    } else {
                        funcs_passed += 1;
                    }
                }
            }
        }

        if (had_failed_test_func) {
            files_failed += 1;
        } else {
            files_passed += 1;
            printf("            - \x1B[1;32mPASS\x1B[0m\n");
        }

        pyro_free_vm(vm);
    }

    double time = (double)(clock() - start_time) / CLOCKS_PER_SEC;

    if (files_failed > 0) {
        printf("\n  \x1B[1;41;30m FAIL \x1B[0m  ");
    } else {
        printf("\n  \x1B[1;42;30m PASS \x1B[0m  ");
    }

    printf("  Files: %d/%d", files_passed, files_passed + files_failed);
    printf(" · Funcs: %d/%d", funcs_passed, funcs_passed + funcs_failed);
    printf(" · Time: %f secs\n", time);

    exit(files_failed > 0 ? 1 : 0);

}


static void run_quiet_tests(ArgParser* cmd_parser) {
    double start_time = clock();
    int funcs_passed = 0;
    int funcs_failed = 0;
    int files_passed = 0;
    int files_failed = 0;

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_arg(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "Error: invalid path '%s'.\n", path);
            exit(1);
        }

        printf("[  ····  ]  %s", path);
        fflush(stdout);

        size_t stack_size = pyro_cli_get_stack_size(cmd_parser);
        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "Error: out of memory, unable to initialize Pyro VM.\n");
            exit(1);
        }

        pyro_cli_set_max_memory(vm, cmd_parser);
        pyro_cli_add_import_roots_from_command_line(vm, cmd_parser);
        pyro_cli_add_import_roots_from_environment(vm);
        pyro_cli_add_import_roots_from_path(vm, path);
        pyro_set_stderr(vm, NULL);

        pyro_exec_path_as_main(vm, path);
        if (pyro_get_panic_flag(vm) || pyro_get_exit_code(vm) != 0) {
            printf("\r[  \x1B[1;31mFAIL\x1B[0m\n");
            files_failed += 1;
            pyro_free_vm(vm);
            continue;
        }

        bool had_failed_test_func = false;

        for (size_t i = 0; i < vm->main_module->all_member_indexes->entry_array_count; i++) {
            PyroMapEntry* entry = &vm->main_module->all_member_indexes->entry_array[i];
            if (IS_TOMBSTONE(entry->key)) {
                continue;
            }

            Value member_name = entry->key;
            Value member_index = entry->value;
            Value member_value = vm->main_module->members->values[member_index.as.i64];

            if (IS_CLOSURE(member_value)) {
                ObjStr* name = AS_STR(member_name);
                if (name->length > 6 && memcmp(name->bytes, "$test_", 6) == 0) {
                    pyro_reset_vm(vm);
                    pyro_push(vm, member_value);
                    pyro_call_function(vm, 0);
                    if (pyro_get_panic_flag(vm) || pyro_get_exit_code(vm) != 0) {
                        if (!had_failed_test_func) {
                            printf("\r[  \x1B[1;31mFAIL\x1B[0m\n");
                        }
                        had_failed_test_func = true;
                        funcs_failed += 1;
                        printf("            - \x1B[1;31m%s\x1B[0m\n", name->bytes);
                    } else {
                        funcs_passed += 1;
                    }
                }
            }
        }

        if (had_failed_test_func) {
            files_failed += 1;
        } else {
            files_passed += 1;
            printf("\r[  \x1B[1;32mPASS\x1B[0m\n");
        }

        pyro_free_vm(vm);
    }

    double time = (double)(clock() - start_time) / CLOCKS_PER_SEC;

    if (files_failed > 0) {
        printf("\n  \x1B[1;41;30m FAIL \x1B[0m  ");
    } else {
        printf("\n  \x1B[1;42;30m PASS \x1B[0m  ");
    }

    printf("  Files: %d/%d", files_passed, files_passed + files_failed);
    printf(" · Funcs: %d/%d", funcs_passed, funcs_passed + funcs_failed);
    printf(" · Time: %f secs\n", time);

    exit(files_failed > 0 ? 1 : 0);
}


void pyro_cmd_test(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return;
    }
    if (ap_found(cmd_parser, "verbose")) {
        run_verbose_tests(cmd_parser);
    } else {
        run_quiet_tests(cmd_parser);
    }
}
