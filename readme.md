# Pyro

Pyro is a dynamically-typed, garbage-collected scripting language implemented in C.

(This is an experimental project I'm working on for my own edutainment - it's at an early stage and isn't suitable yet for any kind of real world use.)

To build the binary run:

    $ make release

The binary will be created as `out/pyro`. Only POSIX builds are currently supported (Mac, Linux, BSD, etc.).

Run a script by supplying its filename to the interpreter:

    $ pyro script.pyro

Use the `test` command to run the test suite:

    $ pyro test tests/*.pyro

