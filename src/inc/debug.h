#ifndef pyro_debug_h
#define pyro_debug_h

void pyro_disassemble_function(PyroVM* vm, PyroFn* fn);
size_t pyro_disassemble_instruction(PyroVM* vm, PyroFn* fn, size_t ip);

#endif
