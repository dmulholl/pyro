#ifndef pyro_stdlib_c_mods_h
#define pyro_stdlib_c_mods_h

// Standard library modules.
void pyro_load_std_mod_constants(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_math(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_prng(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_pyro(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_fs(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_sqlite(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_log(PyroVM* vm, PyroMod* module);

#endif
