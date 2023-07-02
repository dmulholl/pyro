#include "cli.h"


void pyro_cli_run_module(ArgParser* parser) {
    size_t stack_size = pyro_cli_get_stack_size(parser);

    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        exit(1);
    }

    // Set the VM's maximum memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add import roots.
    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);

    // Add the command line args to the global $args variable.
    char** args = ap_get_str_values(parser, "module");
    pyro_set_args(vm, ap_count(parser, "module"), args);
    free(args);

    // Set the trace-execution flag.
    pyro_set_trace_execution_flag(vm, ap_found(parser, "trace-execution"));

    // Compile and execute the script.
    pyro_import_module_from_string(vm, ap_get_str_value_at_index(parser, "module", 0), vm->main_module);
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
