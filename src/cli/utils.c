#include "cli.h"


// [path] can be a script file or a module directory.
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path) {
    char* resolved_path = pyro_realpath(path);
    if (!resolved_path) {
        fprintf(stderr, "Error: Unable to resolve path '%s' to determine import roots.\n", path);
        exit(1);
    }

    // Add the directory containing [path].
    char* dirpath = pyro_dirname(resolved_path);
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
    free(resolved_path);
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


static int64_t parse_memory_size(const char* arg) {
    const size_t buf_len = 24;
    char buf[24];

    size_t arg_len = strlen(arg);
    if (arg_len == 0 || arg_len + 1 > buf_len) {
        return -1;
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
        return -1;
    }

    return value * multiplier;
}


void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser) {
    if (!ap_found(parser, "max-memory")) {
        return;
    }

    int64_t size = parse_memory_size(ap_str_value(parser, "max-memory"));
    if (size <= 0) {
        fprintf(stderr, "Error: invalid argument for --max-memory option.\n");
        exit(1);
    }

    pyro_set_max_memory(vm, size);
}


size_t pyro_cli_get_stack_size(ArgParser* parser) {
    if (!ap_found(parser, "stack-size")) {
        return 1024 * 1024;
    }

    int64_t size = parse_memory_size(ap_str_value(parser, "stack-size"));
    if (size <= 0) {
        fprintf(stderr, "Error: invalid argument for --stack-size option.\n");
        exit(1);
    }

    return (size_t)size;
}


char* pyro_cli_sprintf(const char* format_string, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [length] will be the output string length,
    // not counting the terminating null, so we'll need to allocate [length + 1] bytes.
    va_start(args, format_string);
    int length = vsnprintf(NULL, 0, format_string, args);
    va_end(args);

    // If [length] is negative, an encoding error occurred.
    if (length < 0) {
        return NULL;
    }

    char* array = malloc(length + 1);
    if (!array) {
        return NULL;
    }

    va_start(args, format_string);
    vsprintf(array, format_string, args);
    va_end(args);

    return array;
}
