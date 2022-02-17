#ifndef pyro_exec_h
#define pyro_exec_h

#include "pyro.h"
#include "values.h"

// Executes a chunk of source code in the context of the VM's main module.
// - Outside of string literals, code should be utf-8 encoded.
// - String literals can contain arbitrary binary data including nulls and invalid utf-8.
// - [code] is treated as a byte array, not a C string, so no terminating null is required.
// - [code_length] is the length of [code] in bytes.
// - [source_id] is a null-terminated C string used to identify the code in error messages -- for
//   code loaded from a source file it should be the filepath.
void pyro_exec_code_as_main(PyroVM* vm, const char* code, size_t code_length, const char* source_id);

// Loads and executes a source file.
void pyro_exec_file_as_main(PyroVM* vm, const char* filepath);

// If [path] is a source file, loads and executes that file.
// If [path] is a module directory, loads and executes the file [path/self.pyro].
void pyro_exec_path_as_main(PyroVM* vm, const char* path);

// Executes a chunk of source code in the context of the specified module.
void pyro_exec_code_as_module(
    PyroVM* vm,
    const char* code,
    size_t code_length,
    const char* source_id,
    ObjModule* module
);

// Executes a file in the context of the specified module.
void pyro_exec_file_as_module(PyroVM* vm, const char* filepath, ObjModule* module);

// Runs the $main() function if it is defined in the main module.
void pyro_run_main_func(PyroVM* vm);

// Runs $test_ functions if any are defined in the main module.
void pyro_run_test_funcs(PyroVM* vm, int* passed, int* failed);

// Runs $time_ functions if any are defined in the main module.
void pyro_run_time_funcs(PyroVM* vm, size_t num_iterations);

// Attempts to compile but not execute the file.
void pyro_try_compile_file(PyroVM* vm, const char* path);

// Calls a method on a receiver. Returns the value returned by the method. Before calling this
// function the receiver and the method's arguments should be pushed onto the stack. These values
// (and the return value of the method) will be popped off the stack before this function returns.
// The called method can set the panic or exit flags so the caller should check the [vm->halt_flag]
// immediately on return before using the result. If the halt flag is set the caller should clean
// up any allocated resources and unwind the call stack.
//
// Checklist:
//  1. Push the receiver value.
//  2. Push the argument values.
//  3. Call this function.
//  4. Check [vm->halt_flag].
//
Value pyro_call_method(PyroVM* vm, Value method, uint8_t arg_count);

// Calls a function. Returns the value returned by the function. Before calling this function the
// function value itself and its arguments should be pushed onto the stack. These values (and the
// return value of the function) will be popped off the stack before this function returns. The
// called function can set the panic or exit flags so the caller should check the [vm->halt_flag]
// immediately on return before using the result. If the halt flag is set the caller should clean
// up any allocated resources and unwind the call stack.
//
// Checklist:
//  1. Push the function value.
//  2. Push the argument values.
//  3. Call this function.
//  4. Check [vm->halt_flag].
//
Value pyro_call_function(PyroVM* vm, uint8_t arg_count);

#endif
