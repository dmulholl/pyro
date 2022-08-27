#include "cli.h"
#include "../lib/bestline/bestline.h"


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


void pyro_run_repl(ArgParser* parser) {
    size_t stack_size = pyro_cli_get_stack_size(parser);

    PyroVM* vm = pyro_new_vm(stack_size);
    if (!vm) {
        fprintf(stderr, "Error: Out of memory, unable to initialize the Pyro VM.\n");
        exit(1);
    }

    // Turn on automatic printing of expression statement values.
    pyro_set_repl_flag(vm, true);

    // Set the VM's max memory allocation.
    pyro_cli_set_max_memory(vm, parser);

    // Add import roots.
    pyro_cli_add_import_roots_from_command_line(vm, parser);
    pyro_cli_add_import_roots_from_environment(vm);
    pyro_add_import_root(vm, ".");

    char* version = pyro_get_version_string();
    printf("Pyro %s -- Type 'exit' to quit.\n", version);
    free(version);

    char* code = NULL;
    size_t code_count = 0;

    for (;;) {
        char* line;
        if (code) {
            line = bestline("··· ");
        } else {
            line = bestline(">>> ");
        }

        if (!line) {
            printf("\n");
            break;
        }

        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        bestlineHistoryAdd(line);

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
        if (has_open_quote(code, code_count) || count_open_brackets(code, code_count) > 0) {
            continue;
        }

        // Tack a semicolon onto the end. Multiple semicolons are simply ignored.
        // This will replace the trailing newline character.
        code[code_count - 1] = ';';

        pyro_exec_code_as_main(vm, code, code_count, "<repl>");
        if (pyro_get_exit_flag(vm) || pyro_get_hard_panic_flag(vm)) {
            pyro_free_vm(vm);
            exit(pyro_get_exit_code(vm));
        } else if (pyro_get_panic_flag(vm)) {
            pyro_reset_vm(vm);
        }

        free(code);
        code = NULL;
        code_count = 0;
    }

    pyro_free_vm(vm);
}
