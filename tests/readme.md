# Test Suite

To build a new release binary and run the test suite run:

    make check-release

To build a new debug binary and run the test suite run:

    make check-debug

To build a new debug binary with sanitizer checks and run the test suite run:

    make check-debug-sanitized

The debug binary is (much) slower than the release binary but includes extra checks useful during development.

Alternatively, you can run the test suite using the `test` command directly:

    pyro test tests/*.pyro
