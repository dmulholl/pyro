#ifndef pyro_imports_h
#define pyro_imports_h

// This function attempts to import the module specified by [args], an array of [arg_count > 0]
// module names corresponding to an import path, e.g. "foo::bar::baz" as ["foo", "bar", "baz"].
//
// It looks for a file matching the import path, compiles the file if it finds one, then executes
// the compiled code in the context of the supplied [module] object.
//
// This function can set the panic or exit flags. It will panic if the module cannot be found, if
// the module code contains syntax errors, if the VM runs out of memory, or if executing the code
// results in a panic.
void pyro_import_module(PyroVM* vm, uint8_t arg_count, PyroValue* args, PyroMod* module);

// This function attempts to import the module specified by [path], where [path] is a standard
// import path, e.g. "foo::bar::baz", or an arbitrary filepath ending in ".pyro".
//
// It looks for a file matching the import path, compiles the file if it finds one, then executes
// the compiled code in the context of the supplied [module] object.
//
// This function can set the panic or exit flags. It will panic if the module cannot be found, if
// the module code contains syntax errors, if the VM runs out of memory, or if executing the code
// results in a panic.
void pyro_import_module_from_path(PyroVM* vm, const char* path, PyroMod* module);

#endif
