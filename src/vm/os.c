#include "os.h"

// These headers are POSIX only.
#include <dirent.h>
#include <unistd.h>


bool pyro_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}


bool pyro_dir_exists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}
