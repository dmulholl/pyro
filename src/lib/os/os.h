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

// True if a file or directory exists at [path].
bool pyro_exists(const char* path);

// True if a directory exists at [path].
bool pyro_is_dir(const char* path);

// Wrapper for dirname().
char* pyro_dirname(char* path);

// Wrapper for popen().
FILE* pyro_popen(const char* command, const char* mode);

// Wrapper for pclose().
int pyro_pclose(FILE* stream);

#endif
