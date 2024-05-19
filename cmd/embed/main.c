// C standard library -- no OS-dependent headers.
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// POSIX: stat(), S_ISDIR(), S_ISREG, S_ISLNK()
#include <sys/stat.h>

// POSIX: opendir(), closedir()
#include <dirent.h>

#include "../../lib/lz4/lz4.h"

const char* HEADER =
    "#include <string.h>\n"
    "#include <stddef.h>\n"
    "#include <stdbool.h>\n"
    "#include <stdint.h>\n"
    "\n"
    "typedef struct {\n"
    "    const char* path;\n"
    "    const uint8_t* compressed_data;\n"
    "    size_t compressed_bytecount;\n"
    "    size_t uncompressed_bytecount;\n"
    "} Entry;\n"
    "\n"
    "static const Entry entries[] = {\n"
    ;

const char* FOOTER =
    "};\n"
    "\n"
    "static const size_t entry_count = sizeof(entries) / sizeof(Entry);\n"
    "\n"
    "bool pyro_find_embedded_file(const char* path, const uint8_t** compressed_data, size_t* compressed_bytecount, size_t* uncompressed_bytecount) {\n"
    "    for (size_t i = 0; i < entry_count; i++) {\n"
    "        Entry entry = entries[i];\n"
    "        if (strcmp(path, entry.path) == 0) {\n"
    "            *compressed_data = entry.compressed_data;\n"
    "            *compressed_bytecount = entry.compressed_bytecount;\n"
    "            *uncompressed_bytecount = entry.uncompressed_bytecount;\n"
    "            return true;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    return false;\n"
    "}\n"
    ;

// If [path] is a symlink, stat() returns info about the target of the link.
bool is_file(const char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        if (S_ISREG(s.st_mode)) {
            return true;
        }
    }
    return false;
}

// If [path] is a symlink, stat() returns info about the target of the link.
bool is_dir(const char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        if (S_ISDIR(s.st_mode)) {
            return true;
        }
    }
    return false;
}

const char* get_basename(const char* path) {
    const char* start = path + strlen(path);

    while (start > path) {
        start--;
        if (*start == '/') {
            start++;
            break;
        }
    }

    return start;
}

void embed_file(const char* path, const char* embed_path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fclose(file);
        fprintf(stderr, "error: failed to open file: %s\n", path);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    if (file_size == 0) {
        fclose(file);
        fprintf(stderr, "error: file is empty: %s\n", path);
        exit(1);
    }

    uint8_t* uncompressed_buffer = malloc(file_size);
    if (!uncompressed_buffer) {
        fclose(file);
        fprintf(stderr, "error: out of memory: failed to allocate memory for file: %s\n", path);
        exit(1);
    }

    size_t num_bytes_read = fread(uncompressed_buffer, sizeof(uint8_t), file_size, file);
    if (num_bytes_read < file_size) {
        fclose(file);
        free(uncompressed_buffer);
        fprintf(stderr, "error: failed to read file: %s\n", path);
        exit(1);
    }

    fclose(file);

    int compressed_buffer_capacity = LZ4_compressBound(file_size);

    uint8_t* compressed_buffer = malloc(compressed_buffer_capacity);
    if (!compressed_buffer) {
        free(uncompressed_buffer);
        fprintf(stderr, "error: out of memory: failed to allocate memory for file: %s\n", path);
        exit(1);
    }

    int compressed_buffer_count = LZ4_compress_default((char*)uncompressed_buffer, (char*)compressed_buffer, file_size, compressed_buffer_capacity);
    if (compressed_buffer_count == 0) {
        free(uncompressed_buffer);
        free(compressed_buffer);
        fprintf(stderr, "error: failed to compress file: %s\n", path);
        exit(1);
    }

    free(uncompressed_buffer);

    printf("    {\n        \"%s\",\n        (const uint8_t[]){", embed_path);

    for (int i = 0; i < compressed_buffer_count; i++) {
        if (i % 12 == 0) {
            printf("\n            ");
        }

        printf("0x%02X, ", compressed_buffer[i]);
    }

    printf("\n");
    printf("        },\n");
    printf("        %d,\n", compressed_buffer_count);
    printf("        %zu,\n", file_size);
    printf("    },\n");

    free(compressed_buffer);
}

void embed_directory(const char* path, const char* embed_path) {
    DIR* dirp = opendir(path);
    if (!dirp) {
        fprintf(stderr, "error: failed to open directory: %s\n", path);
        exit(1);
    }

    struct dirent* dp;

    while ((dp = readdir(dirp)) != NULL) {
        char* entry_name = dp->d_name;
        size_t entry_name_length = strlen(entry_name);

        if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
            continue;
        }

        char* entry_path = malloc(strlen(path) + strlen("/") + entry_name_length + 1);
        if (!entry_path) {
            fprintf(stderr, "error: out of memory");
            exit(1);
        }
        memcpy(entry_path, path, strlen(path));
        entry_path[strlen(path)] = '/';
        memcpy(entry_path + strlen(path) + 1, entry_name, entry_name_length);
        entry_path[strlen(path) + strlen("/") + entry_name_length] = '\0';

        char* entry_embed_path;
        if (strlen(embed_path) == 0) {
            entry_embed_path = malloc(entry_name_length + 1);
            if (!entry_embed_path) {
                fprintf(stderr, "error: out of memory");
                exit(1);
            }
            memcpy(entry_embed_path, entry_name, entry_name_length);
            entry_embed_path[entry_name_length] = '\0';
        } else {
            entry_embed_path = malloc(strlen(embed_path) + strlen("/") + entry_name_length + 1);
            if (!entry_embed_path) {
                fprintf(stderr, "error: out of memory");
                exit(1);
            }
            memcpy(entry_embed_path, embed_path, strlen(embed_path));
            entry_embed_path[strlen(embed_path)] = '/';
            memcpy(entry_embed_path + strlen(embed_path) + 1, entry_name, entry_name_length);
            entry_embed_path[strlen(embed_path) + strlen("/") + entry_name_length] = '\0';
        }

        if (is_file(entry_path)) {
            embed_file(entry_path, entry_embed_path);
        } else if (is_dir(entry_path)) {
            embed_directory(entry_path, entry_embed_path);
        }

        free(entry_embed_path);
        free(entry_path);
    }

    closedir(dirp);
}

int main(int argc, char** argv) {
    printf("%s", HEADER);

    for (int i = 1; i < argc; i++) {
        char* path = argv[i];

        if (is_file(path)) {
            embed_file(path, get_basename(path));
        }

        if (is_dir(path)) {
            embed_directory(path, "");
        }
    }

    printf("%s", FOOTER);
    return 0;
}
