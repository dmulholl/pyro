#ifndef pyro_cli_h
#define pyro_cli_h

#include "../vm/pyro.h"
#include "../vm/setup.h"
#include "../vm/exec.h"
#include "../vm/os.h"
#include "../vm/utils.h"
#include "../lib/args/args.h"

void pyro_run_file(ArgParser* parser);
void pyro_run_repl(ArgParser* parser);

void pyro_cmd_test(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_time(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_check(char* cmd_name, ArgParser* cmd_parser);

void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_command_line_import_roots(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_import_roots_from_path(PyroVM* vm, const char* path);

#endif
