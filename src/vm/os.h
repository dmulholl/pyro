#ifndef pyro_os_h
#define pyro_os_h

#include "common.h"

// Returns [true] if a file exists at [path].
bool pyro_file_exists(const char* path);

// Returns [true] if a directory exists at [path].
bool pyro_dir_exists(const char* path);

#endif
