#ifndef pyro_h
#define pyro_h

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Language version.
#define PYRO_VERSION_MAJOR 0
#define PYRO_VERSION_MINOR 5
#define PYRO_VERSION_PATCH 4
#define PYRO_VERSION_STRING "0.5.4"

// Selects the hash function for strings.
#ifndef PYRO_STRING_HASH
    #define PYRO_STRING_HASH pyro_fnv1a_64_opt
#endif

// Sets the (count/capacity) threshold for map resizing.
#ifndef PYRO_MAX_HASHMAP_LOAD
    #define PYRO_MAX_HASHMAP_LOAD 0.5
#endif

// Max number of values on the stack. Each value is 16 bytes so the default size is 256KB.
#ifndef PYRO_STACK_SIZE
    #define PYRO_STACK_SIZE (1024 * 16)
#endif

// Max number of call frames.
#ifndef PYRO_MAX_CALL_FRAMES
    #define PYRO_MAX_CALL_FRAMES 1024
#endif

// Initial garbage collection threshold in bytes. Defaults to 1MB.
#ifndef PYRO_INIT_GC_THRESHOLD
    #define PYRO_INIT_GC_THRESHOLD (1024 * 1024)
#endif

// This value determines the interval between garbage collections. After each garbage collection
// the threshold for the next collection is set to vm->bytes_allocated * PYRO_GC_HEAP_GROW_FACTOR.
#ifndef PYRO_GC_HEAP_GROW_FACTOR
    #define PYRO_GC_HEAP_GROW_FACTOR 2
#endif

// Pi to the maximum accuracy limit of 64-bit IEEE 754 floats.
#ifndef PYRO_PI
    #define PYRO_PI 3.14159265358979323846
#endif

// Euler's constant to the maximum accuracy limit of 64-bit IEEE 754 floats.
#ifndef PYRO_E
    #define PYRO_E 2.7182818284590452354
#endif

// Size in bytes of the buffer for recording panic messages inside try expressions.
#ifndef PYRO_PANIC_BUFFER_SIZE
    #define PYRO_PANIC_BUFFER_SIZE 256
#endif

// All runtime state is stored on a PyroVM instance.
typedef struct PyroVM PyroVM;

// Error codes returned by the VM.
typedef enum {
    ERR_OK,
    ERR_ERROR,
    ERR_OUT_OF_MEMORY,
    ERR_OS_ERROR,
    ERR_ARGS_ERROR,
    ERR_ASSERTION_FAILED,
    ERR_TYPE_ERROR,
    ERR_NAME_ERROR,
    ERR_VALUE_ERROR,
    ERR_MODULE_NOT_FOUND,
    ERR_SYNTAX_ERROR,
} PyroErrorCode;

// Initializes a new VM. Returns NULL if the attempt to allocate memory for the VM fails.
PyroVM* pyro_new_vm(void);

// Frees a VM instance and all heap-allocated memory owned by that VM.
void pyro_free_vm(PyroVM* vm);

// Sets the file to use for error output. Defaults to [stderr].
void pyro_set_err_file(PyroVM* vm, FILE* file);

// Sets the file to use for standard output. Defaults to [stdout].
void pyro_set_out_file(PyroVM* vm, FILE* file);

// Sets the maximum memory allocation for the VM in bytes.
void pyro_set_max_memory(PyroVM* vm, size_t bytes);

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

// Attempts to compile but not execute the file.
void pyro_test_compile_file(PyroVM* vm, const char* path);

// Returns the VM's status code.
int64_t pyro_get_status_code(PyroVM* vm);

// Returns the VM's exit flag.
bool pyro_get_exit_flag(PyroVM* vm);

// Returns the VM's panic flag.
bool pyro_get_panic_flag(PyroVM* vm);

// Returns the status of the VM's hard panic flag.
bool pyro_get_hard_panic_flag(PyroVM* vm);

// This function sets the content of the global $args variable, a tuple of strings. Returns [true]
// on success, [false] if memory could not be allocated for the tuple.
bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args);

// This function appends an entry to the list of directories checked when attempting to import a
// module. [path] should be a directory path, optionally ending with a single trailing slash '/'.
// Use "." for the current working directory, "/" for the root directory. Returns [true] if the
// entry was successfully added, [false] if memory could not be allocated for the entry.
bool pyro_add_import_root(PyroVM* vm, const char* path);

#endif
