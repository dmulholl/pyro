from . import runtime


class Statement:
    pass


class ExpressionStmt(Statement):

    def __init__(self, expr):
        self.expr = expr

    def exec(self, env):
        self.expr.eval(env)


class VarDecStmt(Statement):

    def __init__(self, name_token, initializer_expr):
        self.name_token = name_token
        self.initializer = initializer_expr

    def exec(self, env):
        value = None
        if self.initializer is not None:
            value = self.initializer.eval(env)
        env.define(self.name_token.text, value)


class BlockStmt(Statement):

    def __init__(self, statements):
        self.statements = statements

    def exec(self, env):
        block_env = runtime.Environment(env)
        for statement in self.statements:
            statement.exec(block_env)


class EchoStmt(Statement):

    def __init__(self, expr_list):
        self.expr_list = expr_list

    def exec(self, env):
        values = [expr.eval(env) for expr in self.expr_list]
        output = " ".join(runtime.stringify(value) for value in values)
        print(output)


class IfStmt(Statement):

    def __init__(self, condition_expr, then_stmt, else_stmt):
        self.condition = condition_expr
        self.then_stmt = then_stmt
        self.else_stmt = else_stmt

    def exec(self, env):
        if runtime.is_truthy(self.condition.eval(env)):
            self.then_stmt.exec(env)
        elif self.else_stmt is not None:
            self.else_stmt.exec(env)


class WhileStmt(Statement):

    def __init__(self, condition_expr, body_stmt):
        self.condition = condition_expr
        self.body_stmt = body_stmt

    def exec(self, env):
        try:
            while runtime.is_truthy(self.condition.eval(env)):
                try:
                    self.body_stmt.exec(env)
                except runtime.Continue:
                    pass
        except runtime.Break:
            pass


class ForStmt(Statement):

    def __init__(self, initializer_stmt, condition_expr, increment_expr, body_stmt):
        self.initializer = initializer_stmt
        self.condition = condition_expr
        self.increment = increment_expr
        self.body = body_stmt

    def exec(self, env):
        if self.initializer is not None:
            env = runtime.Environment(env)
            self.initializer.exec(env)
        try:
            while runtime.is_truthy(self.condition.eval(env)):
                try:
                    self.body.exec(env)
                except runtime.Continue:
                    pass
                if self.increment is not None:
                    self.increment.eval(env)
        except runtime.Break:
            pass


class ReturnStmt(Statement):

    def __init__(self, value_expr):
        self.value = value_expr

    def exec(self, env):
        if self.value is None:
            raise runtime.Return(None)
        else:
            raise runtime.Return(self.value.eval(env))


class BreakStmt(Statement):

    def exec(self, env):
        raise runtime.Break()


class ContinueStmt(Statement):

    def exec(self, env):
        raise runtime.Continue()


class FunctionStmt(Statement):

    def __init__(self, name_token, parameter_token_list, body_block_stmt):
        self.name_token = name_token
        self.parameters = parameter_token_list
        self.body = body_block_stmt

    def exec(self, env):
        func_obj = runtime.PyroFunction(self, env)
        env.define(self.name_token.text, func_obj)


class ClassStmt(Statement):

    def __init__(self, name_token, superclass_expr, method_stmt_list, field_dec_list):
        self.name_token = name_token
        self.superclass_expr = superclass_expr
        self.method_stmts = method_stmt_list
        self.field_decs = field_dec_list

    def exec(self, env):
        class_env = runtime.Environment(env)
        superclass_obj = None
        if self.superclass_expr is not None:
            superclass_obj = self.superclass_expr.eval(env)
            if not isinstance(superclass_obj, runtime.PyroClass):
                token = self.superclass_expr.token
                runtime.error(f"Invalid superclass name '{token.text}'.", self.token)
            class_env.define("super", superclass_obj)
        method_objs = {}
        for func_stmt in self.method_stmts:
            func_obj = runtime.PyroFunction(func_stmt, class_env)
            method_objs[func_stmt.name_token.text] = func_obj
        class_obj = runtime.PyroClass(self, superclass_obj, method_objs, class_env)
        env.define(self.name_token.text, class_obj)
