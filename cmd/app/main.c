#include "../../src/includes/pyro.h"

int main(int argc, char* argv[]) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }

    if (!pyro_set_args(vm, (size_t)argc, argv)) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        exit(1);
    }

    const unsigned char* code;
    size_t code_length;

    if (!pyro_find_embedded_file("main.pyro", &code, &code_length)) {
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
