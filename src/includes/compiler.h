#ifndef pyro_compiler_h
#define pyro_compiler_h

// Compiles source code into bytecode.
// - Panics if [src_code] contains a syntax error or if memory allocation fails.
// - Caller should check [vm->halt_flag] immediately before using the result.
// - [src_code] does not require a terminating null byte.
// - [src_len] is the number of bytes in [src_code], excluding any terminating null.
// - [src_id] is used to identify the source code in error messages (e.g. its file path).
// - [dump_bytecode] is a debugging flag for dumping the compiled bytecode to stdout.
PyroFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, bool dump_bytecode);

// Compiles a single expression into a function that returns the expression's value.
// - Panics if [src_code] contains a syntax error or if memory allocation fails.
// - Caller should check [vm->halt_flag] immediately before using the result.
// - [src_code] does not require a terminating null byte.
// - [src_len] is the number of bytes in [src_code], excluding any terminating null.
// - [src_id] is used to identify the source code in error messages.
PyroFn* pyro_compile_expression(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);

#endif
