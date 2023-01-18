#ifndef pyro_std_lib_h
#define pyro_std_lib_h

// Global functions and variables.
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

// Standard library modules.
void pyro_load_std_mod_math(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_mt64(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_prng(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_pyro(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_path(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_sqlite(PyroVM* vm, PyroMod* module);
void pyro_load_std_mod_log(PyroVM* vm, PyroMod* module);

// Embedded source code for modules written in Pyro.
extern unsigned char std_mod_args_pyro[];
extern unsigned int std_mod_args_pyro_len;
extern unsigned char std_mod_email_pyro[];
extern unsigned int std_mod_email_pyro_len;
extern unsigned char std_mod_html_pyro[];
extern unsigned int std_mod_html_pyro_len;
extern unsigned char std_mod_cgi_pyro[];
extern unsigned int std_mod_cgi_pyro_len;
extern unsigned char std_mod_json_pyro[];
extern unsigned int std_mod_json_pyro_len;
extern unsigned char std_mod_pretty_pyro[];
extern unsigned int std_mod_pretty_pyro_len;

#endif
