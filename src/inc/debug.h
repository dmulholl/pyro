#ifndef pyro_debug_h
#define pyro_debug_h

#include "pyro.h"
#include "values.h"
#include "objects.h"

void pyro_disassemble_function(PyroVM* vm, ObjFn* fn);
size_t pyro_disassemble_instruction(PyroVM* vm, ObjFn* fn, size_t ip);

#endif
