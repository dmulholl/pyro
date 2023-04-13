#include "cli.h"


int pyro_cli_cmd_time(char* cmd_name, ArgParser* cmd_parser) {
    if (ap_count_args(cmd_parser) == 0) {
        return 0;
    }

    int num_runs = ap_get_int_value(cmd_parser, "num-runs");
    if (num_runs < 1) {
        fprintf(stderr, "Pyro CLI error: invalid argument for --num-runs.\n");
        exit(1);
    }

    double cmd_start_time = clock();

    for (int i = 0; i < ap_count_args(cmd_parser); i++) {
        char* path = ap_get_arg_at_index(cmd_parser, i);
        if (!pyro_exists(path)) {
            fprintf(stderr, "Pyro CLI error: invalid path '%s'.\n", path);
            exit(1);
        }

        printf("[  TIME  ]  %s\n", path);

        size_t stack_size = pyro_cli_get_stack_size(cmd_parser);
        PyroVM* vm = pyro_new_vm(stack_size);
        if (!vm) {
            fprintf(stderr, "Pyro CLI error: out of memory, unable to initialize Pyro VM.\n");
            exit(1);
        }

        pyro_cli_set_max_memory(vm, cmd_parser);
        pyro_cli_add_import_roots_from_command_line(vm, cmd_parser);
        pyro_cli_add_import_roots_from_environment(vm);
        pyro_cli_add_import_roots_from_path(vm, path);

        pyro_exec_path(vm, path, vm->main_module);
        if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_exit_code(vm));
        }

        size_t max_name_length = 0;

        for (size_t i = 0; i < vm->main_module->all_member_indexes->entry_array_count; i++) {
            PyroMapEntry* entry = &vm->main_module->all_member_indexes->entry_array[i];
            if (PYRO_IS_TOMBSTONE(entry->key)) {
                continue;
            }

            PyroValue member_name = entry->key;
            PyroValue member_index = entry->value;
            PyroValue member_value = vm->main_module->members->values[member_index.as.i64];

            if (PYRO_IS_CLOSURE(member_value)) {
                PyroStr* name = PYRO_AS_STR(member_name);
                if (name->count > 6 && memcmp(name->bytes, "$time_", 6) == 0) {
                    if (name->count > max_name_length) {
                        max_name_length = name->count;
                    }
                }
            }
        }

        for (size_t i = 0; i < vm->main_module->all_member_indexes->entry_array_count; i++) {
            PyroMapEntry* entry = &vm->main_module->all_member_indexes->entry_array[i];
            if (PYRO_IS_TOMBSTONE(entry->key)) {
                continue;
            }

            PyroValue member_name = entry->key;
            PyroValue member_index = entry->value;
            PyroValue member_value = vm->main_module->members->values[member_index.as.i64];

            if (PYRO_IS_CLOSURE(member_value)) {
                PyroStr* name = PYRO_AS_STR(member_name);
                if (name->count > 6 && memcmp(name->bytes, "$time_", 6) == 0) {
                    double start_time = clock();

                    for (int j = 0; j < num_runs; j++) {
                        pyro_reset_vm(vm);
                        pyro_push(vm, member_value);
                        pyro_call_function(vm, 0);
                        if (vm->halt_flag) {
                            pyro_free_vm(vm);
                            exit(1);
                        }
                    }

                    double total_runtime = (double)(clock() - start_time) / CLOCKS_PER_SEC;
                    double average_in_secs = total_runtime / num_runs;
                    double average_in_millisecs = average_in_secs * 1000;

                    printf("            - %s() ···", name->bytes);
                    for (size_t j = 0; j < max_name_length - name->count; j++) {
                        printf("·");
                    }

                    printf(" %.6f s", average_in_secs);

                    if (average_in_secs < 1.0) {
                        printf(" ··· ");
                        printf("%.3f ms", average_in_millisecs);
                    }

                    printf("\n");
                }
            }
        }

        pyro_free_vm(vm);
    }

    double time = (double)(clock() - cmd_start_time) / CLOCKS_PER_SEC;
    printf("\nTotal Runtime: %f secs\n", time);

    return 0;
}
