#ifndef pyro_utils_h
#define pyro_utils_h

// Reads the file into the buffer. Panics and returns NULL if an error occurs.
PyroBuf* pyro_read_file_into_buf(PyroVM* vm, const char* path, const char* err_prefix);

// Parse a string as an integer. The string can contain underscores and can begin with 0b, 0o,
// or 0x to specify the base. Returns true if successful, false if the string was invalid.
bool pyro_parse_string_as_int(const char* string, size_t length, int64_t* value);

// Parse a string as a float. The string can contain underscores. Returns true if successful,
// false if the string was invalid.
bool pyro_parse_string_as_float(const char* string, size_t length, double* value);

// Dynamically assembles and returns a version string. The caller should free the string using
// free().
char* pyro_get_version_string(void);

// Duplicates a null-terminated C string.
// - Allocates memory using malloc() -- the return value should be freed using free().
// - Returns NULL if memory cannot be allocated for the copy.
char* pyro_strdup(const char* source);

// Takes a null-termminated path as input. Returns a pointer to the beginning of the final
// path element within the input string. Does not modify the input string.
// - "" --> ""
// - "/" --> ""
// - "/bar.txt" --> "bar.txt"
// - "bar.txt/" --> ""
// - "bar.txt" --> "bar.txt"
// - "foo/bar.txt" --> "bar.txt"
// - "/foo/bar.txt" --> "bar.txt"
const char* pyro_basename(const char* path);

// Takes a null-terminated path as input. Returns the length of the directory name prefix
// within the input string. Does not modify the input string.
// - "" --> ""
// - "/" --> "/"
// - "/bar.txt" --> "/"
// - "bar.txt/" --> "bar.txt"
// - "bar.txt" --> ""
// - "foo/bar.txt" --> "foo"
// - "/foo/bar.txt" --> "/foo"
size_t pyro_dirname(const char* path);

// Returns a string with '%' symbols in the input escaped as '%%'.
// Returns NULL if memory cannot be allocated for the new string.
PyroStr* pyro_double_escape_percents(PyroVM* vm, const char* src, size_t src_len);

// Copies [src_count] bytes from [src] to [dst], processing backslashed escape sequences.
// - Returns the number of bytes written to [dst] -- always less than or equal to [src_count].
// - Does not add a terminating null to [dst].
// - [dst] can be the same buffer as [src] - i.e. it's safe for a buffer to overwrite itself.
// - Ignores unrecognised escape sequences - i.e. copies them verbatim to [dst].
size_t pyro_process_backslashed_escapes(const char* src, size_t src_count, char* dst);

// Returns true if [c] is in the ASCII range [a-z] or [A-Z].
bool pyro_is_ascii_alpha(char c);

// Returns true if [c] is an ASCII whitespace character.
bool pyro_is_ascii_ws(char c);

// Returns true if [c] is an ASCII decimal digit.
bool pyro_is_ascii_decimal_digit(char c);

// Returns true if [c] is an ASCII hexadecimal digit.
bool pyro_is_ascii_hex_digit(char c);

// Returns true if [c] is an ASCII octal digit.
bool pyro_is_ascii_octal_digit(char c);

// Returns true if [c] is an ASCII binary digit.
bool pyro_is_ascii_binary_digit(char c);

// Returns true if [c] is a printable ASCII character.
bool pyro_is_ascii_printable(char c);

// Returns true if [c] is in the ASCII range [a-z].
bool pyro_is_ascii_lower(char c);

// Returns true if [c] is in the ASCII range [A-Z].
bool pyro_is_ascii_upper(char c);

// Generates a randomish value suitable for seeding a PRNG. Uses locally available sources of
// entropy. Not suitable for cryptographic use.
uint64_t pyro_random_seed(void);

// String hash: DJB2, 64-bit version.
static inline uint64_t pyro_djb2_64(const uint8_t* string, size_t length) {
    uint64_t hash = UINT64_C(5381);

    // Function: hash(i) = hash(i - 1) * 33 + string[i]
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + string[i];
    }

    return hash;
}

// String hash: FNV-1a, 64-bit version.
static inline uint64_t pyro_fnv1a_64(const uint8_t* string, size_t length) {
    uint64_t hash = UINT64_C(14695981039346656037);

    // Function: hash(i) = (hash(i - 1) XOR string[i]) * 1099511628211
    for (size_t i = 0; i < length; i++) {
        hash ^= string[i];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

// Returns true if the string [s1] is equal to the string [s2].
static inline bool pyro_str_eq(const char* s1, size_t s1_count, const char* s2, size_t s2_count) {
    if (s1_count == s2_count) {
        return memcmp(s1, s2, s1_count) == 0;
    }
    return false;
}

// Simple hash combiner. This is safer than simply using XOR to combine the hashes.
// - XOR maps pairwise identical values to zero.
// - XOR is symmetric so the order of the values is lost.
static inline uint64_t pyro_hash_combine(uint64_t h1, uint64_t h2) {
    // Equivalent to h1 * 3 + h2.
    return (h1 << 1) + h1 + h2;
}

// Like C23's ckd_add(). Returns true if the result would overflow.
static inline bool pyro_ckd_add(int64_t* result, int64_t a, int64_t b) {
    #if __pyro_has_include(<stdckdint.h>)
        return ckd_add(result, a, b);
    #elif __pyro_has_builtin(__builtin_add_overflow)
        return __builtin_add_overflow(a, b, result);
    #else
        if (b > 0 && a > INT64_MAX - b) {
            return true;
        }

        if (b < 0 && a < INT64_MIN - b) {
            return true;
        }

        *result = a + b;
        return false;
    #endif
}

// Like C23's ckd_sub(). Returns true if the result would overflow.
static inline bool pyro_ckd_sub(int64_t* result, int64_t a, int64_t b) {
    #if __pyro_has_include(<stdckdint.h>)
        return ckd_sub(result, a, b);
    #elif __pyro_has_builtin(__builtin_sub_overflow)
        return __builtin_sub_overflow(a, b, result);
    #else
        if (b < 0 && a > INT64_MAX + b) {
            return true;
        }

        if (b > 0 && a < INT64_MIN + b) {
            return true;
        }

        *result = a - b;
        return false;
    #endif
}

// Like C23's ckd_mul(). Returns true if the result would overflow.
static inline bool pyro_ckd_mul(int64_t* result, int64_t a, int64_t b) {
    #if __pyro_has_include(<stdckdint.h>)
        return ckd_mul(result, a, b);
    #elif __pyro_has_builtin(__builtin_mul_overflow)
        return __builtin_mul_overflow(a, b, result);
    #else
        if (a == 0 || b == 0) {
            *result = 0;
            return false;
        }

        if (a > 0) {
            // a > 0, b > 0
            if (b > 0) {
                if (a > INT64_MAX / b) {
                    return true;
                }
                *result = a * b;
                return false;
            }

            // a > 0, b < 0
            if (b != -1 && a > INT64_MIN / b) {
                return true;
            }
            *result = a * b;
            return false;
        }

        // a < 0, b > 0
        if (b > 0) {
            if (a < INT64_MIN / b) {
                return true;
            }
            *result = a * b;
            return false;
        }

        // a < 0, b < 0
        if (a < INT64_MAX / b) {
            return true;
        }
        *result = a * b;
        return false;
    #endif
}

// Returns the remainder left behind after floored division. This corresponds to the output of the
// modulo operation in mathematics.
static inline int64_t pyro_modulo(int64_t numerator, int64_t denominator) {
    return ((numerator % denominator) + denominator) % denominator;
}

#endif
