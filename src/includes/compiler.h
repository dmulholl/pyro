#ifndef pyro_compiler_h
#define pyro_compiler_h

// This function compiles source code into bytecode. It will panic and return NULL if the code
// contains a syntax error or if memory allocation fails.
// - [src_code] does not require a terminating-null.
// - [src_len] is the number of bytes in [src_code], excluding any terminating null.
// - [src_id] is used to identify the source code in error messages (e.g. its file path).
// - [dump_bytecode] is a debugging flag for dumping the compiled bytecode to stdout.
PyroFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, bool dump_bytecode);

#endif
