#ifndef pyro_cli_h
#define pyro_cli_h

#include "../inc/pyro.h"
#include "../lib/args/args.h"
#include "../lib/bestline/bestline.h"

void pyro_cli_run_file(ArgParser* parser);
void pyro_cli_run_repl(ArgParser* parser);

void pyro_cli_cmd_test(char* cmd_name, ArgParser* cmd_parser);
void pyro_cli_cmd_time(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_check(char* cmd_name, ArgParser* cmd_parser);

void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_import_roots_from_command_line(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path);
void pyro_cli_add_import_roots_from_environment(PyroVM* vm);
char* pyro_cli_sprintf(const char* format_string, ...);
size_t pyro_cli_get_stack_size(ArgParser* parser);

#endif
