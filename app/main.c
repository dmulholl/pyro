#include "../inc/pyro.h"

int main(int argc, char* argv[]) {
    size_t stack_size = 1024 * 1024;

    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }

    pyro_set_args(vm, (size_t)argc, argv);

    const unsigned char* code;
    size_t code_length;

    if (!pyro_get_embedded("main.pyro", &code, &code_length)) {
        fprintf(stderr, "error: missing 'embed/main.pyro' file\n");
        exit(1);
    }

    pyro_exec_code(vm, (const char*)code, code_length, "main.pyro", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_run_main_func(vm);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
