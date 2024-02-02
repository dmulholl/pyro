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
    "      --max-memory <int>     Set the maximum memory allocation in bytes.\n"
    "                             (Append 'K' for KB, 'M' for MB, 'G' for GB.)\n"
    "  -m, --module <module>      Run an imported module as a script.\n"
    "\n"
    "Flags:\n"
    "  -h, --help                 Print this help text and exit.\n"
    "  -t, --trace-execution      Trace bytecode execution. (Debug builds only.)\n"
    "  -v, --version              Print the version number and exit.\n"
    "\n"
    "Commands:\n"
    "  bake                       Compile a baked application binary.\n"
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
    "  This command runs unit tests. It exits with a non-zero status code if\n"
    "  any of the tests fail.\n"
    "\n"
    "  For each input file, Pyro first executes the file, then runs any test\n"
    "  functions it contains, i.e. functions whose names begin with '$test_'.\n"
    "\n"
    "    - A test file passes if it executes without panicking.\n"
    "    - A test function passes if it executes without panicking.\n"
    "\n"
    "  You can use an 'assert' statement to make a test file or function panic\n"
    "  if a test fails, e.g.\n"
    "\n"
    "      def $test_addition() {\n"
    "          assert 1 + 2 == 3;\n"
    "      }\n"
    "\n"
    "  Note that test functions take no arguments.\n"
    "\n"
    "  Use the -v/--verbose flag to show error messages.\n"
    "\n"
    "  Use the -d/--debug flag to isolate individual test failures. With this\n"
    "  flag, only error output will be shown. Execution will halt at the first\n"
    "  panic and the full stack trace will be printed.\n"
    "\n"
    "Arguments:\n"
    "  [files]                    Files to test.\n"
    "\n"
    "Options:\n"
    "  -i, --import-root <dir>    Adds a directory to the list of import roots.\n"
    "                             (This option can be specified multiple times.)\n"
    "\n"
    "Flags:\n"
    "  -d, --debug                Halts execution at the first panic.\n"
    "                             Prints the full stack trace for the panic.\n"
    "  -h, --help                 Print this help text and exit.\n"
    "  -n, --no-color             Plain text output, no terminal colors.\n"
    "  -v, --verbose              Show error messages."
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


static const char* BAKE_HELPTEXT =
    "Usage: pyro bake <file>\n"
    "\n"
    "  This command compiles a Pyro script and (optionally) a collection of\n"
    "  modules into a baked application binary. Modules required by the script\n"
    "  should be placed in a 'modules' directory alongside the script file.\n"
    "\n"
    "  Requires make, git, and a C compiler.\n"
    "\n"
    "  If you don't specify a custom Pyro source code repository, the default\n"
    "  repository will be used, i.e. \"https://github.com/dmulholl/pyro.git\".\n"
    "\n"
    "  If no Pyro version is specified, the master branch will be used.\n"
    "\n"
    "  Note that you can only bake modules written in Pyro into a baked\n"
    "  application binary. Compiled extension modules written in C are not\n"
    "  supported.\n"
    "\n"
    "Examples:\n"
    "  pyro bake ./script.pyro\n"
    "  pyro bake ./script.pyro --output ./appname --pyro-version v0.12.3\n"
    "\n"
    "Arguments:\n"
    "  <file>                       Pyro script.\n"
    "\n"
    "Options:\n"
    "  -o, --output <file>          Output file.\n"
    "  -r, --pyro-repo <url>        Git repository containing Pyro source code.\n"
    "  -v, --pyro-version <tag>     Git tag specifying the Pyro version to use.\n"
    "\n"
    "Flags:\n"
    "  -h, --help                   Print this help text and exit.\n"
    "      --no-cache               Don't use cached Pyro source code."
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
    ap_add_str_opt(parser, "max-memory", NULL);
    ap_add_str_opt(parser, "import-root i", NULL);
    ap_add_greedy_str_opt(parser, "module m");
    ap_add_flag(parser, "trace-execution t");
    ap_first_pos_arg_ends_option_parsing(parser);

    // Register the parser for the 'test' comand.
    ArgParser* cmd_test = ap_new_cmd(parser, "test");
    if (!cmd_test) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_test, TEST_HELPTEXT);
    ap_set_cmd_callback(cmd_test, pyro_cli_cmd_test);
    ap_add_str_opt(cmd_test, "import-root i", NULL);
    ap_add_flag(cmd_test, "debug d");
    ap_add_flag(cmd_test, "verbose v");
    ap_add_flag(cmd_test, "no-color no-colour n");

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

    // Register the parser for the 'bake' command.
    ArgParser* cmd_bake = ap_new_cmd(parser, "bake");
    if (!cmd_bake) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    ap_set_helptext(cmd_bake, BAKE_HELPTEXT);
    ap_set_cmd_callback(cmd_bake, pyro_cli_cmd_bake);
    ap_add_str_opt(cmd_bake, "pyro-version v", "master");
    ap_add_str_opt(cmd_bake, "pyro-repo r", "https://github.com/dmulholl/pyro.git");
    ap_add_str_opt(cmd_bake, "output o", "");
    ap_add_flag(cmd_bake, "no-cache");

    // Parse the command line arguments.
    if (!ap_parse(parser, argc, argv)) {
        ap_free(parser);
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    if (ap_found_cmd(parser)) {
        int exit_code = ap_get_cmd_exit_code(parser);
        ap_free(parser);
        return exit_code;
    }

    if (ap_found(parser, "exec")) {
        pyro_cli_run_exec(parser);
        ap_free(parser);
        return 0;
    }

    if (ap_found(parser, "module")) {
        pyro_cli_run_module(parser);
        ap_free(parser);
        return 0;
    }

    if (ap_count_args(parser) > 0) {
        pyro_cli_run_path(parser);
        ap_free(parser);
        return 0;
    }

    pyro_cli_run_repl(parser);
    ap_free(parser);
    return 0;
}
