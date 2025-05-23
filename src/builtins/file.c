#include "../includes/pyro.h"


static PyroValue fn_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (arg_count == 1) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$file(): invalid argument [path], expected a string");
            return pyro_null();
        }

        FILE* stream = fopen(PYRO_AS_STR(args[0])->bytes, "r");
        if (!stream) {
            pyro_panic(vm, "$file(): unable to open file '%s'", PYRO_AS_STR(args[0])->bytes);
            return pyro_null();
        }

        PyroFile* file = PyroFile_new(vm, stream);
        if (!file) {
            pyro_panic(vm, "$file(): out of memory");
            return pyro_null();
        }

        file->path = PYRO_AS_STR(args[0]);
        return pyro_obj(file);
    }

    if (arg_count == 2) {
        if (!PYRO_IS_STR(args[0])) {
            pyro_panic(vm, "$file(): invalid argument [path], expected a string");
            return pyro_null();
        }

        if (!PYRO_IS_STR(args[1])) {
            pyro_panic(vm, "$file(): invalid argument [mode], expected a string");
            return pyro_null();
        }

        FILE* stream = fopen(PYRO_AS_STR(args[0])->bytes, PYRO_AS_STR(args[1])->bytes);
        if (!stream) {
            pyro_panic(vm, "$file(): unable to open file '%s'", PYRO_AS_STR(args[0])->bytes);
            return pyro_null();
        }

        PyroFile* file = PyroFile_new(vm, stream);
        if (!file) {
            pyro_panic(vm, "$file(): out of memory");
            return pyro_null();
        }

        file->path = PYRO_AS_STR(args[0]);
        return pyro_obj(file);
    }

    pyro_panic(vm, "$file(): expected 1 or 2 arguments, found %zu", arg_count);
    return pyro_null();
}


static PyroValue fn_is_file(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_FILE(args[0]));
}


static PyroValue file_flush(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "flush(): file is closed");
        return pyro_null();
    }

    fflush(file->stream);
    return pyro_null();
}


static PyroValue file_close(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);

    if (!file->stream) {
        return pyro_null();
    }

    int result = fclose(file->stream);
    file->stream = NULL;

    if (result != 0) {
        pyro_panic(vm, "close(): error closing file: %s", strerror(errno));
    }

    return pyro_null();
}


static PyroValue file_end_with(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);

    if (file->stream) {
        fclose(file->stream);
        file->stream = NULL;
    }

    return pyro_null();
}


static PyroValue file_read(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "read(): file is closed");
        return pyro_null();
    }

    PyroBuf* buf = PyroFile_read(file, "read()", vm);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return pyro_obj(buf);
}


static PyroValue file_read_string(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "read_string(): file is closed");
        return pyro_null();
    }

    PyroBuf* buf = PyroFile_read(file, "read_string()", vm);
    if (vm->halt_flag) {
        return pyro_null();
    }

    PyroStr* string = PyroBuf_to_str(buf, vm);
    if (!string) {
        pyro_panic(vm, "read_string(): out of memory");
        return pyro_null();
    }

    return pyro_obj(string);
}


static PyroValue file_read_bytes(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "read_bytes(): file is closed");
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "read_bytes(): invalid argument [n], expected a non-negative integer");
        return pyro_null();
    }
    size_t num_bytes_to_read = args[0].as.i64;

    PyroBuf* buf = PyroBuf_new_with_capacity(num_bytes_to_read + 1, vm);
    if (!buf) {
        pyro_panic(vm, "read_bytes(): out of memory");
        return pyro_null();
    }

    if (num_bytes_to_read == 0 || feof(file->stream)) {
        return pyro_obj(buf);
    }

    buf->count = fread(buf->bytes, sizeof(uint8_t), num_bytes_to_read, file->stream);

    if (buf->count < num_bytes_to_read && ferror(file->stream)) {
        pyro_panic(vm, "read_bytes(): I/O read error");
        return pyro_null();
    }

    return pyro_obj(buf);
}


static PyroValue file_read_byte(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "read_byte(): file is closed");
        return pyro_null();
    }

    if (feof(file->stream)) {
        return pyro_null();
    }

    uint8_t byte;
    size_t n = fread(&byte, sizeof(uint8_t), 1, file->stream);

    if (n < 1) {
        if (ferror(file->stream)) {
            pyro_panic(vm, "read_byte(): I/O read error");
            return pyro_null();
        }
        if (n == 0) {
            return pyro_null();
        }
    }

    return pyro_i64(byte);
}


static PyroValue file_read_line(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "read_line(): file is closed");
        return pyro_null();
    }

    PyroStr* string = PyroFile_read_line(file, vm);
    if (vm->halt_flag) {
        return pyro_null();
    }

    return string ? pyro_obj(string) : pyro_null();
}


static PyroValue file_lines(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "lines(): file is closed");
        return pyro_null();
    }

    PyroIter* iter = PyroIter_new((PyroObject*)file, PYRO_ITER_FILE_LINES, vm);
    if (!iter) {
        pyro_panic(vm, "lines(): out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue file_write(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "write(): file is closed");
        return pyro_null();
    }

    if (arg_count == 0) {
        pyro_panic(vm, "write(): expected 1 or more arguments, found 0");
        return pyro_null();
    }

    if (arg_count == 1) {
        if (PYRO_IS_BUF(args[0])) {
            PyroBuf* buf = PYRO_AS_BUF(args[0]);
            size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, file->stream);
            if (n < buf->count) {
                pyro_panic(vm, "write(): I/O write error");
                return pyro_null();
            }
            return pyro_i64((int64_t)n);
        } else {
            PyroStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return pyro_null();
            }
            size_t n = fwrite(string->bytes, sizeof(char), string->count, file->stream);
            if (n < string->count) {
                pyro_panic(vm, "write(): I/O write error");
                return pyro_null();
            }
            return pyro_i64((int64_t)n);
        }
    }

    if (!PYRO_IS_STR(args[0])) {
        pyro_panic(vm, "write(): invalid agument [format_string], expected a string");
        return pyro_null();
    }

    PyroStr* string = pyro_format(vm, PYRO_AS_STR(args[0]), arg_count - 1, &args[1], "$write()");
    if (vm->halt_flag) {
        return pyro_null();
    }

    size_t n = fwrite(string->bytes, sizeof(char), string->count, file->stream);
    if (n < string->count) {
        pyro_panic(vm, "write(): I/O write error");
        return pyro_null();
    }

    return pyro_i64((int64_t)n);
}


static PyroValue file_write_byte(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "write_byte(): file is closed");
        return pyro_null();
    }

    uint8_t byte;

    if (PYRO_IS_I64(args[0])) {
        if (args[0].as.i64 < 0 || args[0].as.i64 > 255) {
            pyro_panic(vm, "write_byte(): invalid argument [byte], integer (%d) is out of range", args[0].as.i64);
            return pyro_null();
        }
        byte = (uint8_t)args[0].as.i64;
    } else if (PYRO_IS_RUNE(args[0])) {
        if (args[0].as.u32 > 255) {
            pyro_panic(vm, "write_byte(): invalid argument [byte], char (%d) is out of range", args[0].as.u32);
            return pyro_null();
        }
        byte = (uint8_t)args[0].as.u32;
    } else {
        pyro_panic(vm, "write_byte(): invalid argument [byte], expected an integer");
        return pyro_null();
    }

    size_t n = fwrite(&byte, sizeof(uint8_t), 1, file->stream);
    if (n < 1) {
        pyro_panic(vm, "write_byte(): I/O write error");
        return pyro_null();
    }

    return pyro_null();
}


static PyroValue file_tell(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "tell(): file is closed");
        return pyro_null();
    }

    return pyro_i64(ftell(file->stream));
}


static PyroValue file_seek_from_start(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "seek_from_start(): file is closed");
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "seek_from_start(): invalid argument [offset], expected an integer");
        return pyro_null();
    }

    int result = fseek(file->stream, args[0].as.i64, SEEK_SET);
    if (result == -1) {
        pyro_panic(vm, "seek_from_start(): file is not seekable to offset %" PRId64, args[0].as.i64);
        return pyro_null();
    }

    return pyro_i64(ftell(file->stream));
}


static PyroValue file_seek_from_end(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "seek_from_end(): file is closed");
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "seek_from_end(): invalid argument [offset], expected an integer");
        return pyro_null();
    }

    int result = fseek(file->stream, args[0].as.i64, SEEK_END);
    if (result == -1) {
        pyro_panic(vm, "seek_from_end(): file is not seekable to offset %" PRId64, args[0].as.i64);
        return pyro_null();
    }

    return pyro_i64(ftell(file->stream));
}


static PyroValue file_seek_from_current(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (!file->stream) {
        pyro_panic(vm, "seek_from_current(): file is closed");
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(vm, "seek_from_current(): invalid argument [offset], expected an integer");
        return pyro_null();
    }

    int result = fseek(file->stream, args[0].as.i64, SEEK_CUR);
    if (result == -1) {
        pyro_panic(vm, "seek_from_current(): file is not seekable to offset %" PRId64, args[0].as.i64);
        return pyro_null();
    }

    return pyro_i64(ftell(file->stream));
}


static PyroValue file_is_terminal(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);
    if (file->stream) {
        return pyro_bool(pyro_file_is_terminal(file->stream));
    }

    return pyro_bool(false);
}


static PyroValue file_get_terminal_size(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroFile* file = PYRO_AS_FILE(args[-1]);

    int64_t width;
    int64_t height;

    if (pyro_get_terminal_size(file->stream, &width, &height)) {
        PyroTup* tup = PyroTup_new(2, vm);
        if (!tup) {
            pyro_panic(vm, "out of memory");
            return pyro_null();
        }

        tup->values[0] = pyro_i64(width);
        tup->values[1] = pyro_i64(height);

        return pyro_obj(tup);
    }

    return pyro_obj(vm->empty_error);
}


void pyro_load_builtin_type_file(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$file", fn_file, -1);
    pyro_define_superglobal_fn(vm, "$is_file", fn_is_file, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_file, "$end_with", file_end_with, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_file, "close", file_close, 0);
    pyro_define_pub_method(vm, vm->class_file, "flush", file_flush, 0);
    pyro_define_pub_method(vm, vm->class_file, "read", file_read, 0);
    pyro_define_pub_method(vm, vm->class_file, "read_string", file_read_string, 0);
    pyro_define_pub_method(vm, vm->class_file, "read_line", file_read_line, 0);
    pyro_define_pub_method(vm, vm->class_file, "read_bytes", file_read_bytes, 1);
    pyro_define_pub_method(vm, vm->class_file, "read_byte", file_read_byte, 0);
    pyro_define_pub_method(vm, vm->class_file, "write", file_write, -1);
    pyro_define_pub_method(vm, vm->class_file, "write_byte", file_write_byte, 1);
    pyro_define_pub_method(vm, vm->class_file, "lines", file_lines, 0);
    pyro_define_pub_method(vm, vm->class_file, "tell", file_tell, 0);
    pyro_define_pub_method(vm, vm->class_file, "seek_from_start", file_seek_from_start, 1);
    pyro_define_pub_method(vm, vm->class_file, "seek_from_end", file_seek_from_end, 1);
    pyro_define_pub_method(vm, vm->class_file, "seek_from_current", file_seek_from_current, 1);
    pyro_define_pub_method(vm, vm->class_file, "is_terminal", file_is_terminal, 0);
    pyro_define_pub_method(vm, vm->class_file, "get_terminal_size", file_get_terminal_size, 0);
}
