#include "cli.h"


static const char* HELPTEXT =
    "%s\n"
    "\n"
    "  The Pyro programming language.\n"
    "\n"
    "Usage:\n"
    "  pyro [file]\n"
    "  pyro <command>\n"
    "  pyro help <command>\n"
    "\n"
    "Arguments:\n"
    "  [file]                     Script to run. Opens the REPL if omitted.\n"
    "\n"
    "Options:\n"
    "  -e, --exec <str>           Execute a string of Pyro code.\n"
    "  -i, --import-root <dir>    Add a directory to the list of import roots.\n"
    "                             (This option can be specified multiple times.)\n"
    "  -m, --max-memory <int>     Set the maximum memory allocation in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "  -s, --stack-size <int>     Set the stack size in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit.\n"
    "  -t, --trace-execution      Trace bytecode execution. (Debug builds only.)\n"
    "  -v, --version              Print the version number and exit.\n"
    "\n"
    "Commands:\n"
    "  check                      Compile files without executing.\n"
    "  test                       Run unit tests.\n"
    "  time                       Run timing functions.\n"
    "\n"
    "Command Help:\n"
    "  help <command>             Print the specified command's help text."
    ;


static const char* TEST_HELPTEXT =
    "Usage: pyro test [files]\n"
    "\n"
    "  This command runs unit tests.\n"
    "\n"
    "  For each input file, Pyro first executes the file, then runs any test\n"
    "  functions it contains, i.e. functions whose names begin with '$test_'.\n"
    "  A test function passes if it executes without panicking.\n"
    "\n"
    "  You can use an 'assert' statement to make a test function panic if the\n"
    "  test fails, e.g.\n"
    "\n"
    "      def $test_addition() {\n"
    "          assert 1 + 2 == 3;\n"
    "      }\n"
    "\n"
    "  Note that test functions take no arguments.\n"
    "\n"
    "Arguments:\n"
    "  [files]                    Files to test.\n"
    "\n"
    "Options:\n"
    "  -i, --import-root <dir>    Adds a directory to the list of import roots.\n"
    "\n"
    "Flags:\n"
    "  -e, --errors               Show standard error output.\n"
    "  -h, --help                 Print this help text and exit."
    ;


static const char* TIME_HELPTEXT =
    "Usage: pyro time [files]\n"
    "\n"
    "  This command runs timing functions.\n"
    "\n"
    "  For each input file, Pyro first executes the file, then runs any timing\n"
    "  functions it contains, i.e. functions whose name begins with '$time_'.\n"
    "\n"
    "  By default Pyro runs each timing function 10 times, then prints the\n"
    "  mean execution time. The number of iterations can be customized using\n"
    "  the -n/--num-runs option.\n"
    "\n"
    "Arguments:\n"
    "  [files]                    Files to test.\n"
    "\n"
    "Options:\n"
    "  -i, --import-root <dir>    Adds a directory to the list of import roots.\n"
    "  -n, --num-runs <int>       Number of times to run each function.\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit."
    ;


static const char* CHECK_HELPTEXT =
    "Usage: pyro check [files]\n"
    "\n"
    "  Attempts to compile but not execute the specified files. Exits with a\n"
    "  non-zero status code if an input file contains syntax errors.\n"
    "\n"
    "Arguments:\n"
    "  [files]              Input files.\n"
    "\n"
    "Flags:\n"
    "  -d, --disassemble    Print the disassembled bytecode for each input file.\n"
    "  -h, --help           Print this help text and exit."
    ;


static const char* GET_HELPTEXT =
    "Usage: pyro get <module>\n"
    "\n"
    "  This command is a stub -- it hasn't been implemented yet.\n"
    "\n"
    "Arguments:\n"
    "  <module>             Module to download.\n"
    "\n"
    "Flags:\n"
    "  -h, --help           Print this help text and exit."
    ;


int main(int argc, char* argv[]) {
    // Initialize the root argument parser.
    ArgParser* parser = ap_new_parser();
    if (!parser) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    char* version_string = pyro_get_version_string();
    if (!version_string) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    char* helptext_string = pyro_cli_sprintf(HELPTEXT, version_string);
    if (!helptext_string) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_version(parser, version_string);
    ap_set_helptext(parser, helptext_string);
    free(version_string);
    free(helptext_string);

    ap_add_str_opt(parser, "exec e", NULL);
    ap_add_str_opt(parser, "max-memory m", NULL);
    ap_add_str_opt(parser, "stack-size s", NULL);
    ap_add_str_opt(parser, "import-root i", NULL);
    ap_add_flag(parser, "trace-execution t");
    ap_first_pos_arg_ends_options(parser, true);

    // Register the parser for the 'test' comand.
    ArgParser* cmd_test = ap_new_cmd(parser, "test");
    if (!cmd_test) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_test, TEST_HELPTEXT);
    ap_set_cmd_callback(cmd_test, pyro_cli_cmd_test);
    ap_add_str_opt(cmd_test, "import-root i", NULL);
    ap_add_flag(cmd_test, "errors e");

    // Register the parser for the 'time' comand.
    ArgParser* cmd_time = ap_new_cmd(parser, "time");
    if (!cmd_time) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_time, TIME_HELPTEXT);
    ap_set_cmd_callback(cmd_time, pyro_cli_cmd_time);
    ap_add_str_opt(cmd_time, "import-root i", NULL);
    ap_add_int_opt(cmd_time, "num-runs n", 10);

    // Register the parser for the 'check' comand.
    ArgParser* cmd_check = ap_new_cmd(parser, "check");
    if (!cmd_check) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_check, CHECK_HELPTEXT);
    ap_set_cmd_callback(cmd_check, pyro_cli_cmd_check);
    ap_add_flag(cmd_check, "disassemble d");

    // Register the parser for the 'get' command.
    ArgParser* cmd_get = ap_new_cmd(parser, "get");
    if (!cmd_get) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_get, GET_HELPTEXT);
    ap_set_cmd_callback(cmd_get, pyro_cli_cmd_get);

    // Parse the command line arguments.
    if (!ap_parse(parser, argc, argv)) {
        ap_free(parser);
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    if (ap_found_cmd(parser)) {
        ap_free(parser);
        return 0;
    }

    if (ap_found(parser, "exec")) {
        pyro_cli_run_exec(parser);
    } else if (ap_count_args(parser) > 0) {
        pyro_cli_run_file(parser);
    } else {
        pyro_cli_run_repl(parser);
    }

    ap_free(parser);
    return 0;
}
