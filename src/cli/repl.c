#include "cli.h"


// Returns true if [code] contains an open quote.
static bool has_open_quote(const char* code, size_t code_count) {
    bool in_quotes = false;
    size_t index = 0;

    while (index < code_count) {
        char c = code[index++];

        if (c == '`') {
            in_quotes = true;
            while (index < code_count) {
                if (code[index++] == '`') {
                    in_quotes = false;
                    break;
                }
            }
        }

        else if (c == '"') {
            in_quotes = true;
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '"') {
                    in_quotes = false;
                    break;
                }
            }
        }

        else if (c == '\'') {
            in_quotes = true;
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '\'') {
                    in_quotes = false;
                    break;
                }
            }
        }
    }

    return in_quotes;
}


// Returns the number of opening brackets minus the number of closing brackets.
static int count_brackets(const char* code, size_t code_count) {
    int bracket_count = 0;
    size_t index = 0;

    while (index < code_count) {
        char c = code[index++];

        if (c == '(' || c == '[' || c == '{') {
            bracket_count++;
        }

        else if (c == ')' || c == ']' || c == '}') {
            bracket_count--;
        }

        else if (c == '`') {
            while (index < code_count && code[index++] != '`');
        }

        else if (c == '"') {
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '"') {
                    break;
                }
            }
        }

        else if (c == '\'') {
            while (index < code_count) {
                c = code[index++];
                if (c == '\\') {
                    index++;
                } else if (c == '\'') {
                    break;
                }
            }
        }
    }

    return bracket_count;
}


void pyro_run_repl(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "Error: Out of memory, unable to initialize Pyro VM.\n");
        exit(1);
    }

    // Set the VM"s max memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add any import roots supplied on the command line.
    pyro_cli_add_import_roots(vm, parser);

    // Add the current working directory to the list of import roots.
    pyro_add_import_root(vm, ".");

    printf("Pyro %s -- Type 'exit' to quit.\n", PYRO_VERSION_STRING);

    char* code = NULL;
    size_t code_count = 0;

    for (;;) {
        if (code) {
            printf("\x1B[90m···\x1B[0m ");
        } else {
            printf("\x1B[90m>>>\x1B[0m ");
        }

        char* line = NULL;
        size_t line_capacity = 0;
        ssize_t line_count = getline(&line, &line_capacity, stdin);

        if (line_count == -1) {
            if (feof(stdin)) {
                printf("EOF\n");
                exit(0);
            } else {
                fprintf(stderr, "Error: Failed to read input from STDIN.\n");
                exit(1);
            }
        }

        if (strcmp(line, "exit\n") == 0) {
            exit(0);
        }

        code = realloc(code, code_count + line_count);
        if (!code) {
            fprintf(stderr, "Error: Failed to allocate memory for input.\n");
            exit(1);
        }

        memcpy(&code[code_count], line, line_count);
        code_count += line_count;
        free(line);

        // If the code contains unclosed quotes or more opening brackets than closing brackets,
        // read and append another line of input.
        if (has_open_quote(code, code_count) || count_brackets(code, code_count) > 0) {
            continue;
        }

        pyro_exec_code_as_main(vm, code, code_count, "repl");
        if (pyro_get_exit_flag(vm) || pyro_get_hard_panic_flag(vm)) {
            exit(pyro_get_status_code(vm));
        }

        free(code);
        code = NULL;
        code_count = 0;
    }

    pyro_free_vm(vm);
}
