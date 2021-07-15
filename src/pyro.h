#ifndef pyro_h
#define pyro_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct PyroVM PyroVM;

// Initializes a new VM. Returns NULL if the attempt to allocate memory for the VM fails.
PyroVM* pyro_new_vm(void);

// Frees a VM instance and all heap-allocated memory owned by that VM.
void pyro_free_vm(PyroVM* vm);

// Sets the file to use for error output. Defaults to [stderr].
void pyro_set_err_file(PyroVM* vm, FILE* file);

// Sets the file to use for standard output. Defaults to [stdout].
void pyro_set_out_file(PyroVM* vm, FILE* file);

// Executes a chunk of code in the context of the VM's main module. Outside of string literals the
// code should be utf-8 encoded. String literals can contain arbitrary binary data including nulls
// and invalid utf-8. [src_code] is treated as a byte array not a C string so no terminating null
// is required. [src_len] should be the length in bytes of [src_code]. [src_id] is a null-terminated
// C string used to identify the code in error messages -- for code loaded from a source file it
// should be the filepath.
void pyro_exec_code_as_main(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);

// Convenience function for loading and executing a source file in the context of the VM's main
// module.
void pyro_exec_file_as_main(PyroVM* vm, const char* path);

// Runs the $main() function if it is defined in the main module.
void pyro_run_main_func(PyroVM* vm);

// Runs $test_ functions if any are defined in the main module.
void pyro_run_test_funcs(PyroVM* vm, int* passed, int* failed);

// Runs $time_ functions if any are defined in the main module.
void pyro_run_time_funcs(PyroVM* vm, size_t num_iterations);

// Returns the VM's exit code.
int pyro_get_exit_code(PyroVM* vm);

// Returns the status of the VM's exit flag.
bool pyro_get_exit_flag(PyroVM* vm);

// Returns the status of the VM's panic flag.
bool pyro_get_panic_flag(PyroVM* vm);

// Returns the status of the VM's halt flag.
bool pyro_get_halt_flag(PyroVM* vm);

// Returns the status of the VM's memory_error flag.
bool pyro_get_memory_error_flag(PyroVM* vm);

// This function sets the content of the global $args variable, a tuple of strings. Returns [true]
// on success, [false] if memory could not be allocated for the tuple.
bool pyro_set_args(PyroVM* vm, size_t argc, char** argv);

// This function appends an entry to the list of directories checked when attempting to import a
// module. [path] should be a directory path, optionally ending with a single trailing slash '/'.
// Use "." for the current working directory, "/" for the root directory. Returns [true] if the
// entry was successfully added, [false] if memory could not be allocated for the entry.
bool pyro_add_import_root(PyroVM* vm, const char* path);

#endif
