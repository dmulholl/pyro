#ifndef pyro_io_h
#define pyro_io_h

#include "pyro.h"

// Writes a printf-style formatted string to the VM's standard output stream.
// This is a no-op if [vm->stdout_stream] is NULL.
bool pyro_write_stdout(PyroVM* vm, const char* format_string, ...);
bool pyro_write_stdout_v(PyroVM* vm, const char* format_string, va_list args);

// Writes a printf-style formatted string to the VM's standard error stream.
// This is a no-op if [vm->stderr_stream] is NULL.
bool pyro_write_stderr(PyroVM* vm, const char* format_string, ...);
bool pyro_write_stderr_v(PyroVM* vm, const char* format_string, va_list args);

#endif
