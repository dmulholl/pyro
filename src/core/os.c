// Local imports.
#include "../includes/pyro.h"

// POSIX: popen(), pclose(), fileno()
#include <stdio.h>

// POSIX: stat(), lstat(), S_ISDIR(), S_ISREG, S_ISLNK()
#include <sys/stat.h>

// POSIX: getcwd(), chdir(), fork(), exec(), dup2(), read(), write(), isatty(),
#include <unistd.h>

// POSIX: waitpid()
#include <sys/wait.h>

// POSIX: opendir(), closedir()
#include <dirent.h>

// POSIX: setenv()
#include <stdlib.h>

// POSIX: dlopen(), dlsym()
#include <dlfcn.h>

// POSIX: ioctl()
#include <sys/ioctl.h>

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


bool pyro_exec_cmd(
    PyroVM* vm,
    const char* cmd,
    char* const argv[],
    const uint8_t* stdin_input,
    size_t stdin_input_length,
    PyroStr** stdout_output,
    PyroStr** stderr_output,
    int* exit_code
) {
    int child_stdin_pipe[2];
    if (pipe(child_stdin_pipe) == -1) {
        pyro_panic(vm, "exec: %s: failed to create child's stdin pipe", cmd);
        return false;
    }

    int child_stdout_pipe[2];
    if (pipe(child_stdout_pipe) == -1) {
        pyro_panic(vm, "exec: %s: failed to create child's stdout pipe", cmd);
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        return false;
    }

    int child_stderr_pipe[2];
    if (pipe(child_stderr_pipe) == -1) {
        pyro_panic(vm, "exec: %s: failed to create child's stderr pipe", cmd);
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

        // A call to execvp() only returns if it fails.
        execvp(cmd, argv);
        char* err_msg = strerror(errno);
        pyro_stderr_write_f(vm, "exec: %s: %s\n", cmd, err_msg);
        exit(1);
    }

    // If child_pid > 0, we're in the parent.
    if (child_pid > 0) {
        close(child_stdin_pipe[0]);  // close the read end of the input pipe
        close(child_stdout_pipe[1]); // close the write end of the output pipe
        close(child_stderr_pipe[1]); // close the write end of the error pipe

        // Write the input string to the child's stdin pipe.
        if (stdin_input_length > 0 && stdin_input != NULL) {
            if (!write_to_fd(child_stdin_pipe[1], stdin_input, stdin_input_length)) {
                pyro_panic(vm, "exec: %s: error while writing to child's stdin pipe", cmd);
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
            pyro_panic(vm, "out of memory");
            close(child_stdout_pipe[0]);
            close(child_stderr_pipe[0]);
            return false;
        } else if (out_result == 2) {
            pyro_panic(vm, "exec: %s: error while reading from child's stdout pipe", cmd);
            close(child_stdout_pipe[0]);
            close(child_stderr_pipe[0]);
            return false;
        }
        close(child_stdout_pipe[0]);

        // Read the child's stderr pipe.
        PyroBuf* err_buf;
        int err_result = read_from_fd(vm, child_stderr_pipe[0], &err_buf);
        if (err_result == 1) {
            pyro_panic(vm, "out of memory");
            close(child_stderr_pipe[0]);
            return false;
        } else if (err_result == 2) {
            pyro_panic(vm, "exec: %s: error while reading from child's stderr pipe", cmd);
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
            pyro_panic(vm, "out of memory");
            return false;
        }

        PyroStr* err_str = PyroBuf_to_str(err_buf, vm);
        if (!err_str) {
            pyro_panic(vm, "out of memory");
            return false;
        }

        *stdout_output = out_str;
        *stderr_output = err_str;
        *exit_code = status;
        return true;
    }

    // If child_pid < 0, we're in the parent and the attempt to fork() failed.
    pyro_panic(vm, "exec: %s: fork() failed", cmd);
    close(child_stdin_pipe[0]);
    close(child_stdin_pipe[1]);
    close(child_stdout_pipe[0]);
    close(child_stdout_pipe[1]);
    close(child_stderr_pipe[0]);
    close(child_stderr_pipe[1]);
    return false;
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


bool pyro_file_is_terminal(FILE* file) {
    if (file) {
        return isatty(fileno(file));
    }

    return false;
}


bool pyro_get_terminal_size(FILE* file, int64_t* width, int64_t* height) {
    if (file && isatty(fileno(file))) {
        struct winsize w;

        if (ioctl(fileno(file), TIOCGWINSZ, &w) == 0) {
            *width = (int64_t)w.ws_col;
            *height = (int64_t)w.ws_row;
            return true;
        }

        return false;
    }

    return false;
}
