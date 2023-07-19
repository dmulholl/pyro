#include "cli.h"


// Adds the directory containing [path] to the list of import roots.
// - [path] can be a script file or a module directory.
// - Adds <parent-directory> to the list of import roots.
// - Adds <parent-directory>/modules to the list of import roots.
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path) {
    // We call realpath() here because we want to add the directory containing the *actual*
    // script or directory at [path]. This may be different from the result of a simple
    // dirname() call if [path] is a symlink.
   char* real_path = pyro_realpath(path);
    if (!real_path) {
        fprintf(stderr, "error: failed to resolve path '%s' to determine import roots\n", path);
        exit(1);
    }

    // Add the directory containing [real_path].
    size_t dirname_length = pyro_dirname(real_path);
    pyro_add_import_root_n(vm, real_path, dirname_length);

    // Add the modules directory.
    char* buffer = malloc(dirname_length + strlen("/modules") + 1);
    if (!buffer) {
        free(real_path);
        fprintf(stderr, "error: out of memory\n");
        exit(1);
    }

    memcpy(buffer, real_path, dirname_length);
    memcpy(&buffer[dirname_length], "/modules", strlen("/modules"));
    buffer[dirname_length + strlen("/modules")] = '\0';
    pyro_add_import_root(vm, buffer);

    free(buffer);
    free(real_path);
}


void pyro_cli_add_import_roots_from_command_line(PyroVM* vm, ArgParser* parser) {
    if (!ap_found(parser, "import-root")) {
        return;
    }

    int arg_count = ap_count(parser, "import-root");

    for (int i = 0; i < arg_count; i++) {
        char* root = ap_get_str_value_at_index(parser, "import-root", i);
        pyro_add_import_root(vm, root);
    }
}


void pyro_cli_add_import_roots_from_environment(PyroVM* vm) {
    char* env_roots_var = getenv("PYRO_IMPORT_ROOTS");
    if (!env_roots_var) {
        return;
    }

    char* array = pyro_strdup(env_roots_var);
    if (!array) {
        fprintf(stderr, "error: out of memory\n");
        exit(2);
    }

    char* token = strtok(array, ":");
    while (token != NULL) {
        pyro_add_import_root(vm, token);
        token = strtok(NULL, ":");
    }

    free(array);
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

    int64_t size = parse_memory_size(ap_get_str_value(parser, "max-memory"));
    if (size <= 0) {
        fprintf(stderr, "error: invalid argument for --max-memory option\n");
        exit(1);
    }

    pyro_set_max_memory(vm, size);
}


char* pyro_cli_sprintf(const char* format_string, ...) {
    va_list args;

    // Figure out how much memory we need to allocate. [length] will be the output string
    // length, not counting the terminating null, so we'll need to allocate [length + 1] bytes.
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
