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

To run the test suite run:

    make check

See the [documentation][1] for details.
