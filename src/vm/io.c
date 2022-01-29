#include "io.h"
#include "values.h"
#include "objects.h"
#include "vm.h"


// Writes a printf-style formatted string. If [destination] is NULL, this is a no-op. Otherwise
// [destination] must be either an ObjFile or an ObjBuf.
static bool write(PyroVM* vm, Obj* destination, const char* format_string, va_list args) {
    if (!destination) {
        return true;
    }

    switch (destination->type) {
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)destination;
            if (file->stream) {
                return vfprintf(file->stream, format_string, args) >= 0;
            }
            return false;
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)destination;
            return ObjBuf_write_v(buf, vm, format_string, args);
        }

        default: {
            assert(false);
            return false;
        }
    }
}


bool pyro_write_stdout_v(PyroVM* vm, const char* format_string, va_list args) {
    return write(vm, vm->stdout_stream, format_string, args);
}


bool pyro_write_stdout(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    bool result = pyro_write_stdout_v(vm, format_string, args);
    va_end(args);
    return result;
}


bool pyro_write_stderr_v(PyroVM* vm, const char* format_string, va_list args) {
    return write(vm, vm->stderr_stream, format_string, args);
}


bool pyro_write_stderr(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    bool result = pyro_write_stderr_v(vm, format_string, args);
    va_end(args);
    return result;
}
