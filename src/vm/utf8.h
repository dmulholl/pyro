#ifndef pyro_utf8_h
#define pyro_utf8_h

#include "common.h"

// Output struct for [pyro_read_utf8_codepoint]. [length] is the number of bytes in the utf-8
// encoding.
typedef struct {
    uint32_t codepoint;
    uint8_t length;
} Utf8CodePoint;

// Attempts to read a utf-8-encoded unicode codepoint from [src].  Returns [true] if a valid
// codepoint can be read, otherwise [false].
bool pyro_read_utf8_codepoint(uint8_t src[], size_t src_len, Utf8CodePoint* out);

// Writes a unicode codepoint to [dst], encoded as utf-8. Returns the number of bytes written
// (between 1 and 4). Does not validate that [codepoint] is within the valid range for unicode
// codepoints.
uint8_t pyro_write_utf8_codepoint(uint32_t codepoint, uint8_t dst[]);

#endif
