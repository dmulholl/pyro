// Local imports.
#include "../includes/pyro.h"

// POSIX: popen(), pclose()
#include <stdio.h>

// POSIX: stat(), lstat(), S_ISDIR(), S_ISREG, S_ISLNK()
#include <sys/stat.h>

// POSIX: nftw()
#include <ftw.h>

// POSIX: getcwd(), chdir(), chroot(), fork(), exec(), dup2(), read(), write()
#include <unistd.h>

// POSIX: waitpid()
#include <sys/wait.h>

// POSIX: opendir(), closedir()
#include <dirent.h>

// POSIX: setenv()
#include <stdlib.h>

// POSIX: dlopen(), dlsym()
#include <dlfcn.h>


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


// Returns 0 if successful, -1 if an error occurs.
static int remove_callback(const char* path, const struct stat* s, int type, struct FTW* ftw_buf) {
    return remove(path);
}


// Returns 0 if successful, -1 if an error occurs.
int pyro_remove(const char* path) {
    if (pyro_is_symlink(path) || pyro_is_file(path)) {
        return remove(path);
    } else if (pyro_is_dir(path)) {
        return nftw(path, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
    }
    return -1;
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


PyroVec* pyro_listdir(PyroVM* vm, const char* path) {
    PyroVec* vec = PyroVec_new(vm);
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

        PyroStr* string = PyroStr_copy(name, length, false, vm);
        if (!string) {
            pyro_panic(vm, "out of memory");
            break;
        }

        if (!PyroVec_append(vec, pyro_obj(string), vm)) {
            pyro_panic(vm, "out of memory");
            break;
        }
    }

    closedir(dirp);
    return vec;
}


char* pyro_realpath(const char* path) {
    return realpath(path, NULL);
}


bool pyro_setenv(const char* name, const char* value) {
    return setenv(name, value, 1) == 0;
}


bool pyro_chdir(const char* path) {
    return chdir(path) == 0;
}


bool pyro_chroot(const char* path) {
    /* return chroot(path) == 0; */
    return false;
}


// - Returns true on success.
// - Returns false if an I/O error occurs.
static bool write_to_fd(int fd, const uint8_t* buf, size_t length) {
    size_t index = 0;

    while (index < length) {
        size_t num_bytes_to_write = length - index;
        ssize_t count = write(fd, &buf[index], num_bytes_to_write);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return false;
            }
        }
        index += count;
    }

    return true;
}


// - Returns 0 on success.
// - Returns 1 if memory allocation fails.
// - Returns 2 if an I/O error occurs.
static int read_from_fd(PyroVM* vm, int fd, PyroBuf** output) {
    PyroBuf* out_buf = PyroBuf_new(vm);
    if (!out_buf) {
        return 1;
    }

    uint8_t in_buf[1024];

    while (true) {
        ssize_t count = read(fd, in_buf, 1024);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return 2;
            }
        } else if (count == 0) {
            break;
        } else {
            if (!PyroBuf_append_bytes(out_buf, count, in_buf, vm)) {
                return 1;
            }
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
    PyroStr** output,
    PyroStr** error,
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
        PyroBuf* out_buf;
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
        PyroBuf* err_buf;
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

        PyroStr* out_str = PyroBuf_to_str(out_buf, vm);
        if (!out_str) {
            pyro_panic(vm, "pyro_exec_shell_cmd(): out of memory");
            return false;
        }

        PyroStr* err_str = PyroBuf_to_str(err_buf, vm);
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


void pyro_dlopen_as_module(PyroVM* vm, const char* path, const char* mod_name, PyroMod* module) {
    void* handle = dlopen(path, RTLD_NOW);
    if (!handle) {
        pyro_panic(vm, "failed to load module: %s: %s", path, dlerror());
        return;
    }

    typedef bool (*init_func_t)(PyroVM* vm, PyroMod* module);
    init_func_t init_func;

    char* init_func_name = pyro_sprintf(vm, "pyro_init_mod_%s", mod_name);
    if (vm->halt_flag) {
        return;
    }

    // The contortion on the left is to silence a compiler warning about converting an object
    // pointer to a function pointer. The conversion is safe and is required by dlsym().
    *(void**)(&init_func) = dlsym(handle, init_func_name);
    PYRO_FREE_ARRAY(vm, char, init_func_name, strlen(init_func_name) + 1);
    if (!init_func) {
        pyro_panic(vm, "failed to locate module initialization function: %s: %s", path, dlerror());
        return;
    }

    bool okay = init_func(vm, module);
    if (vm->halt_flag) {
        return;
    } else if (vm->memory_allocation_failed) {
        pyro_panic(vm, "out of memory");
        return;
    } else if (!okay) {
        pyro_panic(vm, "failed to initialize module: %s", path);
        return;
    }
}
