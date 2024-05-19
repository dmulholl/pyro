#include "../includes/pyro.h"
#include "../../lib/lz4/lz4.h"

PyroBuf* pyro_load_embedded_file(PyroVM* vm, const char* path) {
    const uint8_t* compressed_data;
    size_t compressed_bytecount;
    size_t uncompressed_bytecount;

    if (!pyro_find_embedded_file(path, &compressed_data, &compressed_bytecount, &uncompressed_bytecount)) {
        return NULL;
    }

    PyroBuf* buf = PyroBuf_new_with_capacity(uncompressed_bytecount + 1, vm);
    if (!buf) {
        return NULL;
    }

    int result = LZ4_decompress_safe((const char*)compressed_data, (char*)buf->bytes, (int)compressed_bytecount, (int)uncompressed_bytecount);
    if (result < 0) {
        return NULL;
    }

    buf->count = uncompressed_bytecount;
    return buf;
}
