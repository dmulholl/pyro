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

void pyro_exec(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);
void pyro_exec_file(PyroVM* vm, const char* path);

void pyro_run_main_func(PyroVM* vm);
void pyro_run_test_funcs(PyroVM* vm, int* passed, int* failed);

int pyro_get_exit_code(PyroVM* vm);
bool pyro_get_exit_flag(PyroVM* vm);
bool pyro_get_panic_flag(PyroVM* vm);

void pyro_set_args(PyroVM* vm, int argc, char** argv);
void pyro_add_import_dir(PyroVM* vm, const char* path);

#endif
