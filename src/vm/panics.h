#ifndef pyro_panics_h
#define pyro_panics_h

#include "common.h"

// This function triggers a panic. It:
// - Sets vm->panic_flag to true.
// - Sets vm->halt_flag to true.
// - Sets vm->status_code to [error_code].
// - Prints the error message.
void pyro_panic(PyroVM* vm, int64_t error_code, const char* format, ...);

// This function triggers a custom panic for syntax errors. It's only used by the compiler.
void pyro_syntax_error(PyroVM* vm, const char* source_id, size_t source_line, const char* format, ...);

#endif
