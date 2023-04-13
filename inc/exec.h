#ifndef pyro_exec_h
#define pyro_exec_h

// Executes a chunk of source code in the context of the specified module.
// - Outside of string literals, code should be utf-8 encoded.
// - String literals can contain arbitrary binary data including nulls and invalid utf-8.
// - [code] is treated as a byte array, not a C string, so no terminating null is required.
// - [code_length] is the length of [code] in bytes.
// - [source_id] is a null-terminated C string used to identify the code in error messages -- for
//   code loaded from a source file it should be the filepath.
void pyro_exec_code(PyroVM* vm, const char* code, size_t code_length, const char* source_id, PyroMod* module);

// Loads and executes a source file in the context of the specified module.
void pyro_exec_file(PyroVM* vm, const char* filepath, PyroMod* module);

// Loads and executes a source file in the context of the specified module.
// - If [path] is a source file, loads and executes that file.
// - If [path] is a directory, loads and executes the file [path/self.pyro].
void pyro_exec_path(PyroVM* vm, const char* path, PyroMod* module);

// Runs the $main() function if it is defined in the VM's main module.
void pyro_run_main_func(PyroVM* vm);

// Attempts to compile but not execute the file.
void pyro_try_compile_file(PyroVM* vm, const char* path);

// Calls a value as a method on a receiver, where [method] is a value returned by pyro_get_method()
// containing one of:
//
// - PyroNativeFn
// - PyroClosure
//
// Returns the value returned by the method.
//
// Before calling this function the receiver and the method's arguments should be pushed onto the
// stack. These values (and the return value of the method) will be popped off the stack before this
// function returns.
//
// The called method can set the panic or exit flags so the caller should check [vm->halt_flag]
// immediately on return before using the result. If the halt flag is set the caller should clean
// up any allocated resources and unwind the call stack.
//
// Checklist:
//  1. Push the receiver value onto the stack.
//  2. Push the argument values onto the stack.
//  3. Call this function.
//  4. Check [vm->halt_flag].
//
PyroValue pyro_call_method(PyroVM* vm, PyroValue method, uint8_t arg_count);

// Calls a value as a function, where the value contains one of:
//
// - PyroNativeFn
// - PyroClosure
// - PyroBoundMethod
// - PyroClass
// - PyroInstance
//
// Returns the value returned by the function.
//
// Before calling this function the value itself and its arguments should be pushed onto the stack.
// These values (and the return value of the function) will be popped off the stack before this
// function returns.
//
// The called function can set the panic or exit flags so the caller should check [vm->halt_flag]
// immediately on return before using the result. If the halt flag is set the caller should clean
// up any allocated resources and unwind the call stack.
//
// Checklist:
//  1. Push the callee value onto the stack.
//  2. Push the argument values onto the stack.
//  3. Call this function.
//  4. Check [vm->halt_flag].
//
PyroValue pyro_call_function(PyroVM* vm, uint8_t arg_count);

// Call this function to reset the VM after a panic.
// - Resets the flags, the stack pointer, and the frame pointer.
// - Does not reset the main module or the imported module cache.
void pyro_reset_vm(PyroVM* vm);

#endif
