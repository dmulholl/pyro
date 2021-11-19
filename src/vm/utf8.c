#include "utf8.h"


bool pyro_read_utf8_codepoint(const uint8_t src[], size_t src_len, Utf8CodePoint* out) {
    if (src_len == 0) {
        return false;
    }

    uint8_t byte1 = src[0];

    // Zero continuation bytes (0 to 127).
    if ((byte1 & 0x80) == 0) {
        *out = (Utf8CodePoint) { .length = 1, .value = byte1 };
        return true;
    }

    // One continuation byte (128 to 2047).
    if ((byte1 & 0xE0) == 0xC0 && src_len > 1) {
        uint8_t byte2 = src[1];

        uint8_t c1;
        if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }

        uint32_t value = ((byte1 & 0x1F) << 6) | c1;
        if (value >= 128) {
            *out = (Utf8CodePoint) { .length = 2, .value = value };
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
            *out = (Utf8CodePoint) { .length = 3, .value = value };
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
            *out = (Utf8CodePoint) { .length = 4, .value = value };
            return true;
        }
    }

    return false;
}


bool pyro_read_trailing_utf8_codepoint(const uint8_t src[], size_t src_len, Utf8CodePoint* out) {
    // Zero continuation bytes (0 to 127).
    if (src_len > 0) {
        uint8_t byte1 = src[src_len - 1];
        if ((byte1 & 0x80) == 0) {
            *out = (Utf8CodePoint) { .length = 1, .value = byte1 };
            return true;
        }
    }

    // One continuation byte (128 to 2047).
    if (src_len > 1) {
        uint8_t byte1 = src[src_len - 2];
        uint8_t byte2 = src[src_len - 1];

        if ((byte1 & 0xE0) == 0xC0) {
            uint8_t c1;
            if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }

            uint32_t value = ((byte1 & 0x1F) << 6) | c1;
            if (value >= 128) {
                *out = (Utf8CodePoint) { .length = 2, .value = value };
                return true;
            }
        }
    }

    // Two continuation bytes (2048 to 55295 and 57344 to 65535).
    if (src_len > 2) {
        uint8_t byte1 = src[src_len - 3];
        uint8_t byte2 = src[src_len - 2];
        uint8_t byte3 = src[src_len - 1];

        if ((byte1 & 0xF0) == 0xE0) {
            uint8_t c1, c2;
            if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }
            if ((byte3 & 0xC0) == 0x80) { c2 = byte3 & 0x3F; } else { return false; }

            uint32_t value = ((byte1 & 0x0F) << 12) | (c1 << 6) | c2;
            if (value >= 2048 && (value < 55296 || value > 57343)) {
                *out = (Utf8CodePoint) { .length = 3, .value = value };
                return true;
            }
        }
    }

    // Three continuation bytes (65536 to 1114111).
    if (src_len > 3) {
        uint8_t byte1 = src[src_len - 4];
        uint8_t byte2 = src[src_len - 3];
        uint8_t byte3 = src[src_len - 2];
        uint8_t byte4 = src[src_len - 1];

        if ((byte1 & 0xF8) == 0xF0) {
            uint8_t c1, c2, c3;
            if ((byte2 & 0xC0) == 0x80) { c1 = byte2 & 0x3F; } else { return false; }
            if ((byte3 & 0xC0) == 0x80) { c2 = byte3 & 0x3F; } else { return false; }
            if ((byte4 & 0xC0) == 0x80) { c3 = byte4 & 0x3F; } else { return false; }

            uint32_t value = ((byte1 & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
            if (value >= 65536 && value <= 1114111) {
                *out = (Utf8CodePoint) { .length = 4, .value = value };
                return true;
            }
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


bool pyro_is_unicode_whitespace(uint32_t codepoint) {
    if (codepoint >= 9 && codepoint <= 13) {
        return true;
    }
    if (codepoint == 32 || codepoint == 133 || codepoint == 160 || codepoint == 5760) {
        return true;
    }
    if (codepoint >= 8192 && codepoint <= 8202) {
        return true;
    }
    if (codepoint == 8232 || codepoint == 8233 || codepoint == 8239 || codepoint == 8287) {
        return true;
    }
    if (codepoint == 12288) {
        return true;
    }
    return false;
}


bool pyro_is_valid_utf8(const char* string, size_t length) {
    if (length == 0) {
        return true;
    }

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < length) {
        uint8_t* src_buf = (uint8_t*)&string[byte_index];
        size_t src_len = length - byte_index;

        if (pyro_read_utf8_codepoint(src_buf, src_len, &cp)) {
            byte_index += cp.length;
        } else {
            return false;
        }
    }

    return true;
}


bool pyro_contains_utf8_codepoint(const char* string, size_t length, uint32_t codepoint) {
    if (length == 0) {
        return false;
    }

    size_t byte_index = 0;
    Utf8CodePoint cp;

    while (byte_index < length) {
        uint8_t* src_buf = (uint8_t*)&string[byte_index];
        size_t src_len = length - byte_index;

        if (pyro_read_utf8_codepoint(src_buf, src_len, &cp)) {
            if (cp.value == codepoint) {
                return true;
            }
            byte_index += cp.length;
        } else {
            return false;
        }
    }

    return false;
}
