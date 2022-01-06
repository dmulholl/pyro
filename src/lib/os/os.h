#ifndef pyro_os_h
#define pyro_os_h

#include "../../vm/pyro.h"

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

// Wrapper for POSIX dirname(). May modify the input string.
// NB: It isn't safe to call free() on the return value from this function as it may point to
// statically allocated memory.
char* pyro_dirname(char* path);

// Wrapper for POSIX basename(). May modify the input string.
// NB: It isn't safe to call free() on the return value from this function as it may point to
// statically allocated memory.
char* pyro_basename(char* path);

// Equivalent to `rm -rf` in the shell.
// Deletes the symlink or file or directory at [path]. If [path] is a symlink, the link itself will
// be deleted, not its target. If [path] is a directory, its content will be recursively deleted,
// then the directory itself.
int pyro_rmrf(const char* path);

// Suspends the calling thread for the requested duration.
// Returns 0 if successful, -1 if an error occurs.
int pyro_sleep(double time_in_seconds);

// Wrapper for POSIX getcwd(). Returns NULL if an error occurs. The returned string should be freed
// with free().
char* pyro_getcwd(void);

// Reads directory entries into a vector. Skips '.' and '..'. Panics if there was a memory-
// allocation or I/O read error.
ObjVec* pyro_listdir(PyroVM* vm, const char* path);

typedef struct {
    ObjStr* output;
    int exit_code;
} ShellCmdResult;

// Runs a shell command. Returns true if the attempt to run the command was successful. Panics and
// returns false if there was a memory-allocation or I/O read error.
bool pyro_run_shell_cmd(PyroVM* vm, const char* cmd, ShellCmdResult* out);

// Wrapper for POSIX realpath(). Returns NULL on failure, a freshly allocated string on success.
// If the return value is non-NULL, the caller should free it using free().
char* pyro_realpath(const char* path);

// Wrapper for POSIX strdup().
char* pyro_strdup(const char* source);

#endif
