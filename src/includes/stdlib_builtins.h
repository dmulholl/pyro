#ifndef pyro_stdlib_builtins_h
#define pyro_stdlib_builtins_h

// Superglobal functions and variables.
void pyro_load_superglobals(PyroVM* vm);

// Builtin types.
void pyro_load_builtin_type_map(PyroVM* vm);
void pyro_load_builtin_type_vec(PyroVM* vm);
void pyro_load_builtin_type_tup(PyroVM* vm);
void pyro_load_builtin_type_str(PyroVM* vm);
void pyro_load_builtin_type_buf(PyroVM* vm);
void pyro_load_builtin_type_file(PyroVM* vm);
void pyro_load_builtin_type_range(PyroVM* vm);
void pyro_load_builtin_type_iter(PyroVM* vm);
void pyro_load_builtin_type_queue(PyroVM* vm);
void pyro_load_builtin_type_err(PyroVM* vm);
void pyro_load_builtin_type_module(PyroVM* vm);
void pyro_load_builtin_type_rune(PyroVM* vm);
void pyro_load_builtin_type_enum(PyroVM* vm);

#endif
