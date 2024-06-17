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
static int count_open_brackets(const char* code, size_t code_count) {
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


int pyro_cli_run_repl(ArgParser* parser) {
    PyroVM* vm = pyro_new_vm();
    if (!vm) {
        fprintf(stderr, "error: out of memory, failed to initialize the Pyro VM\n");
        return 1;
    }

    // Turn on automatic printing of expression statement values.
    pyro_set_repl_flag(vm, true);

    // Set the VM's max memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add import roots.
    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);
    pyro_append_import_root(vm, ".", 1);

    char* version = pyro_get_version_string();
    printf("%s -- Type 'exit' to quit.\n", version);
    free(version);

    char* buffer = NULL;
    size_t buffer_count = 0;

    for (;;) {
        char* line;
        if (buffer) {
            line = bestline("\x1B[90m···\x1B[0m ");
        } else {
            line = bestline("\x1B[90m>>>\x1B[0m ");
        }

        if (!line) {
            printf("\n");
            free(buffer);
            pyro_free_vm(vm);
            return 0;
        }

        if (strcmp(line, "exit") == 0) {
            free(line);
            free(buffer);
            pyro_free_vm(vm);
            return 0;
        }

        bestlineHistoryAdd(line);

        char* new_buffer = realloc(buffer, buffer_count + strlen(line) + 1);
        if (!new_buffer) {
            fprintf(stderr, "error: out of memory\n");
            free(line);
            free(buffer);
            pyro_free_vm(vm);
            return 1;
        }

        buffer = new_buffer;

        memcpy(&buffer[buffer_count], line, strlen(line));
        buffer_count += strlen(line) + 1;
        buffer[buffer_count - 1] = '\n';
        free(line);

        // If the buffer contains unclosed quotes or more opening brackets than closing brackets,
        // read and append another line of input.
        if (has_open_quote(buffer, buffer_count) || count_open_brackets(buffer, buffer_count) > 0) {
            continue;
        }

        // Tack a semicolon onto the end. Multiple semicolons are simply ignored.
        // This will replace the trailing newline character.
        buffer[buffer_count - 1] = ';';

        pyro_exec_code(vm, buffer, buffer_count, "<repl>", NULL);

        free(buffer);
        buffer = NULL;
        buffer_count = 0;

        if (pyro_get_exit_flag(vm)) {
            int exit_code = (int)pyro_get_exit_code(vm);
            pyro_free_vm(vm);
            return exit_code;
        }

        if (pyro_get_panic_flag(vm)) {
            pyro_reset_vm(vm);
        }
    }
}
