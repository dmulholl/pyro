#include "cli.h"


static const char* HELPTEXT =
    "Pyro %s\n"
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
    "  -i, --import-root <dir>    Adds a directory to the list of import roots.\n"
    "                             (This option can be specified multiple times.)\n"
    "  -m, --max-memory <int>     Sets the maximum memory allocation in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "  -s, --stack-size <int>     Sets the stack size in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit.\n"
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
    "  This command runs unit tests. Each input file is executed in a new VM\n"
    "  instance.\n"
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
    "  -m, --max-memory <int>     Sets the maximum memory allocation in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "  -s, --stack-size <int>     Sets the stack size in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit.\n"
    "  -v, --verbose              Show error output."
    ;


static const char* TIME_HELPTEXT =
    "Usage: pyro time [files]\n"
    "\n"
    "  This command runs timing functions. Each input file is executed in a\n"
    "  new VM instance.\n"
    "\n"
    "  For each input file, Pyro first executes the file, then runs any timing\n"
    "  functions it contains, i.e. functions whose names begin with '$time_'.\n"
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
    "  -m, --max-memory <int>     Sets the maximum memory allocation in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "  -n, --num-runs <int>       Number of times to run each function.\n"
    "  -s, --stack-size <int>     Sets the stack size in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit."
    ;


static const char* CHECK_HELPTEXT =
    "Usage: pyro check [files]\n"
    "\n"
    "  Attempts to compile but not execute the specified files. Can be used to\n"
    "  check files for syntax errors.\n"
    "\n"
    "Arguments:\n"
    "  [files]              Files to compile.\n"
    "\n"
    "Flags:\n"
    "  -h, --help           Print this help text and exit."
    ;


int main(int argc, char* argv[]) {
    // Initialize the root argument parser.
    ArgParser* parser = ap_new();
    if (!parser) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    char* version_string = pyro_get_version_string();
    if (!version_string) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    char* helptext_string = pyro_cli_sprintf(HELPTEXT, version_string);
    if (!helptext_string) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    ap_set_version(parser, version_string);
    ap_set_helptext(parser, helptext_string);
    free(version_string);
    free(helptext_string);

    ap_str_opt(parser, "max-memory m", NULL);
    ap_str_opt(parser, "stack-size s", NULL);
    ap_str_opt(parser, "import-root i", NULL);
    ap_first_pos_arg_ends_options(parser, true);

    // Register the parser for the 'test' comand.
    ArgParser* test_cmd_parser = ap_cmd(parser, "test");
    if (!test_cmd_parser) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    ap_set_helptext(test_cmd_parser, TEST_HELPTEXT);
    ap_callback(test_cmd_parser, pyro_cmd_test);
    ap_flag(test_cmd_parser, "verbose v");
    ap_str_opt(test_cmd_parser, "max-memory m", NULL);
    ap_str_opt(test_cmd_parser, "stack-size s", NULL);
    ap_str_opt(parser, "import-root i", NULL);
    ap_str_opt(test_cmd_parser, "import-root i", NULL);

    // Register the parser for the 'time' comand.
    ArgParser* time_cmd_parser = ap_cmd(parser, "time");
    if (!time_cmd_parser) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    ap_set_helptext(time_cmd_parser, TIME_HELPTEXT);
    ap_callback(time_cmd_parser, pyro_cmd_time);
    ap_str_opt(time_cmd_parser, "max-memory m", NULL);
    ap_str_opt(time_cmd_parser, "stack-size s", NULL);
    ap_str_opt(time_cmd_parser, "import-root i", NULL);
    ap_int_opt(time_cmd_parser, "num-runs n", 10);

    // Register the parser for the 'check' comand.
    ArgParser* check_cmd_parser = ap_cmd(parser, "check");
    if (!check_cmd_parser) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(1);
    }

    ap_set_helptext(check_cmd_parser, CHECK_HELPTEXT);
    ap_callback(check_cmd_parser, pyro_cmd_check);
    ap_str_opt(check_cmd_parser, "max-memory m", NULL);
    ap_str_opt(check_cmd_parser, "stack-size s", NULL);

    // Parse the command line arguments.
    if (!ap_parse(parser, argc, argv)) {
        fprintf(stderr, "Error: Out of memory.\n");
        exit(2);
    }

    if (!ap_has_cmd(parser)) {
        if (ap_count_args(parser) > 0) {
            pyro_run_file(parser);
        } else {
            pyro_run_repl(parser);
        }
    }

    ap_free(parser);
    return 0;
}