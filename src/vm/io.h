#ifndef pyro_io_h
#define pyro_io_h

#include "pyro.h"

// Writes a printf-style formatted string to the VM's standard output stream.
// This is a no-op if [vm->stdout_stream] is NULL.
// - On success, returns the number of bytes written.
// - Returns -1 if an attempt to write to a file failed.
// - Returns -2 if an attempt to write to a buffer failed.
int64_t pyro_write_stdout(PyroVM* vm, const char* format_string, ...);
int64_t pyro_write_stdout_v(PyroVM* vm, const char* format_string, va_list args);

// Writes a printf-style formatted string to the VM's standard error stream.
// This is a no-op if [vm->stderr_stream] is NULL.
// - On success, returns the number of bytes written.
// - Returns -1 if an attempt to write to a file failed.
// - Returns -2 if an attempt to write to a buffer failed.
int64_t pyro_write_stderr(PyroVM* vm, const char* format_string, ...);
int64_t pyro_write_stderr_v(PyroVM* vm, const char* format_string, va_list args);

#endif
