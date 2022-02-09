#include "io.h"
#include "values.h"
#include "objects.h"
#include "vm.h"


// Writes a printf-style formatted string to [destination] where [destination] is either an ObjFile
// or an ObjBuf. If [destination] is NULL, this is a no-op.
// - On success, returns the number of bytes written.
// - Returns -1 if an attempt to write to a file failed.
// - Returns -2 if an attempt to write to a buffer failed.
static int64_t write(PyroVM* vm, Obj* destination, const char* format_string, va_list args) {
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
            int64_t n = ObjBuf_write_v(buf, vm, format_string, args);
            return n >= 0 ? n : -2;
        }

        default: {
            assert(false);
            return 0;
        }
    }
}


int64_t pyro_write_stdout_v(PyroVM* vm, const char* format_string, va_list args) {
    return write(vm, vm->stdout_stream, format_string, args);
}


int64_t pyro_write_stdout(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_stdout_v(vm, format_string, args);
    va_end(args);
    return result;
}


int64_t pyro_write_stderr_v(PyroVM* vm, const char* format_string, va_list args) {
    return write(vm, vm->stderr_stream, format_string, args);
}


int64_t pyro_write_stderr(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    int64_t result = pyro_write_stderr_v(vm, format_string, args);
    va_end(args);
    return result;
}
