#ifndef pyro_utf8_h
#define pyro_utf8_h

#include "common.h"


typedef struct {
    uint8_t length;
    uint32_t codepoint;
} Utf8CodePoint;


bool pyro_read_utf8_codepoint(uint8_t src[], size_t src_len, Utf8CodePoint* out);
uint8_t pyro_write_utf8_codepoint(uint32_t codepoint, uint8_t dst[]);


#endif

