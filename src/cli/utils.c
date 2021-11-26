#include "cli.h"


void pyro_cli_add_import_roots(PyroVM* vm, ArgParser* parser) {
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
