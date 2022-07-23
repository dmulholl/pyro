#ifndef pyro_utf8_h
#define pyro_utf8_h

#include "pyro.h"

// Output struct for [pyro_read_utf8_codepoint]. [length] is the number of bytes in the utf-8
// encoding.
typedef struct {
    uint32_t value;
    uint8_t length;
} Utf8CodePoint;

// Attempts to read a utf-8-encoded unicode codepoint from the start of the buffer [src]. Returns
// [true] if a valid codepoint can be read, otherwise [false].
bool pyro_read_utf8_codepoint(const uint8_t src[], size_t src_len, Utf8CodePoint* out);

// Attempts to read a utf-8-encoded unicode codepoint from the end of the buffer [src]. Returns
// [true] if a valid codepoint can be read, otherwise [false].
bool pyro_read_trailing_utf8_codepoint(const uint8_t src[], size_t src_len, Utf8CodePoint* out);

// Writes a unicode codepoint to [dst], encoded as utf-8. Returns the number of bytes written
// (between 1 and 4). Does not validate that [codepoint] is within the valid range for unicode
// codepoints.
uint8_t pyro_write_utf8_codepoint(uint32_t codepoint, uint8_t dst[]);

// Returns [true] if [codepoint] is considered to be a unicode whitespace character.
// Ref: https://en.wikipedia.org/wiki/Whitespace_character
bool pyro_is_unicode_whitespace(uint32_t codepoint);

// Returns [true] if [string] is empty or contains only valid utf-8 codepoints.
bool pyro_is_valid_utf8(const char* string, size_t length);

// Returns [true] if [string] contains the utf-8 encoded [codepoint]. [string] should contain
// only valid utf-8; the function will return [false] if it encounters invalid utf-8.
bool pyro_contains_utf8_codepoint(const char* string, size_t length, uint32_t codepoint);

#endif
