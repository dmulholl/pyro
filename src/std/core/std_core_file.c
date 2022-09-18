#include "../../inc/std_lib.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/stringify.h"
#include "../../inc/panics.h"


static Value fn_file(PyroVM* vm, size_t arg_count, Value* args) {
    const char* path;
    const char* mode = "r";

    if (arg_count == 1) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$file(): invalid argument [path], expected a string");
            return pyro_make_null();
        }
        path = AS_STR(args[0])->bytes;
    } else if (arg_count == 2) {
        if (!IS_STR(args[0])) {
            pyro_panic(vm, "$file(): invalid argument [path], expected a string");
            return pyro_make_null();
        }
        path = AS_STR(args[0])->bytes;
        if (!IS_STR(args[1])) {
            pyro_panic(vm, "$file(): invalid argument [mode], expected a string");
            return pyro_make_null();
        }
        mode = AS_STR(args[1])->bytes;
    } else {
        pyro_panic(vm, "$file(): expected 1 or 2 arguments, found %zu", arg_count);
        return pyro_make_null();
    }

    FILE* stream = fopen(path, mode);
    if (!stream) {
        pyro_panic(vm, "$file(): unable to open file '%s'", path);
        return pyro_make_null();
    }

    ObjFile* file = ObjFile_new(vm, stream);
    if (!file) {
        pyro_panic(vm, "$file(): out of memory");
        return pyro_make_null();
    }

    return MAKE_OBJ(file);
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_FILE(args[0]));
}


static Value file_flush(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fflush(file->stream);
    }

    return pyro_make_null();
}


static Value file_close(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fclose(file->stream);
        file->stream = NULL;
    }

    return pyro_make_null();
}


static Value file_read(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "read(): out of memory");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return pyro_make_null();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, "read(): I/O read error");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return pyro_make_null();
            }
            break;
        }

        array[count++] = c;
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, "read(): out of memory");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return pyro_make_null();
    }

    buf->count = count;
    buf->capacity = capacity;
    buf->bytes = array;

    return MAKE_OBJ(buf);
}


static Value file_read_string(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, "read_string(): out of memory");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return pyro_make_null();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, "read_string(): I/O read error");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return pyro_make_null();
            }
            break;
        }

        array[count++] = c;
    }

    array[count] = '\0';
    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }

    ObjStr* string = ObjStr_take((char*)array, count, vm);
    if (!string) {
        pyro_panic(vm, "read_string(): out of memory");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return pyro_make_null();
    }

    return MAKE_OBJ(string);
}


static Value file_read_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, "read_bytes(): invalid argument [n], expected a non-negative integer");
        return pyro_make_null();
    }
    size_t num_bytes_to_read = args[0].as.i64;

    ObjBuf* buf = ObjBuf_new_with_cap(num_bytes_to_read, vm);
    if (!buf) {
        pyro_panic(vm, "read_bytes(): out of memory");
        return pyro_make_null();
    }

    if (num_bytes_to_read == 0 || feof(file->stream)) {
        return MAKE_OBJ(buf);
    }

    buf->count = fread(buf->bytes, sizeof(uint8_t), num_bytes_to_read, file->stream);

    if (buf->count < num_bytes_to_read && ferror(file->stream)) {
        pyro_panic(vm, "read_bytes(): I/O read error");
        return pyro_make_null();
    }

    return MAKE_OBJ(buf);
}


static Value file_read_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (feof(file->stream)) {
        return pyro_make_null();
    }

    uint8_t byte;
    size_t n = fread(&byte, sizeof(uint8_t), 1, file->stream);

    if (n < 1) {
        if (ferror(file->stream)) {
            pyro_panic(vm, "read_byte(): I/O read error");
            return pyro_make_null();
        }
        if (n == 0) {
            return pyro_make_null();
        }
    }

    return MAKE_I64(byte);
}


static Value file_read_line(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    ObjStr* string = ObjFile_read_line(file, vm);
    if (vm->halt_flag) {
        return pyro_make_null();
    }

    return string ? MAKE_OBJ(string) : pyro_make_null();
}


static Value file_lines(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);
    ObjIter* iter = ObjIter_new((Obj*)file, ITER_FILE_LINES, vm);
    if (!iter) {
        pyro_panic(vm, "lines(): out of memory");
        return pyro_make_null();
    }
    return MAKE_OBJ(iter);
}


static Value file_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, "write(): expected 1 or more arguments, found 0");
        return pyro_make_null();
    }

    if (arg_count == 1) {
        if (IS_BUF(args[0])) {
            ObjBuf* buf = AS_BUF(args[0]);
            size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, file->stream);
            if (n < buf->count) {
                pyro_panic(vm, "write(): I/O write error");
                return pyro_make_null();
            }
            return MAKE_I64((int64_t)n);
        } else {
            ObjStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return pyro_make_null();
            }
            size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);
            if (n < string->length) {
                pyro_panic(vm, "write(): I/O write error");
                return pyro_make_null();
            }
            return MAKE_I64((int64_t)n);
        }
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, "write(): invalid agument [format_string], expected a string");
        return pyro_make_null();
    }

    Value formatted = pyro_fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return pyro_make_null();
    }
    ObjStr* string = AS_STR(formatted);

    size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);

    if (n < string->length) {
        pyro_panic(vm, "write(): I/O write error");
        return pyro_make_null();
    }
    return MAKE_I64((int64_t)n);
}


static Value file_write_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);
    uint8_t byte;

    if (IS_I64(args[0])) {
        if (args[0].as.i64 < 0 || args[0].as.i64 > 255) {
            pyro_panic(vm, "write_byte(): invalid argument [byte], integer (%d) is out of range", args[0].as.i64);
            return pyro_make_null();
        }
        byte = (uint8_t)args[0].as.i64;
    } else if (IS_CHAR(args[0])) {
        if (args[0].as.u32 > 255) {
            pyro_panic(vm, "write_byte(): invalid argument [byte], char (%d) is out of range", args[0].as.u32);
            return pyro_make_null();
        }
        byte = (uint8_t)args[0].as.u32;
    } else {
        pyro_panic(vm, "write_byte(): invalid argument [byte], expected an integer");
        return pyro_make_null();
    }

    size_t n = fwrite(&byte, sizeof(uint8_t), 1, file->stream);
    if (n < 1) {
        pyro_panic(vm, "write_byte(): I/O write error");
        return pyro_make_null();
    }

    return pyro_make_null();
}


void pyro_load_std_core_file(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$file", fn_file, -1);
    pyro_define_global_fn(vm, "$is_file", fn_is_file, 1);

    // Methods.
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
}
