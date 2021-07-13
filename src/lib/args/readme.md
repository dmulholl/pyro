# Args

[1]: https://github.com/dmulholl/args

This is a customized version of the [Args][1] library (v2.1.4) for parsing command line arguments. It's used by the command line interface rather than by the language implementation itself.

This version has been customized so the first non-positional argument turns off option parsing, i.e. when parsing the command:

    $ pyro -a -b script.pyro -c -d

we want `-a` and `-b` to be parsed as options for the Pyro binary but we want to pass `-c` and `-d` on as options for the script file.

This change only affects the root parser &mdash; arguments to commands are parsed as before.
