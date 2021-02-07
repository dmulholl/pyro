import sys

from . import runtime
from . import lexer
from . import parser
from . import argslib


__version__ = "0.1.0"


def cmd_debug_tokens(cmd_name, cmd_parser):
    if cmd_parser.args:
        source = open(cmd_parser.args[0]).read()
    else:
        source = sys.stdin.read()
    for token in lexer.Lexer(source).tokenize():
        print(token)


def cmd_debug_expr(cmd_name, cmd_parser):
    if cmd_parser.args:
        source = open(cmd_parser.args[0]).read()
    else:
        source = sys.stdin.read()
    tokens = lexer.Lexer(source).tokenize()
    expression = parser.Parser(tokens).expression()
    print(expression)


def run(argparser):
    if argparser.args:
        source = open(argparser.args[0]).read()
    else:
        source = sys.stdin.read()
    tokens = lexer.Lexer(source).tokenize()
    statements = parser.Parser(tokens).parse()
    runtime.Interpreter().run(statements)


def main():
    argparser = argslib.ArgParser()
    argparser.command("debug_tokens", callback=cmd_debug_tokens)
    argparser.command("debug_expr", callback=cmd_debug_expr)
    argparser.parse()
    if not argparser.command_name:
        run(argparser)

