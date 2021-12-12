#include "cli.h"


// [path] can be a script file or a module directory.
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path) {
    char* realpath = pyro_realpath(path);
    if (!realpath) {
        fprintf(stderr, "Error: Failed to resolve path '%s'.\n", path);
        exit(1);
    }

    // Add the directory containing [path].
    char* dirpath = pyro_dirname(realpath);
    size_t dirpath_length = strlen(dirpath);
    pyro_add_import_root(vm, dirpath);

    char* array = malloc(dirpath_length + strlen("/modules") + 1);
    if (!array) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(2);
    }

    // Add [dirpath/modules].
    memcpy(array, dirpath, dirpath_length);
    memcpy(&array[dirpath_length], "/modules", strlen("/modules"));
    array[dirpath_length + strlen("/modules")] = '\0';
    pyro_add_import_root(vm, array);

    free(array);
    free(realpath);
}


void pyro_cli_add_command_line_import_roots(PyroVM* vm, ArgParser* parser) {
    if (!ap_found(parser, "import-root")) {
        return;
    }

    int arg_count = ap_count(parser, "import-root");

    for (int i = 0; i < arg_count; i++) {
        char* root = ap_str_value_at_index(parser, "import-root", i);
        pyro_add_import_root(vm, root);
    }
}


void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser) {
    if (!ap_found(parser, "max-memory")) {
        return;
    }

    const size_t buf_len = 24;
    char buf[24];

    char* arg = ap_str_value(parser, "max-memory");
    size_t arg_len = strlen(arg);

    if (arg_len == 0) {
        fprintf(stderr, "Error: --max-memory argument is invalid.\n");
        exit(1);
    }

    if (arg_len + 1 > buf_len) {
        fprintf(stderr, "Error: --max-memory argument is too long.\n");
        exit(1);
    }

    memcpy(buf, arg, arg_len + 1);

    size_t multiplier = 1;

    if (buf[arg_len - 1] == 'K') {
        multiplier = 1024;
        buf[arg_len - 1] = '\0';
    } else if (buf[arg_len - 1] == 'M') {
        multiplier = 1024 * 1024;
        buf[arg_len - 1] = '\0';
    } else if (buf[arg_len - 1] == 'G') {
        multiplier = 1024 * 1024 * 1024;
        buf[arg_len - 1] = '\0';
    }

    errno = 0;
    int64_t value = strtoll(buf, NULL, 10);
    if (errno != 0 || value <= 0) {
        fprintf(stderr, "Error: --max-memory argument is invalid.\n");
        exit(1);
    }

    pyro_set_max_memory(vm, value * multiplier);
}
