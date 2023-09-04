#ifndef pyro_utils_h
#define pyro_utils_h

// Reads the file into the buffer. Panics and returns NULL if an error occurs.
PyroBuf* pyro_read_file_into_buf(PyroVM* vm, const char* path, const char* err_prefix);

// String hash functions, 64-bit versions.
uint64_t pyro_fnv1a_64(const char* string, size_t length);
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length);
uint64_t pyro_djb2_64(const char* string, size_t length);
uint64_t pyro_sdbm_64(const char* string, size_t length);

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

// Returns true if the string [s1] is equal to the string [s2].
static inline bool pyro_str_eq(const char* s1, size_t s1_count, const char* s2, size_t s2_count) {
    if (s1_count == s2_count) {
        return memcmp(s1, s2, s1_count) == 0;
    }
    return false;
}

// Returns true if [c] is ASCII A-Z or a-z.
bool pyro_is_ascii_alpha(char c);

// Returns true if [c] is ASCII 0-9.
bool pyro_is_ascii_digit(char c);

// Returns true if [c] is an ASCII whitespace character.
bool pyro_is_ascii_space(char c);

// Returns true if [c] is an ASCII hexadecimal digit.
bool pyro_is_ascii_hex_digit(char c);

// Returns true if [c] is an ASCII octal digit.
bool pyro_is_ascii_octal_digit(char c);

// Returns true if [c] is an ASCII binary digit.
bool pyro_is_ascii_binary_digit(char c);

// Returns true if [c] is a printable ASCII character.
bool pyro_is_ascii_printable(char c);

// Returns true if [c] is a lowercase ASCII character.
bool pyro_is_ascii_lower(char c);

// Returns true if [c] is an uppercase ASCII character.
bool pyro_is_ascii_upper(char c);

// Like C23's ckd_add(). Returns true if the result would overflow.
bool pyro_ckd_add(int64_t* result, int64_t a, int64_t b);

// Like C23's ckd_sub(). Returns true if the result would overflow.
bool pyro_ckd_sub(int64_t* result, int64_t a, int64_t b);

// Like C23's ckd_mul(). Returns true if the result would overflow.
bool pyro_ckd_mul(int64_t* result, int64_t a, int64_t b);

// Simple hash combiner. Returns h1 * 3 + h2. This is safer than simply using XOR to combine
// the hashes as XOR maps pairwise identical values to zero.
uint64_t pyro_hash_combine(uint64_t h1, uint64_t h2);

// Generates a randomish value suitable for seeding a PRNG. Uses locally available sources of
// entropy. Not suitable for cryptographic use.
uint64_t pyro_random_seed();

#endif