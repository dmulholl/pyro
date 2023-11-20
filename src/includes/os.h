#ifndef pyro_os_h
#define pyro_os_h

// This file quarantines all functions with OS-specific dependencies.
// Currently only POSIX systems are supported.

// Returns true if a file or directory exists at [path].
// If [path] is a symlink, returns true if the target of the link exists.
bool pyro_exists(const char* path);

// Returns true if a directory exists at [path].
// If [path] is a symlink, returns true if the target of the link is an existing directory.
bool pyro_is_dir(const char* path);

// Returns true if a regular file exists at [path].
// If [path] is a symlink, returns true if the target of the link is an existing file.
bool pyro_is_file(const char* path);

// Returns true if a symlink exists at [path].
// (This checks if the symlink itself exists, not its target.)
bool pyro_is_symlink(const char* path);

// Deletes the symlink or file or directory at [path]. If [path] is a symlink, the link itself
// will be deleted, not its target. If [path] is a directory, its content will be recursively
// deleted, then the directory itself.
int pyro_remove(const char* path);

// Suspends the calling thread for the requested duration.
// Returns 0 if successful, -1 if an error occurs.
int pyro_sleep(double time_in_seconds);

// Wrapper for POSIX getcwd(). Returns NULL if an error occurs. The returned string should be
// freed with free().
char* pyro_getcwd(void);

// Reads directory entries into a vector. Skips '.' and '..'. Panics if there was a memory-
// allocation or I/O read error.
PyroVec* pyro_listdir(PyroVM* vm, const char* path);

// Wrapper for POSIX realpath().
// - Returns NULL on failure.
// - Returns a new null-terminated, heap-allocated string on success.
// - If the return value is not NULL, the caller should free the new string using free().
char* pyro_realpath(const char* path);

// Wrapper for POSIX setenv(). Returns true on success, false on failure.
bool pyro_setenv(const char* name, const char* value);

// Wrapper for POSIX chdir(). Returns true on success, false on failure.
bool pyro_chdir(const char* path);

// Wrapper for POSIX chroot(). Returns true on success, false on failure.
bool pyro_chroot(const char* path);

// Executes [cmd] via execvp().
// - [cmd] is a null-terminated string specifying an executable file.
// - If [cmd] contains a '/', it will be treated as a filepath.
// - If [cmd] does not contain a '/', it will be searched for on PATH.
// - [argv] is null-terminated array of null-terminated strings.
// - The first entry of [argv] should be [cmd].
// - If [stdin_input_length > 0], [stdin_input] will be written to the program's
//   standard input stream.
// - Returns the program's stdout output as [stdout_output].
// - Returns the program's stderr output as [stderr_output].
// - Returns the program's exit code as [exit_code].
// - If [cmd] is not found, the exit code will be non-zero and [stderr_ouput]
//   will contain an error message.
// If the return value is false, the function has panicked.
bool pyro_exec_cmd(
    PyroVM* vm,
    const char* cmd,
    char* const argv[],
    const uint8_t* stdin_input,
    size_t stdin_input_length,
    PyroStr** stdout_output,
    PyroStr** stderr_output,
    int* exit_code
);

// Attempts to load a dynamic library as a Pyro module. Can panic or set the exit flag.
// Caller should check [vm->halt_flag] immediately on return.
void pyro_dlopen_as_module(PyroVM* vm, const char* path, const char* mod_name, PyroMod* module);

#endif
