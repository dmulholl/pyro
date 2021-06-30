# Pyro

Pyro is a dynamically-typed, garbage-collected scripting language implemented in C.
(This is an experimental project I'm working on for my own edutainment - it's at an early stage and isn't suitable yet for any kind of real world use.)

Only POSIX builds are currently supported (Mac, Linux, BSD, etc.). To build the interpreter run:

    $ make release

To run a script, supply its filename to the interpreter:

    $ pyro script.pyro

The `test` command can be used to run the test suite:

    $ pyro test tests/*.pyro
