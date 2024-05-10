#include "cli.h"


int pyro_cli_cmd_bake(char* cmd_name, ArgParser* cmd_parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, unable to initialize the Pyro VM\n");
        return 1;
    }

    if (ap_count_args(cmd_parser) == 0) {
        fprintf(stderr, "error: missing script argument\n");
        pyro_free_vm(vm);
        return 1;
    }

    char* script_arg = ap_get_arg_at_index(cmd_parser, 0);
    PyroStr* script_string = PyroStr_COPY(script_arg);
    if (!script_string) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_define_superglobal(vm, "SCRIPT", pyro_obj(script_string))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    char* version_arg = ap_get_str_value(cmd_parser, "pyro-version");
    PyroStr* version_string = PyroStr_COPY(version_arg);
    if (!version_string) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_define_superglobal(vm, "PYRO_VERSION", pyro_obj(version_string))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    char* repo_arg = ap_get_str_value(cmd_parser, "pyro-repo");
    PyroStr* repo_string = PyroStr_COPY(repo_arg);
    if (!repo_string) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_define_superglobal(vm, "PYRO_REPO", pyro_obj(repo_string))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    char* output_arg = ap_get_str_value(cmd_parser, "output");
    PyroStr* output_string = PyroStr_COPY(output_arg);
    if (!output_string) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    if (!pyro_define_superglobal(vm, "OUTPUT", pyro_obj(output_string))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    bool no_cache_arg = ap_found(cmd_parser, "no-cache");
    if (!pyro_define_superglobal(vm, "NO_CACHE", pyro_bool(no_cache_arg))) {
        fprintf(stderr, "error: out of memory\n");
        pyro_free_vm(vm);
        return 1;
    }

    const unsigned char* code;
    size_t code_length;

    if (!pyro_find_embedded_file("std/cli/bake.pyro", &code, &code_length)) {
        fprintf(stderr, "error: failed to load 'embed/std/cli/bake.pyro'\n");
        pyro_free_vm(vm);
        return 1;
    }

    pyro_exec_code(vm, (const char*)code, code_length, "std::cli::bake", NULL);
    if (pyro_get_exit_flag(vm) || pyro_get_panic_flag(vm)) {
        int64_t exit_code = pyro_get_exit_code(vm);
        pyro_free_vm(vm);
        exit(exit_code);
    }

    pyro_free_vm(vm);
    return 0;
}
