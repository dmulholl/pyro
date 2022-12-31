#include "../inc/io.h"
#include "../inc/values.h"
#include "../inc/objects.h"
#include "../inc/vm.h"


static int64_t pyro_write_n(PyroVM* vm, PyroObjFile* file, const char* string, size_t count) {
    if (!file || count == 0) {
        return 0;
    }

    if (file->stream) {
        size_t result = fwrite(string, 1, count, file->stream);
        if (result < count) {
            return -1;
        }
        return result;
    }

    return -1;
}


static int64_t pyro_write_fv(PyroVM* vm, PyroObjFile* file, const char* format_string, va_list args) {
    if (!file) {
        return 0;
    }

    if (file->stream) {
        int n = vfprintf(file->stream, format_string, args);
        return n >= 0 ? n : -1;
    }

    return -1;
}


int64_t pyro_stdout_write(PyroVM* vm, const char* string) {
    return pyro_write_n(vm, vm->stdout_file, string, strlen(string));
}


int64_t pyro_stdout_write_n(PyroVM* vm, const char* string, size_t count) {
    return pyro_write_n(vm, vm->stdout_file, string, count);
}


int64_t pyro_stdout_write_s(PyroVM* vm, PyroObjStr* string) {
    return pyro_write_n(vm, vm->stdout_file, string->bytes, string->length);
}


int64_t pyro_stdout_write_f(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_fv(vm, vm->stdout_file, format_string, args);
    va_end(args);
    return result;
}


int64_t pyro_stdout_write_fv(PyroVM* vm, const char* format_string, va_list args) {
    return pyro_write_fv(vm, vm->stdout_file, format_string, args);
}


int64_t pyro_stderr_write(PyroVM* vm, const char* string) {
    return pyro_write_n(vm, vm->stderr_file, string, strlen(string));
}


int64_t pyro_stderr_write_n(PyroVM* vm, const char* string, size_t count) {
    return pyro_write_n(vm, vm->stderr_file, string, count);
}


int64_t pyro_stderr_write_s(PyroVM* vm, PyroObjStr* string) {
    return pyro_write_n(vm, vm->stderr_file, string->bytes, string->length);
}


int64_t pyro_stderr_write_f(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_fv(vm, vm->stderr_file, format_string, args);
    va_end(args);
    return result;
}


int64_t pyro_stderr_write_fv(PyroVM* vm, const char* format_string, va_list args) {
    return pyro_write_fv(vm, vm->stderr_file, format_string, args);
}
