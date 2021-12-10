#ifndef pyro_cli_h
#define pyro_cli_h

// C standard library headers.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

// Local headers.
#include "../vm/setup.h"
#include "../vm/exec.h"

#include "../lib/args/args.h"
#include "../lib/os/os.h"

void pyro_run_file(ArgParser* parser);
void pyro_run_repl(ArgParser* parser);

void pyro_cmd_test(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_time(char* cmd_name, ArgParser* cmd_parser);
void pyro_cmd_check(char* cmd_name, ArgParser* cmd_parser);

void pyro_cli_set_max_memory(PyroVM* vm, ArgParser* parser);
void pyro_cli_add_import_roots(PyroVM* vm, ArgParser* parser);

#endif
