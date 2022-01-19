#include "os.h"

// Local imports.
#include "vm.h"
#include "values.h"
#include "objects.h"
#include "heap.h"

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

// POSIX: opendir() closedir()
#include <dirent.h>

// POSIX: strdup()
#include <string.h>

// POSIX: setenv()
#include <stdlib.h>


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


ObjVec* pyro_listdir(PyroVM* vm, const char* path) {
    ObjVec* vec = ObjVec_new(vm);
    if (!vec) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL;
    }

    DIR* dirp = opendir(path);
    if (!dirp) {
        pyro_panic(vm, ERR_OS_ERROR, "Unable to open directory '%s'.", path);
        return NULL;
    }

    struct dirent* dp;
    vm->gc_disallows++;

    while ((dp = readdir(dirp)) != NULL) {
        char* name = dp->d_name;
        size_t length = strlen(name);

        if (length == 1 && name[0] == '.') {
            continue;
        }

        if (length == 2 && name[0] == '.' && name[1] == '.') {
            continue;
        }

        ObjStr* string = ObjStr_copy_raw(name, length, vm);
        if (!string) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            break;
        }

        if (!ObjVec_append(vec, MAKE_OBJ(string), vm)) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            break;
        }
    }

    vm->gc_disallows--;
    closedir(dirp);
    return vec;
}


bool pyro_run_shell_cmd(PyroVM* vm, const char* cmd, ShellCmdResult* out) {
    FILE* file = popen(cmd, "r");
    if (!file) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to run comand.");
        return false;
    }

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 >= capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pclose(file);
                return false;
            }
            capacity = new_capacity;
            array = new_array;
        }

        size_t max_bytes = capacity - count - 1;
        size_t bytes_read = fread(&array[count], sizeof(uint8_t), max_bytes, file);
        count += bytes_read;

        if (bytes_read < max_bytes) {
            if (ferror(file)) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pclose(file);
                return false;
            }
            break; // EOF
        }
    }

    int exit_code = pclose(file);

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }
    array[count] = '\0';

    ObjStr* output = ObjStr_take((char*)array, count, vm);
    if (!output) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return false;
    }

    *out = (ShellCmdResult) {
        .output = output,
        .exit_code = exit_code
    };

    return true;
}


char* pyro_realpath(const char* path) {
    char* array = malloc(PATH_MAX);
    if (!array) {
        return NULL;
    }

    char* result = realpath(path, array);
    if (!result) {
        free(array);
        return NULL;
    }

    return array;
}


char* pyro_strdup(const char* source) {
    return strdup(source);
}


bool pyro_setenv(const char* name, const char* value) {
    return setenv(name, value, 1) == 0;
}
