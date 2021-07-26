#ifndef pyro_os_h
#define pyro_os_h

#include "common.h"

// Returns [true] if a file exists at [path].
bool pyro_file_exists(const char* path);

// Returns [true] if a directory exists at [path].
bool pyro_dir_exists(const char* path);

typedef struct {
    ObjStr* output;
    int exit_code;
} CmdResult;

bool pyro_run_shell_cmd(PyroVM* vm, const char* cmd, CmdResult* out);

#endif
