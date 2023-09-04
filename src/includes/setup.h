#ifndef pyro_setup_h
#define pyro_setup_h

// Initializes a new VM. Returns NULL if memory allocation fails.
PyroVM* pyro_new_vm();

// Frees a VM instance and all heap-allocated memory owned by that VM.
void pyro_free_vm(PyroVM* vm);

// Sets the file to use for the VM's standard error stream. Returns false if a PyroFile cannot
// be allocated to wrap the stream.
bool pyro_set_stderr(PyroVM* vm, FILE* stream);

// Sets the file to use for the VM's standard output stream. Returns false if a PyroFile
// cannot be allocated to wrap the stream.
bool pyro_set_stdout(PyroVM* vm, FILE* stream);

// Sets the maximum memory allocation for the VM in bytes.
void pyro_set_max_memory(PyroVM* vm, size_t bytes);

// Returns the VM's exit code.
int64_t pyro_get_exit_code(PyroVM* vm);

// Returns the VM's exit flag.
bool pyro_get_exit_flag(PyroVM* vm);

// Returns the VM's panic flag.
bool pyro_get_panic_flag(PyroVM* vm);

// This function sets the content of the global $args variable, a tuple of strings. Returns
// true on success, false if memory could not be allocated for the tuple.
bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args);

// Appends an entry to the list of directories checked when attempting to import a module.
// - [path] should be a directory path, optionally ending in '/'.
// - Use "" or "." for the current working directory.
// - Use "/" for the root directory.
// - Returns true on success, false if memory could not be allocated for the entry.
bool pyro_add_import_root(PyroVM* vm, const char* path);
bool pyro_add_import_root_n(PyroVM* vm, const char* path, size_t length);

// Creates a new VM-level global variable. Returns true on success, false if memory could not
// be allocated.
bool pyro_define_superglobal(PyroVM* vm, const char* name, PyroValue value);

// Creates a new VM-level global variable pointing to a native function. Returns true on
// success, false if memory could not be allocated.
bool pyro_define_superglobal_fn(PyroVM* vm, const char* name, pyro_native_fn_t fn_ptr, int arity);

// Adds a new public member to [module], i.e. creates a module-level global variable called
// [name] with initial value [value]. Returns true on success, false if memory could not be
// allocated.
bool pyro_define_pub_member(PyroVM* vm, PyroMod* module, const char* name, PyroValue value);

// Adds a new private member to [module], i.e. creates a module-level global variable called
// [name] with initial value [value]. Returns true on success, false if memory could not be
// allocated.
bool pyro_define_pri_member(PyroVM* vm, PyroMod* module, const char* name, PyroValue value);

// Convenience function for adding a new public member to [module] where the value is a native
// function. Returns true on success, false if memory could not be allocated.
bool pyro_define_pub_member_fn(PyroVM* vm, PyroMod* module, const char* name, pyro_native_fn_t fn_ptr, int arity);

// Convenience function for adding a new private member to [module] where the value is a
// native function. Returns true on success, false if memory could not be allocated.
bool pyro_define_pri_member_fn(PyroVM* vm, PyroMod* module, const char* name, pyro_native_fn_t fn_ptr, int arity);

// Adds a new public method to the class. Returns true on success, false if memory could not
// be allocated.
bool pyro_define_pub_method(PyroVM* vm, PyroClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity);

// Adds a new private method to the class. Returns true on success, false if memory could not
// be allocated.
bool pyro_define_pri_method(PyroVM* vm, PyroClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity);

// Adds a new public field to the class. Returns true on success, false if memory could not be
// allocated.
bool pyro_define_pub_field(PyroVM* vm, PyroClass* class, const char* name, PyroValue default_value);

// Adds a new private field to the class. Returns true on success, false if memory could not be
// allocated.
bool pyro_define_pri_field(PyroVM* vm, PyroClass* class, const char* name, PyroValue default_value);

// Sets the value of the VM's REPL flag.
void pyro_set_repl_flag(PyroVM* vm, bool flag);

// Sets the value of the VM's trace_execution flag.
void pyro_set_trace_execution_flag(PyroVM* vm, bool flag);

#endif