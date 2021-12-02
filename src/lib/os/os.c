#include "os.h"

// POSIX: popen(), pclose()
#include <stdio.h>

// POSIX: dirname(), basename()
#include <libgen.h>

// POSIX: stat(), lstat(), S_ISDIR(), S_ISREG, S_ISLNK()
#include <sys/stat.h>

// POSIX: nftw()
#include <ftw.h>

// POSIX: getcwd()
#include <unistd.h>


// If [path] is a symlink, stat() returns info about the target of the link.
bool pyro_exists(const char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        return true;
    }
    return false;
}


// If [path] is a symlink, stat() returns info about the target of the link.
bool pyro_is_file(const char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        if (S_ISREG(s.st_mode)) {
            return true;
        }
    }
    return false;
}


// If [path] is a symlink, stat() returns info about the target of the link.
bool pyro_is_dir(const char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        if (S_ISDIR(s.st_mode)) {
            return true;
        }
    }
    return false;
}


// If [path] is a symlink, lstat() returns info about the symlink file itself.
bool pyro_is_symlink(const char* path) {
    struct stat s;
    if (lstat(path, &s) == 0) {
        if (S_ISLNK(s.st_mode)) {
            return true;
        }
    }
    return false;
}


char* pyro_dirname(char* path) {
    return dirname(path);
}


char* pyro_basename(char* path) {
    return basename(path);
}


FILE* pyro_popen(const char* command, const char* mode) {
    return popen(command, mode);
}


int pyro_pclose(FILE* stream) {
    return pclose(stream);
}


// Returns 0 if successful, -1 if an error occurs.
static int rmrf_callback(const char* path, const struct stat* s, int type, struct FTW* ftw_buf) {
    return remove(path);
}


// Returns 0 if successful, -1 if an error occurs.
int pyro_rmrf(const char* path) {
    if (pyro_is_symlink(path) || pyro_is_file(path)) {
        return remove(path);
    } else if (pyro_is_dir(path)) {
        return nftw(path, rmrf_callback, 64, FTW_DEPTH | FTW_PHYS);
    } else {
        return -1;
    }
}


// Returns 0 if successful, -1 if an error occurs.
int pyro_sleep(double time_in_seconds) {
    double whole_seconds = floor(time_in_seconds);
    double fract_seconds = time_in_seconds - whole_seconds;

    struct timespec req;
    req.tv_sec = (time_t)whole_seconds;
    req.tv_nsec = (long)(fract_seconds * 1e9);

    while (true) {
        struct timespec rem;
        if (nanosleep(&req, &rem) == 0) {
            return 0;
        }
        if (errno == EINTR) {
            req = rem;
            continue;
        }
        return -1;
    }
}


char* pyro_getcwd(void) {
    return getcwd(NULL, 0);
}
