#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/os.h"
#include "../vm/errors.h"


static Value fn_file(PyroVM* vm, size_t arg_count, Value* args) {
    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid filename argument, must be a string.");
        return NULL_VAL();
    }

    if (!IS_STR(args[1])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid mode argument, must be a string.");
        return NULL_VAL();
    }

    ObjFile* file = ObjFile_new(vm);
    if (!file) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    FILE* stream = fopen(AS_STR(args[0])->bytes, AS_STR(args[1])->bytes);
    if (!stream) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to open file '%s'.", AS_STR(args[0])->bytes);
        return NULL_VAL();
    }

    file->stream = stream;
    return OBJ_VAL(file);
}


static Value fn_is_file(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_FILE(args[0]));
}


static Value file_flush(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fflush(file->stream);
    }

    return NULL_VAL();
}


static Value file_close(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (file->stream) {
        fclose(file->stream);
        file->stream = NULL;
    }

    return NULL_VAL();
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            break;
        }

        array[count++] = c;
    }

    ObjBuf* buf = ObjBuf_new(vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    buf->count = count;
    buf->capacity = capacity;
    buf->bytes = array;

    return OBJ_VAL(buf);
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
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                return NULL_VAL();
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    return OBJ_VAL(string);
}


// Attempts to read [n] bytes from the file into a byte buffer. May read less than [n] bytes if the
// end of the file is reached first. Returns the byte buffer or [NULL] if the end of the file had
// already been reached before the method was called. Panics if an I/O read error occurs, if the
// argument is invalid, or if memory cannot be allocated for the buffer.
static Value file_read_bytes(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (!IS_I64(args[0]) || args[0].as.i64 < 0) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument to :read_bytes(), must be a non-negative integer.");
        return NULL_VAL();
    }

    if (feof(file->stream)) {
        return NULL_VAL();
    }

    size_t num_bytes_to_read = args[0].as.i64;

    ObjBuf* buf = ObjBuf_new_with_cap(num_bytes_to_read, vm);
    if (!buf) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    if (num_bytes_to_read == 0) {
        return OBJ_VAL(buf);
    }

    size_t n = fread(buf->bytes, sizeof(uint8_t), num_bytes_to_read, file->stream);
    buf->count = n;

    if (n < num_bytes_to_read) {
        if (ferror(file->stream)) {
            pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
            return NULL_VAL();
        }
        if (n == 0) {
            return NULL_VAL();
        }
    }

    return OBJ_VAL(buf);
}


static Value file_read_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (feof(file->stream)) {
        return NULL_VAL();
    }

    uint8_t byte;
    size_t n = fread(&byte, sizeof(uint8_t), 1, file->stream);

    if (n < 1) {
        if (ferror(file->stream)) {
            pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
            return NULL_VAL();
        }
        if (n == 0) {
            return NULL_VAL();
        }
    }

    return I64_VAL(byte);
}


static Value file_read_line(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 > capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            capacity = new_capacity;
            array = new_array;
        }

        int c = fgetc(file->stream);

        if (c == EOF) {
            if (ferror(file->stream)) {
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
                return NULL_VAL();
            }
            break;
        }

        array[count++] = c;

        if (c == '\n') {
            break;
        }
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return NULL_VAL();
    }

    while (count > 0 && (array[count - 1] == '\n' || array[count - 1] == '\r')) {
        count--;
    }

    if (count == 0) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return OBJ_VAL(vm->empty_string);
    }

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }

    array[count] = '\0';

    ObjStr* string = ObjStr_take((char*)array, count, vm);
    if (!string) {
        FREE_ARRAY(vm, uint8_t, array, capacity);
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    return OBJ_VAL(string);
}


static Value file_write(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (arg_count == 0) {
        pyro_panic(vm, ERR_ARGS_ERROR, "Expected 1 or more arguments for :write(), found 0.");
        return NULL_VAL();
    }

    if (arg_count == 1) {
        if (IS_BUF(args[0])) {
            ObjBuf* buf = AS_BUF(args[0]);
            size_t n = fwrite(buf->bytes, sizeof(uint8_t), buf->count, file->stream);
            if (n < buf->count) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
                return NULL_VAL();
            }
            return I64_VAL((int64_t)n);
        } else {
            ObjStr* string = pyro_stringify_value(vm, args[0]);
            if (vm->halt_flag) {
                return NULL_VAL();
            }
            if (!string) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                return NULL_VAL();
            }
            size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);
            if (n < string->length) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
                return NULL_VAL();
            }
            return I64_VAL((int64_t)n);
        }
    }

    if (!IS_STR(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "First argument to :write() should be a format string.");
        return NULL_VAL();
    }

    Value formatted = pyro_fn_fmt(vm, arg_count, args);
    if (vm->halt_flag) {
        return NULL_VAL();
    }
    ObjStr* string = AS_STR(formatted);

    size_t n = fwrite(string->bytes, sizeof(char), string->length, file->stream);

    if (n < string->length) {
        pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
        return NULL_VAL();
    }
    return I64_VAL((int64_t)n);
}


static Value file_write_byte(PyroVM* vm, size_t arg_count, Value* args) {
    ObjFile* file = AS_FILE(args[-1]);

    if (!IS_I64(args[0])) {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid argument type for :write_byte().");
        return NULL_VAL();
    }

    int64_t value = args[0].as.i64;
    if (value < 0 || value > 255) {
        pyro_panic(vm, ERR_OUT_OF_RANGE, "Argument to :write_byte() is out of range.");
        return NULL_VAL();
    }

    uint8_t byte = (uint8_t)value;
    size_t n = fwrite(&byte, sizeof(uint8_t), 1, file->stream);
    if (n < 1) {
        pyro_panic(vm, ERR_OS_ERROR, "I/O write error.");
        return NULL_VAL();
    }

    return NULL_VAL();
}


void pyro_load_std_core_file(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$file", fn_file, 2);
    pyro_define_global_fn(vm, "$is_file", fn_is_file, 1);

    // Methods.
    pyro_define_method(vm, vm->file_class, "close", file_close, 0);
    pyro_define_method(vm, vm->file_class, "flush", file_flush, 0);
    pyro_define_method(vm, vm->file_class, "read", file_read, 0);
    pyro_define_method(vm, vm->file_class, "read_string", file_read_string, 0);
    pyro_define_method(vm, vm->file_class, "read_line", file_read_line, 0);
    pyro_define_method(vm, vm->file_class, "read_bytes", file_read_bytes, 1);
    pyro_define_method(vm, vm->file_class, "read_byte", file_read_byte, 0);
    pyro_define_method(vm, vm->file_class, "write", file_write, -1);
    pyro_define_method(vm, vm->file_class, "write_byte", file_write_byte, 1);
}
