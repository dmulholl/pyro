#ifndef pyro_std_lib_h
#define pyro_std_lib_h

#include "../vm/common.h"
#include "../vm/values.h"

// Global functions and variables.
void pyro_load_std_core(PyroVM* vm);

// Builtin types.
void pyro_load_std_core_map(PyroVM* vm);
void pyro_load_std_core_vec(PyroVM* vm);
void pyro_load_std_core_tup(PyroVM* vm);
void pyro_load_std_core_str(PyroVM* vm);
void pyro_load_std_core_buf(PyroVM* vm);
void pyro_load_std_core_file(PyroVM* vm);
void pyro_load_std_core_range(PyroVM* vm);
void pyro_load_std_core_iter(PyroVM* vm);
void pyro_load_std_core_queue(PyroVM* vm);

// The format function is defined in std_core.c but used by a number of builtin types.
Value pyro_fn_fmt(PyroVM* vm, size_t arg_count, Value* args);

// Standard library modules.
void pyro_load_std_errors(PyroVM* vm);
void pyro_load_std_math(PyroVM* vm);
void pyro_load_std_path(PyroVM* vm);
void pyro_load_std_prng(PyroVM* vm);
void pyro_load_std_pyro(PyroVM* vm);

#endif
