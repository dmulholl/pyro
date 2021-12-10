#ifndef pyro_setup_h
#define pyro_setup_h

#include "common.h"
#include "values.h"
#include "objects.h"

// Initializes a new VM. Returns NULL if the attempt to allocate memory for the VM fails.
PyroVM* pyro_new_vm(void);

// Frees a VM instance and all heap-allocated memory owned by that VM.
void pyro_free_vm(PyroVM* vm);

// Sets the file to use for error output. Defaults to [stderr].
void pyro_set_err_file(PyroVM* vm, FILE* file);

// Sets the file to use for standard output. Defaults to [stdout].
void pyro_set_out_file(PyroVM* vm, FILE* file);

// Sets the maximum memory allocation for the VM in bytes.
void pyro_set_max_memory(PyroVM* vm, size_t bytes);

// Returns the VM's status code.
int64_t pyro_get_status_code(PyroVM* vm);

// Returns the VM's exit flag.
bool pyro_get_exit_flag(PyroVM* vm);

// Returns the VM's panic flag.
bool pyro_get_panic_flag(PyroVM* vm);

// Returns the status of the VM's hard panic flag.
bool pyro_get_hard_panic_flag(PyroVM* vm);

// This function sets the content of the global $args variable, a tuple of strings. Returns [true]
// on success, [false] if memory could not be allocated for the tuple.
bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args);

// This function appends an entry to the list of directories checked when attempting to import a
// module. [path] should be a directory path, optionally ending with a single trailing slash '/'.
// Use "." for the current working directory, "/" for the root directory. Returns [true] if the
// entry was successfully added, [false] if memory could not be allocated for the entry.
bool pyro_add_import_root(PyroVM* vm, const char* path);

// Creates a new VM-level global variable. [value] is shielded from garbage-collection for the
// duration of this function.
void pyro_define_global(PyroVM* vm, const char* name, Value value);

// Creates a new VM-level global variable pointing to a native function.
void pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity);

// Adds a new member to [module], i.e. creates a module-level global variable called [name] with
// initial value [value]. [value] is shielded from garbage-collection while inside this function.
void pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value value);

// Convenience function for adding a new member to [module] where the value is a native function.
void pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity);

// Adds a new method to the class.
void pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity);

// Creates a new top-level module. Returns the module or [NULL] if memory allocation fails.
ObjModule* pyro_define_module_1(PyroVM* vm, const char* name);

// Defines a new 2nd level module where [parent] is a top-level module.
// Returns the module or [NULL] if memory allocation fails.
ObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name);

// Defines a new 3rd level module where [grandparent] is a top-level module and [parent] is a
// submodule of [grandparent]. Returns the module or [NULL] if memory allocation fails.
ObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name);

#endif
