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

To build the release binary run:

    make release

The binary will be created in a new `out` directory as `out/release/pyro`.

See the [documentation][1] for details.
