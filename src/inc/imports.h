#ifndef pyro_imports_h
#define pyro_imports_h

// This function attempts to import the module specified by the import path [args], an array of
// [arg_count] PyroStr* values.
//
// It looks for a file or directory matching the import path, compiles the file if it finds one,
// then executes the compiled code in the context of the supplied [module] object. (This module
// object has already been inserted into the module tree to enable circular imports.)
//
// This function can set the panic or exit flags. It will panic if the module cannot be found, if
// the module code contains syntax errors, if the VM runs out of memory, or if executing the code
// results in a panic.
void pyro_import_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module);

#endif
