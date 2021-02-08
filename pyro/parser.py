import sys
from .expressions import *
from .statements import *


# Recursive descent parser.
class Parser:

    def __init__(self, token_list):
        self.tokens = token_list
        self.current = 0

    def parse(self):
        statements = []
        while not self.is_at_end():
            statements.append(self.declaration())
        return statements

    # ----------------------------------------------------------------------
    # Statement parsers.
    # ----------------------------------------------------------------------

    def declaration(self):
        if self.match("VAR"):
            return self.variable_declaration()
        if self.match("DEF"):
            return self.function_declaration("function")
        if self.match("CLASS"):
            return self.class_declaration()
        self.error("Expected a declaration.")

    def variable_declaration(self):
        name_token = self.consume("IDENTIFIER", "Expected a variable name after 'var'.")
        initializer_expr = None
        if self.match("EQUAL"):
            initializer_expr = self.expression()
        self.consume("SEMICOLON", "Expected ';' after a variable declaration.")
        return VarDecStmt(name_token, initializer_expr)

    def function_declaration(self, kind="function"):
        name_token = self.consume("IDENTIFIER", f"Expected {kind} name.")
        self.consume("LEFT_PAREN", f"Expected '(' after {kind} name.")
        parameters = []
        if not self.check("RIGHT_PAREN"):
            while True:
                parameters.append(self.consume("IDENTIFIER", "Expected parameter name."))
                if not self.match("COMMA"):
                    break
        self.consume("RIGHT_PAREN", "Expected ')' after parameters.")
        self.consume("LEFT_BRACE", f"Expected '{{' before {kind} body.")
        body = self.block()
        return FunctionStmt(name_token, parameters, body)

    def class_declaration(self):
        name_token = self.consume("IDENTIFIER", "Expected class name.")
        superclass_expr = None
        if self.match("LESS"):
            self.consume("IDENTIFIER", "Expected superclass name.")
            superclass_expr = VariableExpr(self.previous())
        self.consume("LEFT_BRACE", "Expected '{' before class body.")
        methods = []
        fields = []
        while not self.check("RIGHT_BRACE") and not self.is_at_end():
            if self.match("VAR"):
                fields.append(self.variable_declaration())
            elif self.match("DEF"):
                methods.append(self.function_declaration("method"))
            else:
                self.error("Expected a method or variable declaration.")
        self.consume("RIGHT_BRACE", "Expected '}' after class body.")
        return ClassStmt(name_token, superclass_expr, methods, fields)

    def block(self):
        statements = []
        while not self.check("RIGHT_BRACE") and not self.is_at_end():
            statements.append(self.body_statement())
        self.consume("RIGHT_BRACE", "Expected '}' after a block.")
        return BlockStmt(statements)

    def body_statement(self):
        if self.match("LEFT_BRACE"):
            return self.block()
        elif self.match("VAR"):
            return self.variable_declaration()
        elif self.match("ECHO"):
            return self.echo_statement()
        elif self.match("IF"):
            return self.if_statement()
        elif self.match("WHILE"):
            return self.while_statement()
        elif self.match("FOR"):
            return self.for_statement()
        elif self.match("BREAK"):
            return self.break_statement()
        elif self.match("CONTINUE"):
            return self.continue_statement()
        elif self.match("RETURN"):
            return self.return_statement()
        return self.expression_statement()

    def echo_statement(self):
        expressions = []
        if not self.check("SEMICOLON"):
            while True:
                expressions.append(self.expression())
                if not self.match("COMMA"):
                    break
        self.consume("SEMICOLON", "Expected ';' after 'echo' statement.")
        return EchoStmt(expressions)

    def if_statement(self):
        condition_expr = self.expression()
        self.consume("LEFT_BRACE", "Expected block after 'if' condition.")
        then_branch = self.block()
        else_branch = None
        if self.match("ELSE"):
            if self.match("IF"):
                else_branch = self.if_statement()
            else:
                self.consume("LEFT_BRACE", "Expected block after 'else'.")
                else_branch = self.block()
        return IfStmt(condition_expr, then_branch, else_branch)

    def while_statement(self):
        condition_expr = self.expression()
        self.consume("LEFT_BRACE", "Expected block after 'while'.")
        body = self.block()
        return WhileStmt(condition_expr, body)

    def for_statement(self):
        if self.match("SEMICOLON"):
            initializer_stmt = None
        elif self.match("VAR"):
            initializer_stmt = self.variable_declaration()
        else:
            initializer_stmt = self.expression_statement()

        if self.check("SEMICOLON"):
            condition_expr = LiteralExpr(None, True)
        else:
            condition_expr = self.expression()
        self.consume("SEMICOLON", "Expected ';' after loop condition.")

        if self.check("LEFT_BRACE"):
            increment_expr = None
        else:
            increment_expr = self.expression()

        self.consume("LEFT_BRACE", "Expected block after 'for'.")
        body = self.block()
        return ForStmt(initializer_stmt, condition_expr, increment_expr, body)

    def break_statement(self):
        self.consume("SEMICOLON", "Expected ';' after 'break'.")
        return BreakStmt()

    def continue_statement(self):
        self.consume("SEMICOLON", "Expected ';' after 'continue'.")
        return ContinueStmt()

    def return_statement(self):
        if self.match("SEMICOLON"):
            return ReturnStmt(None)
        value_expr = self.expression()
        self.consume("SEMICOLON", "Expected ';' after return value.")
        return ReturnStmt(value_expr)

    def expression_statement(self):
        expr = self.expression()
        self.consume("SEMICOLON", "Expected ';' after expression.")
        return ExpressionStmt(expr)

    # ----------------------------------------------------------------------
    # Expression parsers.
    # ----------------------------------------------------------------------

    def expression(self):
        return self.assignment()

    def assignment(self):
        expr = self.conditional()
        if self.match("EQUAL", "PLUS_EQUAL", "MINUS_EQUAL"):
            operator_token = self.previous()
            value_expr = self.assignment()
            if operator_token.type in ("PLUS_EQUAL", "MINUS_EQUAL"):
                value_expr = BinaryExpr(expr, operator_token, value_expr)
            if isinstance(expr, VariableExpr):
                return AssignExpr(expr.token, value_expr)
            elif isinstance(expr, GetAttrExpr):
                return SetAttrExpr(expr.object, expr.name, value_expr)
            else:
                self.error("Invalid assignment target.", operator_token)
        return expr

    def conditional(self):
        expr = self.logical()
        if self.match("QUESTION"):
            true_branch = self.logical()
            self.consume("COLON", "Expected ':' after '?'.")
            false_branch = self.logical()
            return ConditionalExpr(expr, true_branch, false_branch)
        return expr

    def logical(self):
        expr = self.equality()
        while self.match("AND", "OR"):
            operator_token = self.previous()
            right_expr = self.equality()
            expr = LogicalExpr(expr, operator_token, right_expr)
        return expr

    def equality(self):
        expr = self.comparative()
        while self.match("BANG_EQUAL", "EQUAL_EQUAL"):
            operator_token = self.previous()
            right_expr = self.comparative()
            expr = BinaryExpr(expr, operator_token, right_expr)
        return expr

    def comparative(self):
        expr = self.additive()
        while self.match("GREATER", "GREATER_EQUAL", "LESS", "LESS_EQUAL"):
            operator_token = self.previous()
            right = self.additive()
            expr = BinaryExpr(expr, operator_token, right)
        return expr

    def additive(self):
        expr = self.multiplicative()
        while self.match("MINUS", "PLUS"):
            operator_token = self.previous()
            right = self.multiplicative()
            expr = BinaryExpr(expr, operator_token, right)
        return expr

    def multiplicative(self):
        expr = self.unary()
        while self.match("SLASH", "STAR"):
            operator_token = self.previous()
            right = self.unary()
            expr = BinaryExpr(expr, operator_token, right)
        return expr

    def unary(self):
        if self.match("BANG", "MINUS"):
            operator_token = self.previous()
            expr = self.unary()
            return UnaryExpr(operator_token, expr)
        return self.call()

    def call(self):
        expr = self.primary()
        while True:
            if self.match("LEFT_PAREN"):
                expr = self.finish_call(expr)
            elif self.match("DOT"):
                name = self.consume("IDENTIFIER", "Expected property name after '.'.")
                expr = GetAttrExpr(expr, name)
            else:
                break
        return expr

    def finish_call(self, callee_expr):
        left_paren_token = self.previous()
        arguments = []
        if not self.check("RIGHT_PAREN"):
            while True:
                arguments.append(self.expression())
                if not self.match("COMMA"):
                    break
        self.consume("RIGHT_PAREN", "Expected ')' after arguments.")
        return CallExpr(callee_expr, left_paren_token, arguments)

    def primary(self):
        if self.match("SELF"):
            return SelfExpr(self.previous())
        if self.match("SUPER"):
            super_token = self.previous()
            self.consume("DOT", "Expected '.' after 'super'.")
            attr_token = self.consume("IDENTIFIER", "Expected superclass method name.")
            return SuperExpr(super_token, attr_token)
        if self.match("TRUE"):
            return LiteralExpr(self.previous(), True)
        if self.match("FALSE"):
            return LiteralExpr(self.previous(), False)
        if self.match("NULL"):
            return LiteralExpr(self.previous(), None)
        if self.match("NUMBER"):
            literal = self.previous().text
            value = float(literal)
            return LiteralExpr(self.previous(), value)
        if self.match("STRING"):
            literal = self.previous().text
            value = literal[1:-1]
            return LiteralExpr(self.previous(), value)
        if self.match("IDENTIFIER"):
            return VariableExpr(self.previous())
        if self.match("LEFT_PAREN"):
            expr = self.expression()
            self.consume("RIGHT_PAREN", "Expected ')' after expression.")
            return GroupingExpr(expr)
        self.error("Invalid token. Expected an expression.")

    # ----------------------------------------------------------------------
    # Helpers
    # ----------------------------------------------------------------------

    def match(self, *token_types):
        for token_type in token_types:
            if self.check(token_type):
                self.advance()
                return True
        return False

    def check(self, token_type):
        if self.is_at_end():
            return False
        return self.peek().type == token_type

    def advance(self):
        if not self.is_at_end():
            self.current += 1
        return self.previous()

    def is_at_end(self):
        return self.peek().type == "EOF"

    def peek(self):
        return self.tokens[self.current]

    def previous(self):
        return self.tokens[self.current - 1]

    def consume(self, token_type, message):
        if self.check(token_type):
            return self.advance()
        self.error(message)

    def error(self, message, token=None):
        token = token or self.peek()
        if token.type == "EOF":
            sys.exit(f"Syntax Error: At EOF. {message}")
        else:
            sys.exit(f"Syntax Error: Line {token.line}, at token '{token.text}'. {message}")
