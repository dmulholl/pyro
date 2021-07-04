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
void pyro_run_time_funcs(PyroVM* vm, size_t num_iterations);

int pyro_get_exit_code(PyroVM* vm);
bool pyro_get_exit_flag(PyroVM* vm);
bool pyro_get_panic_flag(PyroVM* vm);
bool pyro_get_halt_flag(PyroVM* vm);
bool pyro_get_memory_error_flag(PyroVM* vm);

// This function sets the content of the global $args variable, a tuple of strings.
void pyro_set_args(PyroVM* vm, int argc, char** argv);

// This function appends an entry to the list of directories checked when attempting to import a
// module. [path] should be a directory path, optionally ending with a single trailing slash '/'.
// Use "." for the current working directory, "/" for the root directory.
void pyro_add_import_root(PyroVM* vm, const char* path);

#endif
