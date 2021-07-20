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

// This function prints to an automatically-allocated null-terminated string. Returns NULL if an
// encoding error occurs or if sufficient memory cannot be allocated. The caller is responsible for
// freeing the returned string via:
//
//   FREE_ARRAY(vm, char, string, strlen(string) + 1)
//
char* pyro_str_fmt(PyroVM* vm, const char* fmtstr, ...);

// Duplicates a null-terminated string, automatically allocating memory for the copy. Returns NULL
// if sufficient memory cannot be allocated.
char* pyro_str_dup(PyroVM* vm, const char* source);

// Duplicates an n-character portion of a string, automatically allocating memory for the copy.
// Returns NULL if sufficient memory cannot be allocated.
char* pyro_str_copy(PyroVM* vm, const char* source, size_t n);

// Creates a new string by concatenating two strings, automatically allocating memory for the copy.
// Returns NULL if sufficient memory cannot be allocated.
char* pyro_str_cat(PyroVM* vm, const char* a, const char* b);

// String hash functions, 32-bit versions.
uint32_t pyro_fnv1a_32(const char* string, size_t length);
uint32_t pyro_fnv1a_32_opt(const char* string, size_t length);
uint32_t pyro_djb2_32(const char* string, size_t length);
uint32_t pyro_sdbm_32(const char* string, size_t length);

// String hash functions, 64-bit versions.
uint64_t pyro_fnv1a_64(const char* string, size_t length);
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length);
uint64_t pyro_djb2_64(const char* string, size_t length);
uint64_t pyro_sdbm_64(const char* string, size_t length);

// Copies [src_len] bytes from the string [src] to the buffer [dst], replacing backslashed escapes.
// Does not add a terminating null to [dst]. Returns the number of bytes written to [dst] -- this
// will be less than or equal to [src_len].
size_t pyro_unescape_string(const char* src, size_t src_len, char* dst);

#endif
