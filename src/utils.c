#include "../inc/pyro.h"


PyroBuf* pyro_read_file_into_buf(PyroVM* vm, const char* path, const char* err_prefix) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        pyro_panic(vm, "%s: unable to open file '%s'", err_prefix, path);
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    PyroBuf* buf = PyroBuf_new_with_capacity(file_size, vm);
    if (!buf) {
        fclose(file);
        pyro_panic(vm, "%s: out of memory", err_prefix);
        return NULL;
    }

    if (file_size == 0) {
        fclose(file);
        return buf;
    }

    size_t num_bytes_read = fread(buf->bytes, sizeof(uint8_t), file_size, file);
    if (num_bytes_read < file_size) {
        fclose(file);
        pyro_panic(vm, "%s: error reading file '%s'", err_prefix, path);
        return NULL;
    }

    fclose(file);
    buf->count = num_bytes_read;
    return buf;
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


// Copies [src_count] bytes from [src] to [dst], processing backslashed escape sequences.
// - Returns the number of bytes written to [dst] - this will be less than or equal to [src_count].
// - Does not add a terminating null to [dst].
// - [dst] can be the same buffer as [src] - i.e. it's safe for a buffer to overwrite itself.
// - Ignores unrecognised escape sequences - i.e. copies them verbatim to [dst].
size_t pyro_process_backslashed_escapes(const char* src, size_t src_count, char* dst) {
    if (src_count == 0) {
        return 0;
    }

    size_t dst_count = 0;
    size_t i = 0;

    while (i < src_count) {
        size_t src_chars_remaining = src_count - i;

        if (src[i] != '\\' || src_chars_remaining < 2) {
            dst[dst_count++] = src[i++];
            continue;
        }

        switch (src[i + 1]) {
            case '\\':
                dst[dst_count++] = '\\';
                i += 2;
                break;

            case '0':
                dst[dst_count++] = 0;
                i += 2;
                break;

            case '"':
                dst[dst_count++] = '"';
                i += 2;
                break;

            case '\'':
                dst[dst_count++] = '\'';
                i += 2;
                break;

            case '$':
                dst[dst_count++] = '$';
                i += 2;
                break;

            case 'b':
                dst[dst_count++] = 8;
                i += 2;
                break;

            case 'e':
                dst[dst_count++] = 27;
                i += 2;
                break;

            case 'n':
                dst[dst_count++] = 10;
                i += 2;
                break;

            case 'r':
                dst[dst_count++] = 13;
                i += 2;
                break;

            case 't':
                dst[dst_count++] = 9;
                i += 2;
                break;

            // 8-bit hex-encoded byte value: \xXX.
            case 'x':
                if (src_chars_remaining >= 4 &&
                    isxdigit(src[i + 2]) &&
                    isxdigit(src[i + 3])
                ) {
                    dst[dst_count++] = (hex_to_int(src[i + 2]) << 4) | hex_to_int(src[i + 3]);
                    i += 4;
                } else {
                    dst[dst_count++] = src[i++];
                }
                break;

            // 16-bit hex-encoded unicode code point: \uXXXX. Output as utf-8.
            case 'u': {
                if (src_chars_remaining >= 6 &&
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
                    i += 6;
                    dst_count += pyro_write_utf8_codepoint(codepoint, (uint8_t*)&dst[dst_count]);
                } else {
                    dst[dst_count++] = src[i++];
                }
                break;
            }

            // 32-bit hex-encoded unicode code point: \UXXXXXXXX. Output as utf-8.
            case 'U': {
                if (src_chars_remaining >= 10 &&
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
                    i += 10;
                    dst_count += pyro_write_utf8_codepoint(codepoint, (uint8_t*)&dst[dst_count]);
                } else {
                    dst[dst_count++] = src[i++];
                }
                break;
            }

            default:
                dst[dst_count++] = src[i++];
                break;
        }
    }

    return dst_count;
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


char* pyro_strdup(const char* source) {
    size_t length = strlen(source);

    char* array = malloc(length + 1);
    if (!array) {
        return NULL;
    }

    memcpy(array, source, length);
    array[length] = '\0';

    return array;
}


const char* pyro_basename(const char* path) {
    const char* start = path + strlen(path);

    while (start > path) {
        start--;
        if (*start == '/') {
            start++;
            break;
        }
    }

    return start;
}


size_t pyro_dirname(const char* path) {
    size_t path_length = strlen(path);

    if (path_length == 0) {
        return 0;
    }

    if (path_length == 1 && path[0]  == '/') {
        return 1;
    }

    const char* end = path + path_length;

    while (end > path) {
        end--;
        if (*end == '/') {
            break;
        }
    }

    if (end > path) {
        return end - path;
    }

    return (*end == '/') ? 1 : 0;
}


PyroStr* pyro_double_escape_percents(PyroVM* vm, const char* src, size_t src_len) {
    PyroBuf* buf = PyroBuf_new_with_capacity(src_len + 1, vm);
    if (!buf) {
        return NULL;
    }

    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '%') {
            if (!PyroBuf_append_bytes(buf, 2, (uint8_t*)"%%", vm)) {
                return NULL;
            }
        } else {
            if (!PyroBuf_append_byte(buf, src[i], vm)) {
                return NULL;
            }
        }
    }

    return PyroBuf_to_str(buf, vm);
}
