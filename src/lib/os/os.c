#include "os.h"

// POSIX: opendir(), closedir()
#include <dirent.h>

// POSIX: access()
#include <unistd.h>

// POSIX: popen(), pclose()
#include <stdio.h>

// POSIX: dirname()
#include <libgen.h>


bool pyro_exists(const char* path) {
    return access(path, F_OK) == 0;
}


bool pyro_is_dir(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}


char* pyro_dirname(char* path) {
    return dirname(path);
}


FILE* pyro_popen(const char* command, const char* mode) {
    return popen(command, mode);
}


int pyro_pclose(FILE* stream) {
    return pclose(stream);
}
