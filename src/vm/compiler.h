#ifndef pyro_compiler_h
#define pyro_compiler_h

#include "common.h"
#include "values.h"
#include "objects.h"

ObjFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, ObjModule* mod);
void pyro_mark_compiler_roots(PyroVM* vm);

#endif
