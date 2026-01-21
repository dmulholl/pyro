# Pyro

[1]: https://www.dmulholl.com/docs/pyro/master/
[2]: https://www.dmulholl.com/docs/pyro/master/quickstart.html
[3]: https://www.dmulholl.com/docs/pyro/master/tour.html


Pyro is a dynamically-typed, garbage-collected scripting language implemented in C.
Think Python if Guido had been a fan of lexical-scoping, curly braces, and semicolons.

~~~ruby
def fib(n) {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
~~~

Pyro is a classic braces-and-semicolons language. If you've used any C-family language before you should find it instantly intuitive.



## Getting Started

* Take Pyro for a test drive with the [quickstart tutorial][2].
* Learn more about the language by taking the [tour][3].

Pyro is an experimental language that's still under development &mdash; you should only write
the control code for your pacemaker/anti-ballistic missile defence system in it if you're feeling particularly adventurous.



## Design Goals

* *Legibility* &mdash; Pyro code should be easy to read and understand.
* *Simplicity* &mdash; Common problems should have simple solutions.
* *Elegance* &mdash; A well-crafted tool should be a pleasure to use.



## Internals

* Pyro compiles source code into bytecode which it executes using a stack-based VM.
* Pyro manages memory using a simple mark-and-sweep garbage collector.



## License

Zero-Clause BSD (0BSD).
