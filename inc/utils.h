#ifndef pyro_utils_h
#define pyro_utils_h

// Output struct for [pyro_read_file].
typedef struct {
    size_t size;
    char* data;
} FileData;

// Attempts to read the content of a file from the filesystem. Returns [true] if the file has been
// successfully loaded. In this case [fd.data] should be freed using
//
//   PYRO_FREE_ARRAY(vm, char, fd.data, fd.size)
//
// Panics and returns [false] in case of error. If the file has zero length the return value will
// be true and [fd.data] will be NULL.
bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd);

// String hash functions, 64-bit versions.
uint64_t pyro_fnv1a_64(const char* string, size_t length);
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length);
uint64_t pyro_djb2_64(const char* string, size_t length);
uint64_t pyro_sdbm_64(const char* string, size_t length);

// Parse a string as an integer. The string can contain underscores and can begin with 0b, 0o, or
// 0x to specify the base. Returns true if successful, false if the string was invalid.
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

// Takes a null-termminated path as input. Returns a pointer to the beginning of the final path
// element within the input string. Does not modify the input string.
// - "" --> ""
// - "/" --> ""
// - "/bar.txt" --> "bar.txt"
// - "bar.txt/" --> ""
// - "bar.txt" --> "bar.txt"
// - "foo/bar.txt" --> "bar.txt"
// - "/foo/bar.txt" --> "bar.txt"
const char* pyro_basename(const char* path);

// Takes a null-terminated path as input. Returns the length of the directory name prefix within
// the input string. Does not modify the input string.
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
// - Returns the number of bytes written to [dst] - this will be less than or equal to [src_count].
// - Does not add a terminating null to [dst].
// - [dst] can be the same buffer as [src] - i.e. it's safe for a buffer to overwrite itself.
// - Ignores unrecognised escape sequences - i.e. copies them verbatim to [dst].
size_t pyro_process_backslashed_escapes(const char* src, size_t src_count, char* dst);

#endif
