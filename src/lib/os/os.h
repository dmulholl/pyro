#ifndef pyro_os_h
#define pyro_os_h

// C standard library. (No OS-dependent headers.)
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

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

// Wrapper for popen().
FILE* pyro_popen(const char* command, const char* mode);

// Wrapper for pclose().
int pyro_pclose(FILE* stream);

#endif
