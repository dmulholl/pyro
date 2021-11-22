#ifndef pyro_std_core_h
#define pyro_std_core_h

#include "../vm/common.h"
#include "../vm/values.h"

void pyro_load_std_core(PyroVM* vm);

void pyro_load_std_core_map(PyroVM* vm);
void pyro_load_std_core_vec(PyroVM* vm);
void pyro_load_std_core_tup(PyroVM* vm);
void pyro_load_std_core_str(PyroVM* vm);
void pyro_load_std_core_buf(PyroVM* vm);
void pyro_load_std_core_file(PyroVM* vm);
void pyro_load_std_core_range(PyroVM* vm);

Value pyro_fn_fmt(PyroVM* vm, size_t arg_count, Value* args);

#endif
