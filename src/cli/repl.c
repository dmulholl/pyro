#include "cli.h"
#include "../lib/linenoise/linenoise.h"


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
        fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
        exit(1);
    }

    // Turn on automatic printing of expression statement values.
    pyro_set_repl_flag(vm, true);

    // Set the VM"s max memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add any import roots supplied on the command line.
    pyro_cli_add_command_line_import_roots(vm, parser);

    // Add the current working directory to the list of import roots.
    pyro_add_import_root(vm, ".");

    printf("Pyro %s -- Type 'exit' to quit.\n", PYRO_VERSION_STRING);

    char* code = NULL;
    size_t code_count = 0;

    for (;;) {
        char* line;
        if (code) {
            line = linenoise("... ");
        } else {
            line = linenoise(">>> ");
        }

        if (!line || strcmp(line, "exit") == 0) {
            break;
        }
        linenoiseHistoryAdd(line);

        code = realloc(code, code_count + strlen(line) + 1);
        if (!code) {
            fprintf(stderr, "Error: Failed to allocate memory for input.\n");
            exit(1);
        }
        memcpy(&code[code_count], line, strlen(line));
        code_count += strlen(line) + 1;
        code[code_count - 1] = '\n';
        free(line);

        // If the code contains unclosed quotes or more opening brackets than closing brackets,
        // read and append another line of input.
        if (has_open_quote(code, code_count) || count_brackets(code, code_count) > 0) {
            continue;
        }

        // Tack a semicolon onto the end. Multiple semicolons are simply ignored.
        code = realloc(code, code_count + 1);
        if (!code) {
            fprintf(stderr, "Error: Failed to allocate memory for input.\n");
            exit(1);
        }
        code[code_count] = ';';
        code_count++;

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
