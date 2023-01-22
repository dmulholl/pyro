#ifndef pyro_stringify_h
#define pyro_stringify_h

// Prints to an automatically-allocated, null-terminated array using printf-style formatting.
// - Panics and returns NULL if a formatting error occurs.
// - Panics ard returns NULL if memory allocation fails.
// - If successful, the caller is responsible for freeing the returned array via:
//
//   PYRO_FREE_ARRAY(vm, char, string, strlen(string) + 1)
//
char* pyro_sprintf(PyroVM* vm, const char* format_string, ...);

// Prints to a new PyroStr instance using printf-style formatting.
// - Panics and returns NULL if a formatting error occurs.
// - Panics ard returns NULL if memory allocation fails.
PyroStr* pyro_sprintf_to_obj(PyroVM* vm, const char* format_string, ...);

// Returns the default string representation of [value].
// - Panics and returns NULL if an error occurs.
// - This function can call into Pyro code and can therefore set the exit flag.
// - The caller should check [vm->halt_flag] immediately on return before using the result.
// - If [vm->halt_flag] is false the return value is safe to use.
PyroStr* pyro_stringify_value(PyroVM* vm, PyroValue value);

// Returns the debug string representation of [value].
// - Panics and returns NULL if an error occurs.
// - This function can call into Pyro code and can therefore set the exit flag.
// - The caller should check [vm->halt_flag] immediately on return before using the result.
// - If [vm->halt_flag] is false the return value is safe to use.
PyroStr* pyro_debugify_value(PyroVM* vm, PyroValue value);

// Returns the formatted string representation of [value].
// - Panics and returns NULL if an error occurs.
// - This function can call into Pyro code and can therefore set the exit flag.
// - The caller should check [vm->halt_flag] immediately on return before using the result.
// - If [vm->halt_flag] is false the return value is safe to use.
// - [format_string] must be non-NULL and non-zero-length.
PyroStr* pyro_format_value(PyroVM* vm, PyroValue value, const char* format_string);

// Stringifies a double, stripping trailing zeros.
// - Panics and returns NULL if an error occurs.
// - [precision] specifies the maximum number of decimal digits after the decimal point.
PyroStr* pyro_stringify_f64(PyroVM* vm, double value, size_t precision);

// Returns a pointer to a static string.
char* pyro_stringify_object_type(PyroObjectType type);

// Interpolates an array of values into a format string.
// - This function provides the format-string backing for Pyro's $fmt(), $print(), etc, functions.
// - Panics and returns NULL if a formatting or memory-allocation error occurs.
// - This function can call into Pyro code which may set the panic and/or exit flags.
// - Caller should check [vm->halt_flag] immeditely on return.
// - The [caller] string is used as a prefix for error messages.
PyroStr* pyro_format(PyroVM* vm, PyroStr* format_string, size_t arg_count, PyroValue* args, const char* caller);

#endif