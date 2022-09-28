# Pyro

[1]: http://www.dmulholl.com/docs/pyro/main/

Pyro is a dynamically-typed, garbage-collected scripting language implemented in C.

```ruby
def fib(n) {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
```

See the [documentation][1] for details.


## Building

To build the Pyro binary run:

    make

This will compile the Pyro binary in a new `out` directory as `out/release/pyro`.
To install this binary as `/usr/local/bin/pyro` run:

    make install


## Testing

To build the release binary and run the test suite run:

    make check-release

To build the debug binary and run the test suite run:

   make check-debug

The debug binary is (much) slower than the release binary but includes extra checks useful in a development environment.
