import time
from . import runtime


class Clock:

    def call(self, args):
        return time.perf_counter()

    def arity(self):
        return 0

    def __str__(self):
        return "<PyroBuiltin $clock>"


class Print:

    def call(self, args):
        print(runtime.stringify(args[0]), end="")

    def arity(self):
        return 1

    def __str__(self):
        return "<PyroBuiltin $print>"


class PrintLine:

    def call(self, args):
        print(runtime.stringify(args[0]))

    def arity(self):
        return 1

    def __str__(self):
        return "<PyroBuiltin $println>"


class Stringify:

    def call(self, args):
        return runtime.stringify(args[0])

    def arity(self):
        return 1

    def __str__(self):
        return "<PyroBuiltin $str>"

