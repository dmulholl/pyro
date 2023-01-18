#include "../inc/pyro.h"


bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        pyro_panic(vm, "unable to open file '%s'", path);
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

    char* file_data = PYRO_ALLOCATE_ARRAY(vm, char, file_size);
    if (file_data == NULL) {
        pyro_panic(vm, "insufficient memory to read file '%s'", path);
        return false;
    }

    size_t bytes_read = fread(file_data, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        pyro_panic(vm, "I/O read error: unable to read file '%s'", path);
        PYRO_FREE_ARRAY(vm, char, file_data, file_size);
        return false;
    }

    fclose(file);
    *fd = (FileData) {
        .size = file_size,
        .data = file_data
    };
    return true;
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
                case 'e':
                    dst[count++] = 27;
                    i++;
                    break;
                case 'f':
                    dst[count++] = 12;
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
                case 'v':
                    dst[count++] = 11;
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


// The max allowable length for an integer literal is 65 --- a plus or minus sign followed by
// 64 binary digits.
bool pyro_parse_string_as_int(const char* string, size_t length, int64_t* value) {
    char buffer[65 + 1];
    size_t count = 0;
    int base = 10;
    size_t next_index = 0;

    if (length > 1 && string[0] == '0') {
        if (string[1] == 'x') {
            next_index += 2;
            base = 16;
        }
        if (string[1] == 'o') {
            next_index += 2;
            base = 8;
        }
        if (string[1] == 'b') {
            next_index += 2;
            base = 2;
        }
    }

    while (next_index < length) {
        if (string[next_index] == '_') {
            next_index++;
            continue;
        }
        buffer[count] = string[next_index];
        next_index++;
        count++;
        if (count > 65) {
            return false;
        }
    }

    if (count == 0) {
        return false;
    }

    buffer[count] = '\0';
    errno = 0;
    char* endptr;
    *value = strtoll(buffer, &endptr, base);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }

    return true;
}


// May want to revisit the max allowed literal length of 24.
bool pyro_parse_string_as_float(const char* string, size_t length, double* value) {
    char buffer[24 + 1];
    size_t count = 0;
    size_t next_index = 0;

    while (next_index < length) {
        if (string[next_index] == '_') {
            next_index++;
            continue;
        }
        buffer[count] = string[next_index];
        next_index++;
        count++;
        if (count > 24) {
            return false;
        }
    }

    if (count == 0) {
        return false;
    }

    buffer[count] = '\0';
    errno = 0;
    char* endptr;
    *value = strtod(buffer, &endptr);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }

    return true;
}

char* pyro_get_version_string(void) {
    char* format_string;

    if (strlen(PYRO_VERSION_LABEL) > 0) {
        if (strlen(PYRO_VERSION_BUILD) > 0) {
            format_string = "v%d.%d.%d-%s (%s)";
        } else {
            format_string = "v%d.%d.%d-%s";
        }
    } else {
        if (strlen(PYRO_VERSION_BUILD) > 0) {
            format_string = "v%d.%d.%d %.0s(%s)";
        } else {
            format_string = "v%d.%d.%d";
        }
    }

    int length = snprintf(
        NULL,
        0,
        format_string,
        PYRO_VERSION_MAJOR,
        PYRO_VERSION_MINOR,
        PYRO_VERSION_PATCH,
        PYRO_VERSION_LABEL,
        PYRO_VERSION_BUILD
    );

    char* version_string = malloc(length + 1);
    if (!version_string) {
        return NULL;
    }

    snprintf(
        version_string,
        length + 1,
        format_string,
        PYRO_VERSION_MAJOR,
        PYRO_VERSION_MINOR,
        PYRO_VERSION_PATCH,
        PYRO_VERSION_LABEL,
        PYRO_VERSION_BUILD
    );

    return version_string;
}
