#include "../../src/includes/pyro.h"

int main(int argc, char* argv[]) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }

    if (!pyro_append_args(vm, argv, (size_t)argc)) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        exit(1);
    }

    PyroBuf* code = pyro_load_embedded_file(vm, "main.pyro");
    if (!code) {
        fprintf(stderr, "error: missing 'embed/main.pyro' file\n");
        pyro_free_vm(vm);
        exit(1);
    }

    if (!pyro_push(vm, pyro_obj(code))) {
        pyro_free_vm(vm);
        exit(1);
    }

    pyro_exec_code(vm, (const char*)code->bytes, code->count, "main.pyro", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int exit_code = (int)pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
