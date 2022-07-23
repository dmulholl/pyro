#ifndef pyro_cli_h
#define pyro_cli_h

#include "../inc/pyro.h"
#include "../inc/setup.h"
#include "../inc/exec.h"
#include "../inc/os.h"
#include "../inc/utils.h"
#include "../lib/args/args.h"

void pyro_run_file(ArgParser* parser);
void pyro_run_repl(ArgParser* parser);

void pyro_cmd_test(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_time(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_check(char* cmd_name, ArgParser* cmd_parser);

void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_command_line_import_roots(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path);
char* pyro_cli_sprintf(const char* format_string, ...);
size_t pyro_cli_get_stack_size(ArgParser* parser);

#endif
