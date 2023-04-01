#!/usr/bin/env python3

import time
import random

num_runs = 100

# TESTS ------------------------------------------------------------------------

global_variable = 0

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

class Object:
    def __init__(self, value):
        self.value = value * 456 + 789

    def change_value(self, arg):
        self.value += arg

    def get_value(self):
        return self.value

    def unused_method_1(self):
        return self.value + 1

    def unused_method_2(self):
        return self.value + 1

    def unused_method_3(self):
        return self.value + 1

    def unused_method_4(self):
        return self.value + 1

    def unused_method_5(self):
        return self.value + 1

    def unused_method_6(self):
        return self.value + 1

    def unused_method_7(self):
        return self.value + 1

    def unused_method_8(self):
        return self.value + 1

    def unused_method_9(self):
        return self.value + 1

class NoConstructorObject:
    pass

def do_arithmetic(a, b, c, d):
    foo = (((a * 123 + b * 123.456 + c * 23) / 31) * a) / (b + 1) + 2.0 * d
    bar = ((foo * a) / (b + 1)) * c + 3 * d
    baz = foo + bar + a + b + c + d
    bam = foo * 2 + bar * 3 + baz * 4
    return bam

def do_string_stuff(str_a, str_b, str_sep):
    string = ""
    vec = []
    local = "xyz"

    for i in range(10):
        string += ("foo" + str_a + str_b + str_sep + "bar" + local)
        for element in string.split(str_sep):
            vec.append(element)

    return vec

def make_adder_closure(n):
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

def run_benchmark():
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

    # Initialize object in loop (no constructor).
    for i in range(1000):
        obj = NoConstructorObject()
        obj.value = i

    # Initialize object in loop (with constructor).
    for i in range(1000):
        obj = Object(123 + i)

    # Modify field in loop.
    obj1 = Object(456)
    for i in range(1000):
        obj1.value += i

    # Call single method in loop.
    obj2 = Object(789)
    for i in range(1000):
        obj2.change_value(i)

    # Mixed field/method operations in loop.
    for i in range(1000):
        obj = Object(999 + i)
        obj.value += i
        obj.change_value(i)
        vec.append(obj.get_value())
        map[str(i)] = obj.get_value()

    # Strings.
    string = ""
    for i in range(1000):
        string += str(i)
        vec.append(string)
        map[i] = string

    # Closures.
    for i in range(1000):
        func = make_adder_closure(i)
        vec.append(func)
        vec.append(func(i))
        map[i] = func(i)

    # FizzBuzz.
    for i in range(1000):
        vec.append(fizzbuzz(i))
        map[i] = fizzbuzz(i)

    # Vector getting and setting.
    for i in range(1000):
        vec[i] = vec[i + 1]

    # Map getting and setting.
    for i in range(1000):
        map[i] = map[i]

    # Update global.
    global global_variable
    for i in range(1000):
        global_variable += 1

    # Update local.
    local_variable = 0
    for i in range(1000):
        local_variable += 1

    # Type conversions.
    for i in range(1000):
        vec[i] = int(str(i))

    # Sorting.
    sort_vec = list()
    for i in range(1000):
        sort_vec.append(i)
    random.shuffle(sort_vec)
    sort_vec.sort()

    # Iterating over a map.
    new_map = {}
    for key, value in map.items():
        new_map[key] = value

    # Do string stuff.
    for i in range(10):
        for j in range(10):
            result = do_string_stuff(str(i), str(j), ":")
            vec.append(result)

# END TESTS --------------------------------------------------------------------

def main():
    start = time.process_time()
    for i in range(num_runs):
        run_benchmark()
    runtime_s = time.process_time() - start
    average_s = runtime_s / num_runs
    average_ms = average_s * 1000
    print(f"Average time: {average_s:0.6f} s :: {average_ms:0.3f} ms")

if __name__ == "__main__":
    main()
