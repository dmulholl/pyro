#include "cli.h"


static const char* HELPTEXT =
    "Usage: pyro [file]\n"
    "\n"
    "  The Pyro programming language.\n"
    "\n"
    "Arguments:\n"
    "  [file]                     Script to run. Opens the REPL if omitted.\n"
    "\n"
    "Options:\n"
    "  -i, --import-root <dir>    Add a directory to the list of import roots.\n"
    "  -m, --max-memory <int>     Maximum allowed memory allocation in bytes.\n"
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
    "  help <command>             Print the specified command's help text.\n"
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
    "  [files]              Files to test.\n"
    "\n"
    "Flags:\n"
    "  -h, --help           Print this help text and exit.\n"
    "  -v, --verbose        Show error output.\n"
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
    "  By default Pyro runs each timing function 100 times, then prints the\n"
    "  mean execution time. The number of iterations can be customized using\n"
    "  the -n/--num-runs option.\n"
    "\n"
    "Arguments:\n"
    "  [files]                    Files to test.\n"
    "\n"
    "Options:\n"
    "  -n, --num-runs <int>       Number of times to run each function.\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit.\n"
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
    "  -h, --help           Print this help text and exit.\n"
    ;


int main(int argc, char* argv[]) {
    ArgParser* parser = ap_new();
    ap_helptext(parser, HELPTEXT);
    ap_version(parser, PYRO_VERSION_STRING);
    ap_str_opt(parser, "max-memory m", NULL);
    ap_str_opt(parser, "import-root i", NULL);

    ArgParser* test_cmd = ap_cmd(parser, "test");
    ap_helptext(test_cmd, TEST_HELPTEXT);
    ap_callback(test_cmd, pyro_cmd_test);
    ap_flag(test_cmd, "verbose v");

    ArgParser* time_cmd = ap_cmd(parser, "time");
    ap_helptext(time_cmd, TIME_HELPTEXT);
    ap_callback(time_cmd, pyro_cmd_time);
    ap_int_opt(time_cmd, "num-runs n", 100);

    ArgParser* check_cmd = ap_cmd(parser, "check");
    ap_helptext(check_cmd, CHECK_HELPTEXT);
    ap_callback(check_cmd, pyro_cmd_check);

    ap_parse(parser, argc, argv);

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
