#ifndef pyro_std_lib_h
#define pyro_std_lib_h

#include "../vm/pyro.h"
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
void pyro_load_std_mod_math(PyroVM* vm, ObjModule* module);
void pyro_load_std_mod_mt64(PyroVM* vm, ObjModule* module);
void pyro_load_std_mod_prng(PyroVM* vm, ObjModule* module);
void pyro_load_std_mod_pyro(PyroVM* vm, ObjModule* module);
void pyro_load_std_mod_path(PyroVM* vm, ObjModule* module);
void pyro_load_std_mod_sqlite(PyroVM* vm, ObjModule* module);

// Embedded source code for modules written in Pyro.
extern unsigned char lib_args_pyro[];
extern unsigned int lib_args_pyro_len;
extern unsigned char lib_email_pyro[];
extern unsigned int lib_email_pyro_len;
extern unsigned char lib_html_pyro[];
extern unsigned int lib_html_pyro_len;

extern unsigned char lib_cgi_pyro[];
extern unsigned int lib_cgi_pyro_len;
#endif
