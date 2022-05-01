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

// POSIX: getcwd(), chdir(), fork(), exec(), dup2(), read(), write()
#include <unistd.h>

// POSIX: waitpid()
#include <sys/wait.h>

// POSIX: opendir(), closedir()
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
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    DIR* dirp = opendir(path);
    if (!dirp) {
        pyro_panic(vm, "unable to open directory '%s'", path);
        return NULL;
    }

    struct dirent* dp;

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
            pyro_panic(vm, "out of memory");
            break;
        }

        if (!ObjVec_append(vec, MAKE_OBJ(string), vm)) {
            pyro_panic(vm, "out of memory");
            break;
        }
    }

    closedir(dirp);
    return vec;
}


char* pyro_realpath(const char* path) {
    errno = 0;
    char* array = malloc(PATH_MAX);
    if (!array) {
        #ifdef DEBUG
            perror("DEBUG: malloc() returned NULL in pyro_realpath()");
        #endif
        return NULL;
    }

    errno = 0;
    char* result = realpath(path, array);
    if (!result) {
        #ifdef DEBUG
            fprintf(stderr, "DEBUG: realpath() returned NULL in pyro_realpath() for path '%s':\n  ", path);
            perror(NULL);
        #endif
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


bool pyro_cd(const char* path) {
    return chdir(path) == 0;
}


// - Returns true on success.
// - Returns false if an I/O error occurs.
static bool write_to_fd(int fd, const uint8_t* buf, size_t length) {
    size_t index = 0;

    while (index < length) {
        size_t num_bytes_to_write = length - index;
        ssize_t n = write(fd, &buf[index], num_bytes_to_write);
        if (n < 0) {
            return false;
        }
        index += n;
    }

    return true;
}


// - Returns 0 on success.
// - Returns 1 if memory allocation fails.
// - Returns 2 if an I/O error occurs.
static int read_from_fd(PyroVM* vm, int fd, ObjBuf** output) {
    ObjBuf* out_buf = ObjBuf_new(vm);
    if (!out_buf) {
        return 1;
    }

    uint8_t in_buf[256];

    while (true) {
        ssize_t n = read(fd, in_buf, 256);
        if (n == 0) {
            break;
        } else if (n > 0) {
            if (!ObjBuf_append_bytes(out_buf, n, in_buf, vm)) {
                return 1;
            }
        } else {
            return 2;
        }
    }

    *output = out_buf;
    return 0;
}


bool pyro_exec_shell_cmd(
    PyroVM* vm,
    const char* cmd,
    const uint8_t* input,
    size_t input_length,
    ObjStr** output,
    ObjStr** error,
    int* exit_code
) {
    int child_stdin_pipe[2];
    if (pipe(child_stdin_pipe) == -1) {
        pyro_panic(vm, "pyro_exec_shell_cmd(): unable to create child's stdin pipe");
        return false;
    }

    int child_stdout_pipe[2];
    if (pipe(child_stdout_pipe) == -1) {
        pyro_panic(vm, "pyro_exec_shell_cmd(): unable to create child's stdout pipe");
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        return false;
    }

    int child_stderr_pipe[2];
    if (pipe(child_stderr_pipe) == -1) {
        pyro_panic(vm, "pyro_exec_shell_cmd(): unable to create child's stderr pipe");
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        close(child_stdout_pipe[0]);
        close(child_stdout_pipe[1]);
        return false;
    }

    pid_t child_pid = fork();

    // If child_pid == 0, we're in the child.
    if (child_pid == 0) {
        close(child_stdin_pipe[1]); // close the write end of the input pipe
        while ((dup2(child_stdin_pipe[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stdin_pipe[0]);

        close(child_stdout_pipe[0]); // close the read end of the output pipe
        while ((dup2(child_stdout_pipe[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stdout_pipe[1]);

        close(child_stderr_pipe[0]); // close the read end of the error pipe
        while ((dup2(child_stderr_pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stderr_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        perror("pyro_exec_shell_cmd(): returned from execl() in child");
        exit(1);
    }

    // If child_pid > 0, we're in the parent.
    else if (child_pid > 0) {
        close(child_stdin_pipe[0]);  // close the read end of the input pipe
        close(child_stdout_pipe[1]); // close the write end of the output pipe
        close(child_stderr_pipe[1]); // close the write end of the error pipe

        // Write the input string to the child's stdin pipe.
        if (input_length > 0) {
            if (!write_to_fd(child_stdin_pipe[1], input, input_length)) {
                pyro_panic(vm, "pyro_exec_shell_cmd(): I/O error while writing to child's stdin pipe");
                close(child_stdin_pipe[1]);
                close(child_stdout_pipe[0]);
                close(child_stderr_pipe[0]);
                return false;
            }
        }
        close(child_stdin_pipe[1]);

        // Read the child's stdout pipe.
        ObjBuf* out_buf;
        int out_result = read_from_fd(vm, child_stdout_pipe[0], &out_buf);
        if (out_result == 1) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): ran out of memory while reading from child's stdout pipe");
            close(child_stdout_pipe[0]);
            close(child_stderr_pipe[0]);
            return false;
        } else if (out_result == 2) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): I/O error while reading from child's stdout pipe");
            close(child_stdout_pipe[0]);
            close(child_stderr_pipe[0]);
            return false;
        }
        close(child_stdout_pipe[0]);

        // Read the child's stderr pipe.
        ObjBuf* err_buf;
        int err_result = read_from_fd(vm, child_stderr_pipe[0], &err_buf);
        if (err_result == 1) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): ran out of memory while reading from child's stderr pipe");
            close(child_stderr_pipe[0]);
            return false;
        } else if (err_result == 2) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): I/O error while reading from child's stderr pipe");
            close(child_stderr_pipe[0]);
            return false;
        }
        close(child_stderr_pipe[0]);

        int status;
        do {
            waitpid(child_pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        ObjStr* out_str = ObjBuf_to_str(out_buf, vm);
        if (!out_str) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): out of memory");
            return false;
        }

        ObjStr* err_str = ObjBuf_to_str(err_buf, vm);
        if (!err_str) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): out of memory");
            return false;
        }

        *output = out_str;
        *error = err_str;
        *exit_code = status;
        return true;
    }

    // If child_pid < 0, the attempt to fork() failed.
    else {
        pyro_panic(vm, "pyro_exec_shell_cmd(): fork() failed");
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        close(child_stdout_pipe[0]);
        close(child_stdout_pipe[1]);
        close(child_stderr_pipe[0]);
        close(child_stderr_pipe[1]);
        return false;
    }
}
