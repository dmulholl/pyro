#ifndef pyro_io_h
#define pyro_io_h

#include "pyro.h"

// Functions with an `_f` suffix support printf-style formatting.
// All these functions:
// - Return the number of bytes written on success.
// - Return -1 if an attempt to write to a file failed.

// [stdout] functions are a no-op if [vm->stdout_file] is NULL.
int64_t pyro_stdout_write(PyroVM* vm, const char* string);
int64_t pyro_stdout_write_n(PyroVM* vm, const char* string, size_t count);
int64_t pyro_stdout_write_s(PyroVM* vm, PyroStr* string);
int64_t pyro_stdout_write_f(PyroVM* vm, const char* format_string, ...);
int64_t pyro_stdout_write_fv(PyroVM* vm, const char* format_string, va_list args);

// [stderr] functions are a no-op if [vm->stderr_file] is NULL.
int64_t pyro_stderr_write(PyroVM* vm, const char* string);
int64_t pyro_stderr_write_n(PyroVM* vm, const char* string, size_t count);
int64_t pyro_stderr_write_s(PyroVM* vm, PyroStr* string);
int64_t pyro_stderr_write_f(PyroVM* vm, const char* format_string, ...);
int64_t pyro_stderr_write_fv(PyroVM* vm, const char* format_string, va_list args);

#endif
