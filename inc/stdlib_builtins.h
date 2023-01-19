#ifndef pyro_stdlib_builtins_h
#define pyro_stdlib_builtins_h

// Superglobal functions and variables.
void pyro_load_std_builtins(PyroVM* vm);

// Builtin types.
void pyro_load_std_builtins_map(PyroVM* vm);
void pyro_load_std_builtins_vec(PyroVM* vm);
void pyro_load_std_builtins_tup(PyroVM* vm);
void pyro_load_std_builtins_str(PyroVM* vm);
void pyro_load_std_builtins_buf(PyroVM* vm);
void pyro_load_std_builtins_file(PyroVM* vm);
void pyro_load_std_builtins_range(PyroVM* vm);
void pyro_load_std_builtins_iter(PyroVM* vm);
void pyro_load_std_builtins_queue(PyroVM* vm);
void pyro_load_std_builtins_err(PyroVM* vm);
void pyro_load_std_builtins_mod(PyroVM* vm);
void pyro_load_std_builtins_char(PyroVM* vm);

// The format function is defined in std_core.c but used by a number of builtin types.
PyroValue pyro_fn_fmt(PyroVM* vm, size_t arg_count, PyroValue* args);

#endif
