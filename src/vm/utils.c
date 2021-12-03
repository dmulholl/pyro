#include "utils.h"
#include "heap.h"
#include "vm.h"
#include "utf8.h"
#include "errors.h"

#include "../lib/os/os.h"


bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        pyro_panic(vm, ERR_OS_ERROR, "Unable to open file '%s'.", path);
        return false;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    if (file_size == 0) {
        fclose(file);
        *fd = (FileData) {
            .size = 0,
            .data = NULL
        };
        return true;
    }

    char* file_data = ALLOCATE_ARRAY(vm, char, file_size);
    if (file_data == NULL) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Insufficient memory to read file '%s'.", path);
        return false;
    }

    size_t bytes_read = fread(file_data, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        pyro_panic(vm, ERR_OS_ERROR, "I/O Read Error. Unable to read file '%s'.", path);
        FREE_ARRAY(vm, char, file_data, file_size);
        return false;
    }

    fclose(file);
    *fd = (FileData) {
        .size = file_size,
        .data = file_data
    };
    return true;
}


// Possible optimization: print first to a static buffer then memcpy to the output array if it fits.
char* pyro_str_fmt(PyroVM* vm, const char* fmtstr, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [len] will be the output string length,
    // not counting the terminating null, so we'll need to allocate [len + 1] bytes.
    va_start(args, fmtstr);
    int len = vsnprintf(NULL, 0, fmtstr, args);
    va_end(args);

    // If [len] is negative, an encoding error occurred.
    if (len < 0) {
        return NULL;
    }

    char* string = ALLOCATE_ARRAY(vm, char, len + 1);
    if (string == NULL) {
        return NULL;
    }

    va_start(args, fmtstr);
    vsnprintf(string, len + 1, fmtstr, args);
    va_end(args);

    return string;
}


char* pyro_str_dup(PyroVM* vm, const char* source) {
    size_t length = strlen(source);
    char* dest = ALLOCATE_ARRAY(vm, char, length + 1);
    if (dest == NULL) {
        return NULL;
    }
    memcpy(dest, source, length);
    dest[length] = '\0';
    return dest;
}


char* pyro_str_copy(PyroVM* vm, const char* source, size_t n) {
    char* dest = ALLOCATE_ARRAY(vm, char, n + 1);
    if (dest == NULL) {
        return NULL;
    }
    memcpy(dest, source, n);
    dest[n] = '\0';
    return dest;
}


char* pyro_str_cat(PyroVM* vm, const char* a, const char* b) {
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t length = len_a + len_b;

    char* dest = ALLOCATE_ARRAY(vm, char, length + 1);
    if (dest == NULL) {
        return NULL;
    }

    memcpy(dest, a, len_a);
    memcpy(dest + len_a, b, len_b);
    dest[length] = '\0';
    return dest;
}


// String hash: FNV-1a, 64-bit version.
uint64_t pyro_fnv1a_64(const char* string, size_t length) {
    uint64_t hash = UINT64_C(14695981039346656037);

    // Function: hash(i) = (hash(i - 1) XOR string[i]) * 1099511628211
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}


// String hash: FNV-1a, 64-bit version with optimized multiplication.
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length) {
    uint64_t hash = UINT64_C(14695981039346656037);

    // Function: hash(i) = (hash(i - 1) XOR string[i]) * 1099511628211
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash += (hash<<1) + (hash<<4) + (hash<<5) + (hash<<7) + (hash<<8) + (hash<<40);
    }

    return hash;
}


// String hash: DJB2, 64-bit version.
uint64_t pyro_djb2_64(const char* string, size_t length) {
    uint64_t hash = UINT64_C(5381);

    // Function: hash(i) = hash(i - 1) * 33 + string[i]
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)string[i];
    }

    return hash;
}


// String hash: SDBM, 64-bit version.
uint64_t pyro_sdbm_64(const char* string, size_t length) {
    uint64_t hash = 0;

    // Function: hash(i) = hash(i - 1) * 65599 + string[i]
    for (size_t i = 0; i < length; i++) {
        hash = (uint8_t)string[i] + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}


// Converts an ASCII hex digit to its corresponding integer value.
static inline uint8_t hex_to_int(char c) {
   return (c > '9') ? (c &~ 0x20) - 'A' + 10 : (c - '0');
}


size_t pyro_unescape_string(const char* src, size_t src_len, char* dst) {
    if (src_len == 0) {
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '\\' && i < src_len - 1) {
            switch (src[i + 1]) {
                case '\\':
                    dst[count++] = '\\';
                    i++;
                    break;
                case '0':
                    dst[count++] = 0;
                    i++;
                    break;
                case '"':
                    dst[count++] = '"';
                    i++;
                    break;
                case '\'':
                    dst[count++] = '\'';
                    i++;
                    break;
                case 'b':
                    dst[count++] = 8;
                    i++;
                    break;
                case 'n':
                    dst[count++] = 10;
                    i++;
                    break;
                case 'r':
                    dst[count++] = 13;
                    i++;
                    break;
                case 't':
                    dst[count++] = 9;
                    i++;
                    break;

                // 8-bit hex-encoded byte value: \xXX.
                case 'x':
                    if (src_len - i > 3 &&
                        isxdigit(src[i + 2]) &&
                        isxdigit(src[i + 3])
                    ) {
                        dst[count++] = (hex_to_int(src[i + 2]) << 4) | hex_to_int(src[i + 3]);
                        i += 3;
                    } else {
                        dst[count++] = src[i];
                    }
                    break;

                // 16-bit hex-encoded unicode code point: \uXXXX. Output as utf-8.
                case 'u': {
                    if (src_len - i > 5 &&
                        isxdigit(src[i + 2]) &&
                        isxdigit(src[i + 3]) &&
                        isxdigit(src[i + 4]) &&
                        isxdigit(src[i + 5])
                    ) {
                        uint16_t codepoint =
                            (hex_to_int(src[i + 2]) << 12) |
                            (hex_to_int(src[i + 3]) << 8)  |
                            (hex_to_int(src[i + 4]) << 4)  |
                            (hex_to_int(src[i + 5]));
                        i += 5;
                        count += pyro_write_utf8_codepoint(codepoint, (uint8_t*)&dst[count]);
                    } else {
                        dst[count++] = src[i];
                    }
                    break;
                }

                // 32-bit hex-encoded unicode code point: \UXXXXXXXX. Output as utf-8.
                case 'U': {
                    if (src_len - i > 9 &&
                        isxdigit(src[i + 2]) &&
                        isxdigit(src[i + 3]) &&
                        isxdigit(src[i + 4]) &&
                        isxdigit(src[i + 5]) &&
                        isxdigit(src[i + 6]) &&
                        isxdigit(src[i + 7]) &&
                        isxdigit(src[i + 8]) &&
                        isxdigit(src[i + 9])
                    ) {
                        uint32_t codepoint =
                            (hex_to_int(src[i + 2]) << 28) |
                            (hex_to_int(src[i + 3]) << 24) |
                            (hex_to_int(src[i + 4]) << 20) |
                            (hex_to_int(src[i + 5]) << 16) |
                            (hex_to_int(src[i + 6]) << 12) |
                            (hex_to_int(src[i + 7]) << 8)  |
                            (hex_to_int(src[i + 8]) << 4)  |
                            (hex_to_int(src[i + 9]));
                        i += 9;
                        count += pyro_write_utf8_codepoint(codepoint, (uint8_t*)&dst[count]);
                    } else {
                        dst[count++] = src[i];
                    }
                    break;
                }

                default:
                    dst[count++] = src[i];
            }
        } else {
            dst[count++] = src[i];
        }
    }

    return count;
}


bool pyro_run_shell_cmd(PyroVM* vm, const char* cmd, ShellResult* out) {
    FILE* file = pyro_popen(cmd, "r");
    if (!file) {
        pyro_panic(vm, ERR_OS_ERROR, "Failed to run comand.");
        return false;
    }

    size_t count = 0;
    size_t capacity = 0;
    uint8_t* array = NULL;

    while (true) {
        if (count + 1 >= capacity) {
            size_t new_capacity = GROW_CAPACITY(capacity);
            uint8_t* new_array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, new_capacity);
            if (!new_array) {
                pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_pclose(file);
                return false;
            }
            capacity = new_capacity;
            array = new_array;
        }

        size_t max_bytes = capacity - count - 1;
        size_t bytes_read = fread(&array[count], sizeof(uint8_t), max_bytes, file);
        count += bytes_read;

        if (bytes_read < max_bytes) {
            if (ferror(file)) {
                pyro_panic(vm, ERR_OS_ERROR, "I/O read error.");
                FREE_ARRAY(vm, uint8_t, array, capacity);
                pyro_pclose(file);
                return false;
            }
            break; // EOF
        }
    }

    int exit_code = pyro_pclose(file);

    if (capacity > count + 1) {
        array = REALLOCATE_ARRAY(vm, uint8_t, array, capacity, count + 1);
        capacity = count + 1;
    }
    array[count] = '\0';

    ObjStr* output = ObjStr_take((char*)array, count, vm);
    if (!output) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        FREE_ARRAY(vm, uint8_t, array, capacity);
        return false;
    }

    *out = (ShellResult) {
        .output = output,
        .exit_code = exit_code
    };

    return true;
}
