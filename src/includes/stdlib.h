#ifndef pyro_stdlib_h
#define pyro_stdlib_h

// Standard library modules.
void pyro_load_stdlib_module_constants(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_math(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_prng(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_csrng(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_pyro(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_fs(PyroVM* vm, PyroMod* module);
void pyro_load_stdlib_module_log(PyroVM* vm, PyroMod* module);

#endif
