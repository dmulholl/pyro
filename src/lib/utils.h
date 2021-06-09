#ifndef pyro_utils_h
#define pyro_utils_h

#include "common.h"

typedef struct {
    size_t size;
    char* data;
} FileData;

bool pyro_read_file(PyroVM* vm, const char* path, FileData* fd);

char* pyro_str_fmt(PyroVM* vm, const char* fmtstr, ...);
char* pyro_str_dup(PyroVM* vm, const char* source);
char* pyro_str_copy(PyroVM* vm, const char* source, size_t n);
char* pyro_str_cat(PyroVM* vm, const char* a, const char* b);
void pyro_str_append(PyroVM* vm, char** a_ptr, const char* b);

uint32_t pyro_fnv1a_32(const char* string, size_t length);
uint32_t pyro_fnv1a_32_opt(const char* string, size_t length);
uint32_t pyro_djb2_32(const char* string, size_t length);
uint32_t pyro_sdbm_32(const char* string, size_t length);

uint64_t pyro_fnv1a_64(const char* string, size_t length);
uint64_t pyro_fnv1a_64_opt(const char* string, size_t length);
uint64_t pyro_djb2_64(const char* string, size_t length);
uint64_t pyro_sdbm_64(const char* string, size_t length);

int pyro_hex_to_int(char c);
bool pyro_is_hex(char c);

bool pyro_file_exists(const char* path);
bool pyro_dir_exists(const char* path);

size_t pyro_unescape_string(const char* src, size_t src_len, char* dst);

#endif

