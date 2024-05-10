// C standard library -- no OS-dependent headers.
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// POSIX: stat(), S_ISDIR(), S_ISREG, S_ISLNK()
#include <sys/stat.h>

// POSIX: opendir(), closedir()
#include <dirent.h>

const char* HEADER =
    "#include <string.h>\n"
    "#include <stddef.h>\n"
    "#include <stdbool.h>\n"
    "\n"
    "typedef struct {\n"
    "    const char* path;\n"
    "    const unsigned char* data;\n"
    "    size_t count;\n"
    "} Entry;\n"
    "\n"
    "static const Entry entries[] = {\n"
    ;

const char* FOOTER =
    "};\n"
    "\n"
    "static const size_t entry_count = sizeof(entries) / sizeof(Entry);\n"
    "\n"
    "bool pyro_find_embedded_file(const char* path, const unsigned char** data, size_t* count) {\n"
    "    for (size_t i = 0; i < entry_count; i++) {\n"
    "        Entry entry = entries[i];\n"
    "        if (strcmp(path, entry.path) == 0) {\n"
    "            *data = entry.data;\n"
    "            *count = entry.count;\n"
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
        fprintf(stderr, "error: failed to open file: %s\n", path);
        exit(1);
    }

    size_t count = 0;
    printf("    {\n        \"%s\",\n        (const unsigned char[]){", embed_path);

    while (true) {
        int c = fgetc(file);
        if (c == EOF) {
            break;
        }

        if (count % 12 == 0) {
            printf("\n            ");
        }

        printf("0x%02X, ", c);
        count++;
    }

    printf("\n        },\n        %zu\n    },\n", count);
    fclose(file);
}

void embed_dir_contents(const char* path, const char* embed_path) {
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
            embed_dir_contents(entry_path, entry_embed_path);
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
            embed_dir_contents(path, "");
        }
    }

    printf("%s", FOOTER);
    return 0;
}
