#include "utils.h"
#include "heap.h"
#include "vm.h"
#include "utf8.h"


// Attempts to read the content of a file from the filesystem.
// Returns [true] if the file has been successfully loaded. In this case [fd.data] should be freed
// using FREE_ARRAY(vm, char, fd.data, fd.size).
// Panics and returns [false] in case of error.
// If the file has zero length the return value will be true and [fd.data] will be NULL.
bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        pyro_panic(vm, "Unable to open file '%s'.", path);
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
        pyro_panic(vm, "Insufficient memory to read file '%s'.", path);
        return false;
    }

    size_t bytes_read = fread(file_data, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        pyro_panic(vm, "Unable to read file '%s'.", path);
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


// This function prints to an automatically-allocated null-terminated string.
// Returns NULL if an encoding error occurs or if sufficient memory cannot be allocated.
// The caller is responsible for freeing the returned string via:
// FREE_ARRAY(vm, char, string, strlen(string) + 1).
// TODO: print first to a static buffer then memcpy to the output array if it fits?
char* pyro_str_fmt(PyroVM* vm, const char* fmtstr, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. `len` will be the output string length,
    // not counting the terminating null, so we'll need to allocate len + 1 bytes.
    va_start(args, fmtstr);
    int len = vsnprintf(NULL, 0, fmtstr, args);
    va_end(args);

    // If `len` is negative, an encoding error occurred.
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


// Duplicates a null-terminated string, automatically allocating memory for the copy.
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


// Duplicates an n-character portion of a string, automatically allocating memory for the copy.
char* pyro_str_copy(PyroVM* vm, const char* source, size_t n) {
    char* dest = ALLOCATE_ARRAY(vm, char, n + 1);
    if (dest == NULL) {
        return NULL;
    }
    memcpy(dest, source, n);
    dest[n] = '\0';
    return dest;
}


// Creates a new string by concatenating two strings, automatically allocating memory for the copy.
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


// Given two strings A and B, this function appends B to A, automatically allocating memory as
// necessary. String A should be heap-allocated or NULL; string B can be allocated anywhere.
// If the memory allocation fails, the output A will be NULL.
void pyro_str_append(PyroVM* vm, char** a_ptr, const char* b) {
    if (b == NULL) {
        return;
    } else if (*a_ptr == NULL) {
        *a_ptr = pyro_str_dup(vm, b);
    } else {
        size_t len_a = strlen(*a_ptr);
        size_t len_b = strlen(b);
        size_t length = len_a + len_b;

        *a_ptr = REALLOCATE_ARRAY(vm, char, *a_ptr, len_a + 1, length + 1);
        memcpy(*a_ptr + len_a, b, len_b);
        *(*a_ptr + length) = '\0';
    }
}


// String hash: FNV-1a, 32-bit version.
uint32_t pyro_fnv1a_32(const char* string, size_t length) {
    uint32_t hash = UINT32_C(2166136261);

    // Function: hash(i) = (hash(i - 1) XOR string[i]) * 16777619
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }

    return hash;
}


// String hash: FNV-1a, 32-bit version with optimized multiplication.
uint32_t pyro_fnv1a_32_opt(const char* string, size_t length) {
    uint32_t hash = UINT32_C(2166136261);

    // Function: hash(i) = (hash(i - 1) XOR string[i]) * 16777619
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24);
    }

    return hash;
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


// String hash: DJB2, 32-bit version.
uint32_t pyro_djb2_32(const char* string, size_t length) {
    uint32_t hash = UINT32_C(5381);

    // Function: hash(i) = hash(i - 1) * 33 + string[i]
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)string[i];
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


// String hash: SDBM, 32-bit version.
uint32_t pyro_sdbm_32(const char* string, size_t length) {
    uint32_t hash = 0;

    // Function: hash(i) = hash(i - 1) * 65599 + string[i]
    for (size_t i = 0; i < length; i++) {
        hash = (uint8_t)string[i] + (hash << 6) + (hash << 16) - hash;
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


// Convert a hex digit to its corresponding integer value.
int pyro_hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}


// Returns true if the character is a valid hex digit, [0-9A-Fa-f].
bool pyro_is_hex(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'A' && c <= 'F') return true;
    if (c >= 'a' && c <= 'f') return true;
    return false;
}


bool pyro_file_exists(const char* path) {
    return access(path, F_OK) == 0;
}


bool pyro_dir_exists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}


// Copies [src_len] bytes from the string [src] to [dst], replacing backslashed escapes.
// Does not add a terminating null to [dst].
// Returns the number of bytes written to [dst] - this will be less than or equal to [src_len].
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
                        pyro_is_hex(src[i + 2]) &&
                        pyro_is_hex(src[i + 3])
                    ) {
                        dst[count++] = (pyro_hex_to_int(src[i + 2]) << 4) | pyro_hex_to_int(src[i + 3]);
                        i += 3;
                    } else {
                        dst[count++] = src[i];
                    }
                    break;

                // 16-bit hex-encoded unicode code point: \uXXXX. Output as utf-8.
                case 'u': {
                    if (src_len - i > 5 &&
                        pyro_is_hex(src[i + 2]) &&
                        pyro_is_hex(src[i + 3]) &&
                        pyro_is_hex(src[i + 4]) &&
                        pyro_is_hex(src[i + 5])
                    ) {
                        uint16_t codepoint =
                            (pyro_hex_to_int(src[i + 2]) << 12) |
                            (pyro_hex_to_int(src[i + 3]) << 8)  |
                            (pyro_hex_to_int(src[i + 4]) << 4)  |
                            (pyro_hex_to_int(src[i + 5]));
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
                        pyro_is_hex(src[i + 2]) &&
                        pyro_is_hex(src[i + 3]) &&
                        pyro_is_hex(src[i + 4]) &&
                        pyro_is_hex(src[i + 5]) &&
                        pyro_is_hex(src[i + 6]) &&
                        pyro_is_hex(src[i + 7]) &&
                        pyro_is_hex(src[i + 8]) &&
                        pyro_is_hex(src[i + 9])
                    ) {
                        uint32_t codepoint =
                            (pyro_hex_to_int(src[i + 2]) << 28) |
                            (pyro_hex_to_int(src[i + 3]) << 24) |
                            (pyro_hex_to_int(src[i + 4]) << 20) |
                            (pyro_hex_to_int(src[i + 5]) << 16) |
                            (pyro_hex_to_int(src[i + 6]) << 12) |
                            (pyro_hex_to_int(src[i + 7]) << 8)  |
                            (pyro_hex_to_int(src[i + 8]) << 4)  |
                            (pyro_hex_to_int(src[i + 9]));
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

