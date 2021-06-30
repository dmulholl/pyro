#ifndef pyro_h
#define pyro_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct PyroVM PyroVM;

PyroVM* pyro_new_vm();
void pyro_free_vm(PyroVM* vm);

void pyro_set_err_file(PyroVM* vm, FILE* file);
void pyro_set_out_file(PyroVM* vm, FILE* file);

void pyro_exec_code_as_main(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);
void pyro_exec_file_as_main(PyroVM* vm, const char* path);

void pyro_run_main_func(PyroVM* vm);
void pyro_run_test_funcs(PyroVM* vm, int* passed, int* failed);

int pyro_get_exit_code(PyroVM* vm);
bool pyro_get_exit_flag(PyroVM* vm);
bool pyro_get_panic_flag(PyroVM* vm);
bool pyro_get_halt_flag(PyroVM* vm);
bool pyro_get_mem_err_flag(PyroVM* vm);

void pyro_set_args(PyroVM* vm, int argc, char** argv);

// Append an entry to the list of directories checked when attempting to import a module.
// [path] should be a directory path without a trailing slash. Use "." for the current working
// directory, "" for the root directory.
void pyro_add_import_root(PyroVM* vm, const char* path);

#endif
