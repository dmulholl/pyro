#!/usr/bin/env python3

import time

num_runs = 10

# ------------------------------------------------------------------------------

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


class Foo:
    def __init__(self, value):
        self.value = value * 257 + 123

    def change_value(self, arg):
        self.value = self.value * arg

    def get_value(self):
        return self.value


def do_arithmetic(a, b, c, d):
    foo = (((a * 137 + b * 17 + c * 23) / 31) * a) / (b + 1) + 2 * d
    bar = ((foo * a) / (b + 1)) * c + 3 * d
    baz = foo + bar + a + b + c + d
    bam = foo * 2 + bar * 3 + baz * 4
    return bam


def make_adder(n):
    def adds_n(arg):
        return arg + n
    return adds_n


def fizzbuzz(n):
    if n % 15 == 0:
        return "fizzbuzz"
    elif n % 3 == 0:
        return "fizz"
    elif n % 5 == 0:
        return "buzz"
    else:
        return n


def benchmark():
    vec = list()
    map = dict()

    # Recursive function calls.
    fib_result = fib(25)
    vec.append(fib_result)
    map["fib"] = fib_result

    # Arithmetic.
    for i in range(1000):
        result = (i * 109153 + 257) / 253 - 751 + 4059.123
        vec.append(result)
        map[i] = result

    # Arithmetic in function.
    for i in range(10):
        for j in range(10):
            for k in range(10):
                result = do_arithmetic(i, j, k, i + j + k)
                vec.append(result)
                map[f"{i}-{j}-{k}"] = result

    # Classes.
    for i in range(1000):
        foo = Foo(123)
        foo.change_value(459)
        vec.append(foo.get_value())
        map[str(i)] = foo.get_value()

    # Strings.
    string = ""
    for i in range(1000):
        string += str(i)
        vec.append(string)
        map[i] = string

    # Closure.
    for i in range(1000):
        func = make_adder(i)
        vec.append(func)
        vec.append(func(i))
        map[i] = func(i)

    # FizzBuzz.
    for i in range(1000):
        vec.append(fizzbuzz(i))
        map[i] = fizzbuzz(i)

# ------------------------------------------------------------------------------

def main():
    start = time.process_time()
    for i in range(num_runs):
        benchmark()
    runtime = time.process_time() - start
    print(f"Average time: {runtime/num_runs:0.6f} secs")

if __name__ == "__main__":
    main()
