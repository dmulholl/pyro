#ifndef pyro_embeds_h
#define pyro_embeds_h

// Attempts to locate the embedded file identified by [path]. Returns false if the file cannot be found.
bool pyro_find_embedded_file(const char* path, const uint8_t** compressed_data, size_t* compressed_bytecount, size_t* uncompressed_bytecount);

// Attempts to load the embedded file identified by [path]. Returns NULL on failure.
PyroBuf* pyro_load_embedded_file(PyroVM* vm, const char* path);

#endif
