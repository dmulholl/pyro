#ifndef pyro_compiler_h
#define pyro_compiler_h

#include "pyro.h"
#include "values.h"
#include "objects.h"

// This function will panic and return NULL if a syntax error occurs or if memory allocation fails.
PyroFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);

#endif
