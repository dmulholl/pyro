#include "io.h"
#include "values.h"
#include "objects.h"
#include "vm.h"


static int64_t pyro_write_n(PyroVM* vm, Obj* destination, const char* string, size_t count) {
    if (!destination || count == 0) {
        return 0;
    }

    switch (destination->type) {
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)destination;
            if (file->stream) {
                size_t result = fwrite(string, 1, count, file->stream);
                if (result < count) {
                    return -1;
                }
                return result;
            }
            return -1;
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)destination;
            bool result = ObjBuf_append_bytes(buf, count, (uint8_t*)string, vm);
            return result ? (int64_t)count : -2;
        }

        default: {
            assert(false);
            return 0;
        }
    }
}


static int64_t pyro_write_fv(PyroVM* vm, Obj* destination, const char* format_string, va_list args) {
    if (!destination) {
        return 0;
    }

    switch (destination->type) {
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)destination;
            if (file->stream) {
                int n = vfprintf(file->stream, format_string, args);
                return n >= 0 ? n : -1;
            }
            return -1;
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)destination;
            int64_t n = ObjBuf_write_fv(buf, vm, format_string, args);
            return n >= 0 ? n : -2;
        }

        default: {
            assert(false);
            return 0;
        }
    }
}


int64_t pyro_stdout_write(PyroVM* vm, const char* string) {
    return pyro_write_n(vm, vm->stdout_stream, string, strlen(string));
}


int64_t pyro_stdout_write_n(PyroVM* vm, const char* string, size_t count) {
    return pyro_write_n(vm, vm->stdout_stream, string, count);
}


int64_t pyro_stdout_write_s(PyroVM* vm, ObjStr* string) {
    return pyro_write_n(vm, vm->stdout_stream, string->bytes, string->length);
}


int64_t pyro_stdout_write_f(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_fv(vm, vm->stdout_stream, format_string, args);
    va_end(args);
    return result;
}


int64_t pyro_stdout_write_fv(PyroVM* vm, const char* format_string, va_list args) {
    return pyro_write_fv(vm, vm->stdout_stream, format_string, args);
}


int64_t pyro_stderr_write(PyroVM* vm, const char* string) {
    return pyro_write_n(vm, vm->stderr_stream, string, strlen(string));
}


int64_t pyro_stderr_write_n(PyroVM* vm, const char* string, size_t count) {
    return pyro_write_n(vm, vm->stderr_stream, string, count);
}


int64_t pyro_stderr_write_s(PyroVM* vm, ObjStr* string) {
    return pyro_write_n(vm, vm->stderr_stream, string->bytes, string->length);
}


int64_t pyro_stderr_write_f(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_fv(vm, vm->stderr_stream, format_string, args);
    va_end(args);
    return result;
}


int64_t pyro_stderr_write_fv(PyroVM* vm, const char* format_string, va_list args) {
    return pyro_write_fv(vm, vm->stderr_stream, format_string, args);
}
