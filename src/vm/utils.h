#ifndef pyro_utils_h
#define pyro_utils_h

#include "common.h"

// Output struct for [pyro_read_file].
typedef struct {
    size_t size;
    char* data;
} FileData;

// Attempts to read the content of a file from the filesystem. Returns [true] if the file has been
// successfully loaded. In this case [fd.data] should be freed using
//
//   FREE_ARRAY(vm, char, fd.data, fd.size)
//
// Panics and returns [false] in case of error. If the file has zero length the return value will
// be true and [fd.data] will be NULL.
bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd);

// This function prints to an automatically-allocated, null-terminated string. It panics and returns
// NULL if a formatting error occurs or if sufficient memory cannot be allocated. The caller is
// responsible for freeing the returned string via:
//
//   FREE_ARRAY(vm, char, string, strlen(string) + 1)
//
char* pyro_sprintf(PyroVM* vm, const char* format_string, ...);

// String hash functions, 64-bit versions.
uint64_t pyro_fnv1a_64(const char* string, size_t length);
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length);
uint64_t pyro_djb2_64(const char* string, size_t length);
uint64_t pyro_sdbm_64(const char* string, size_t length);

// Copies [src_len] bytes from the string [src] to the buffer [dst], replacing backslashed escapes.
// Does not add a terminating null to [dst]. Returns the number of bytes written to [dst] -- this
// will be less than or equal to [src_len].
size_t pyro_unescape_string(const char* src, size_t src_len, char* dst);

// Parse a string as an integer. The string can contain underscores and can begin with 0b, 0o, or
// 0x to specify the base. Returns true if successful, false if the string was invalid.
bool pyro_parse_string_as_int(const char* string, size_t length, int64_t* value);

// Parse a string as a float. The string can contain underscores. Returns true if successful,
// false if the string was invalid.
bool pyro_parse_string_as_float(const char* string, size_t length, double* value);

// Called to signal that an error has occurred.
void pyro_panic(PyroVM* vm, int64_t error_code, const char* format, ...);

// Writes a printf-style formatted string to the VM's output stream, unless that stream is NULL.
void pyro_out(PyroVM* vm, const char* format, ...);

// Writes a printf-style formatted string to the VM's error stream, unless that stream is NULL.
void pyro_err(PyroVM* vm, const char* format, ...);

#endif
