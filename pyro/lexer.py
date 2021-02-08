import sys


keywords = {
    "and",
    "break",
    "class",
    "continue",
    "def",
    "echo",
    "else",
    "false",
    "for",
    "if",
    "null",
    "or",
    "return",
    "self",
    "super",
    "true",
    "var",
    "while",
}


def is_digit(char):
    return ord(char) >= ord('0') and ord(char) <= ord('9')


def is_alpha(char):
    if ord(char) >= ord('a') and ord(char) <= ord('z'):
        return True
    if ord(char) >= ord('A') and ord(char) <= ord('Z'):
        return True
    if char == '_' or char == '$':
        return True
    return False


def is_alphanumeric(char):
    return is_digit(char) or is_alpha(char)


class Token:

    def __init__(self, token_type, token_text, line_number):
        self.type = token_type
        self.text = token_text
        self.line = line_number

    def __str__(self):
        return f'{self.line} :: {self.type} :: {self.text}'


class Lexer:

    def __init__(self, source_text):
        self.source_text = source_text
        self.tokens = []
        self.start_index = 0
        self.current_index = 0
        self.line_number = 1

    def tokenize(self):
        while not self.is_at_end():
            self.start_index = self.current_index
            self.read_next_token()
        self.tokens.append(Token("EOF", "", self.line_number))
        return self.tokens

    def is_at_end(self):
        return self.current_index >= len(self.source_text)

    def add_token(self, token_type):
        token_text = self.source_text[self.start_index:self.current_index]
        self.tokens.append(Token(token_type, token_text, self.line_number))

    def read_next_token(self):
        char = self.advance()

        # Single character tokens.
        if char == "(":
            self.add_token("LEFT_PAREN")
        elif char == ")":
            self.add_token("RIGHT_PAREN")
        elif char == "{":
            self.add_token("LEFT_BRACE")
        elif char == "}":
            self.add_token("RIGHT_BRACE")
        elif char == ",":
            self.add_token("COMMA")
        elif char == ".":
            self.add_token("DOT")
        elif char == ";":
            self.add_token("SEMICOLON")
        elif char == "*":
            self.add_token("STAR")
        elif char == "/":
            self.add_token("SLASH")
        elif char == "?":
            self.add_token("QUESTION")
        elif char == ":":
            self.add_token("COLON")

        # Single or double character tokens.
        elif char == "+":
            self.add_token("PLUS_EQUAL" if self.match("=") else "PLUS")
        elif char == "-":
            self.add_token("MINUS_EQUAL" if self.match("=") else "MINUS")
        elif char == "!":
            self.add_token("BANG_EQUAL" if self.match("=") else "BANG")
        elif char == "=":
            self.add_token("EQUAL_EQUAL" if self.match("=") else "EQUAL")
        elif char == "<":
            self.add_token("LESS_EQUAL" if self.match("=") else "LESS")
        elif char == ">":
            self.add_token("GREATER_EQUAL" if self.match("=") else "GREATER")

        # Discard comments.
        elif char == "#":
            while self.peek() != "\n" and not self.is_at_end():
                self.advance()

        # Discard whitespace.
        elif char == " " or char == "\r" or char == "\t" or char == "\n":
            pass

        # Strings.
        elif char == '"':
            self.read_string()

        # Numbers.
        elif is_digit(char):
            self.read_number()

        # Identifiers & keywords.
        elif is_alpha(char):
            self.read_identifier()

        else:
            sys.exit(f"Syntax Error: Line {self.line_number}. Unexpected character '{char}'.")

    def advance(self):
        next_char = self.source_text[self.current_index]
        if next_char == "\n":
            self.line_number += 1
        self.current_index += 1
        return next_char

    def match(self, char):
        if self.peek() == char:
            self.advance()
            return True
        return False

    def peek(self):
        if self.is_at_end():
            return None
        return self.source_text[self.current_index]

    def peek_next(self):
        if self.current_index + 1 >= len(self.source_text):
            return None
        return self.source_text[self.current_index + 1]

    def read_string(self):
        start_line = self.line_number
        while True:
            if self.is_at_end():
                sys.exit(f"Syntax Error: Unterminated string, opened in line {start_line}.")
            if self.advance() == '"':
                break
        self.add_token("STRING")

    def read_number(self):
        while is_digit(self.peek()):
            self.advance()
        if self.peek() == '.' and is_digit(self.peek_next()):
            self.advance()
            while is_digit(self.peek()):
                self.advance()
        self.add_token("NUMBER")

    def read_identifier(self):
        while is_alphanumeric(self.peek()):
            self.advance()
        lexeme = self.source_text[self.start_index:self.current_index]
        if lexeme in keywords:
            self.add_token(lexeme.upper())
        else:
            self.add_token("IDENTIFIER")
