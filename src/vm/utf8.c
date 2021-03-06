#include "utf8.h"


bool pyro_read_utf8_codepoint(uint8_t src[], size_t src_len, Utf8CodePoint* out) {
    if (src_len == 0) {
        return false;
    }

    uint8_t byte1 = src[0];

    // Zero continuation bytes (0 to 127).
    if ((byte1 & 0x80) == 0) {
        *out = (Utf8CodePoint) { .length = 1, .codepoint = byte1 };
        return true;
    }

    // One continuation byte (128 to 2047).
    if ((byte1 & 0xE0) == 0xC0 && src_len > 1) {
        uint8_t byte2 = src[1];

        uint8_t c1;
        if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }

        uint32_t value = ((byte1 & 0x1F) << 6) | c1;
        if (value >= 128) {
            *out = (Utf8CodePoint) { .length = 2, .codepoint = value };
            return true;
        }
    }

    // Two continuation bytes (2048 to 55295 and 57344 to 65535).
    else if ((byte1 & 0xF0) == 0xE0 && src_len > 2) {
        uint8_t byte2 = src[1];
        uint8_t byte3 = src[2];

        uint8_t c1, c2;
        if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }
        if ((byte3 & 0xC0) == 0x80) { c2 = byte3 & 0x3F; } else { return false; }

        uint32_t value = ((byte1 & 0x0F) << 12) | (c1 << 6) | c2;
        if (value >= 2048 && (value < 55296 || value > 57343)) {
            *out = (Utf8CodePoint) { .length = 3, .codepoint = value };
            return true;
        }
    }

    // Three continuation bytes (65536 to 1114111).
    else if ((byte1 & 0xF8) == 0xF0 && src_len > 3) {
        uint8_t byte2 = src[1];
        uint8_t byte3 = src[2];
        uint8_t byte4 = src[3];

        uint8_t c1, c2, c3;
        if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }
        if ((byte3 & 0xC0) == 0x80) { c2 = byte3 & 0x3F; } else { return false; }
        if ((byte4 & 0xC0) == 0x80) { c3 = byte4 & 0x3F; } else { return false; }

        uint32_t value = ((byte1 & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
        if (value >= 65536 && value <= 1114111) {
            *out = (Utf8CodePoint) { .length = 4, .codepoint = value };
            return true;
        }
    }

    return false;
}


uint8_t pyro_write_utf8_codepoint(uint32_t codepoint, uint8_t dst[]) {
    if (codepoint <= 0x7f) {
        dst[0] = codepoint;
        return 1;
    } else if (codepoint <= 0x7ff) {
        dst[0] = 0xc0 | ((codepoint >> 6) & 0x1f);
        dst[1] = 0x80 | (codepoint & 0x3f);
        return 2;
    } else if (codepoint <= 0xffff) {
        dst[0] = 0xe0 | ((codepoint >> 12) & 0x0f);
        dst[1] = 0x80 | ((codepoint >> 6) & 0x3f);
        dst[2] = 0x80 | (codepoint & 0x3f);
        return 3;
    } else {
        dst[0] = 0xf0 | ((codepoint >> 18) & 0x07);
        dst[1] = 0x80 | ((codepoint >> 12) & 0x3f);
        dst[2] = 0x80 | ((codepoint >> 6) & 0x3f);
        dst[3] = 0x80 | (codepoint & 0x3f);
        return 4;
    }
}
