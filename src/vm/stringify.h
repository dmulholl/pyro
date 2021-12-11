#ifndef pyro_stringify_h
#define pyro_stringify_h

#include "common.h"
#include "values.h"

// Returns the debug string representation of [value].
// Panics and returns NULL if an error occurs.
// This function can call into Pyro code and can therefore set the exit flag.
// The caller should check [vm->halt_flag] immediately on return before using the result.
// If [vm->halt_flag] is false the return value is safe to use.
ObjStr* pyro_stringify_debug(PyroVM* vm, Value value);

#endif
