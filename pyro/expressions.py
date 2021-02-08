from . import runtime
import sys


class Expression:

    def __str__(self):
        return f"{self.__class__.__name__}"


class UnaryExpr(Expression):

    def __init__(self, operator_token, expr):
        self.op = operator_token
        self.expr = expr

    def __str__(self):
        return f"({self.op.text} {self.expr})"

    def eval(self, env):
        value = self.expr.eval(env)
        if self.op.type == "MINUS":
            runtime.check_operand_is_float(value, self.op)
            return -value
        elif self.op.type == "BANG":
            return not runtime.is_truthy(value)


class BinaryExpr(Expression):

    def __init__(self, left_expr, operator_token, right_expr):
        self.left = left_expr
        self.op = operator_token
        self.right = right_expr

    def __str__(self):
        return f"({self.op.text} {self.left} {self.right})"

    def eval(self, env):
        left = self.left.eval(env)
        right = self.right.eval(env)
        if self.op.type == "GREATER":
            runtime.check_operands_are_floats_or_strings(left, right, self.op)
            return left > right
        elif self.op.type == "GREATER_EQUAL":
            runtime.check_operands_are_floats_or_strings(left, right, self.op)
            return left >= right
        elif self.op.type == "LESS":
            runtime.check_operands_are_floats_or_strings(left, right, self.op)
            return left < right
        elif self.op.type == "LESS_EQUAL":
            runtime.check_operands_are_floats_or_strings(left, right, self.op)
            return left <= right
        elif self.op.type in ("PLUS", "PLUS_EQUAL"):
            runtime.check_operands_are_floats_or_strings(left, right, self.op)
            return left + right
        elif self.op.type in ("MINUS", "MINUS_EQUAL"):
            runtime.check_operands_are_floats(left, right, self.op)
            return left - right
        elif self.op.type == "SLASH":
            runtime.check_operands_are_floats(left, right, self.op)
            return left / right
        elif self.op.type == "STAR":
            runtime.check_operands_are_floats(left, right, self.op)
            return left * right
        elif self.op.type == "BANG_EQUAL":
            return left != right
        elif self.op.type == "EQUAL_EQUAL":
            return left == right


class LogicalExpr(Expression):

    def __init__(self, left_expr, operator_token, right_expr):
        self.left = left_expr
        self.op = operator_token
        self.right = right_expr

    def __str__(self):
        return f"({self.op.text} {self.left} {self.right})"

    def eval(self, env):
        left = self.left.eval(env)
        if self.op.type == "OR":
            if runtime.is_truthy(left):
                return left
        else:
            if not runtime.is_truthy(left):
                return left
        return self.right.eval(env)


class LiteralExpr(Expression):

    def __init__(self, token, value):
        self.token = token
        self.value = value

    def __str__(self):
        if self.token.type in ("NULL", "TRUE", "FALSE"):
            return self.token.type.lower()
        else:
            return repr(self.value)

    def eval(self, env):
        return self.value


class GroupingExpr(Expression):

    def __init__(self, expr):
        self.expr = expr

    def __str__(self):
        return f"(GROUP {self.expr})"

    def eval(self, env):
        return self.expr.eval(env)


class VariableExpr(Expression):

    def __init__(self, token):
        self.token = token

    def __str__(self):
        return self.token.text

    def eval(self, env):
        return env.get(self.token)


class AssignExpr(Expression):

    def __init__(self, token, value_expr):
        self.token = token
        self.value = value_expr

    def __str__(self):
        return f"(= {self.token.text} {self.value})"

    def eval(self, env):
        value = self.value.eval(env)
        env.set(self.token, value)
        return value


class CallExpr(Expression):

    def __init__(self, callee_expr, left_paren_token, arg_expr_list):
        self.callee = callee_expr
        self.token = left_paren_token
        self.args = arg_expr_list

    def __str__(self):
        return f"(CALL {self.callee})"

    def eval(self, env):
        callee = self.callee.eval(env)
        if not hasattr(callee, "call"):
            runtime.error(f"Object is not callable.", self.token)
        if len(self.args) != callee.arity():
            runtime.error(f"Invalid number of arguments.", self.token)
        args = []
        for arg in self.args:
            args.append(arg.eval(env))
        return callee.call(args)


class GetAttrExpr(Expression):

    def __init__(self, object_expr, name_token):
        self.object = object_expr
        self.name = name_token

    def __str__(self):
        return f"(GET {self.object} {self.name.text})"

    def eval(self, env):
        instance_obj = self.object.eval(env)
        if isinstance(instance_obj, runtime.PyroInstance):
            return instance_obj.get(self.name)
        runtime.error(f"Invalid attribute '{self.name.text}', not an instance object.", self.name)


class SetAttrExpr(Expression):

    def __init__(self, object_expr, name_token, value_expr):
        self.object = object_expr
        self.name = name_token
        self.value = value_expr

    def __str__(self):
        return f"(SET {self.object} {self.name.text} {self.value})"

    def eval(self, env):
        instance_obj = self.object.eval(env)
        if isinstance(instance_obj, runtime.PyroInstance):
            value = self.value.eval(env)
            instance_obj.set(self.name, value)
            return value
        runtime.error(f"Invalid attribute '{self.name.text}', not an instance object.", self.name)


class SelfExpr(Expression):

    def __init__(self, token):
        self.token = token

    def __str__(self):
        return "self"

    def eval(self, env):
        return env.get(self.token)


class SuperExpr(Expression):

    def __init__(self, super_token, attr_token):
        self.super_token = super_token
        self.attr_token = attr_token

    def eval(self, env):
        superclass_obj = env.get(self.super_token)
        instance_obj = env.get_str("self")
        method = superclass_obj.find_and_bind_method(instance_obj, self.attr_token.text)
        if method is None:
            runtime.error(f"Invalid superclass method name '{self.attr_token.text}'.", self.attr_token)
        return method


class ConditionalExpr(Expression):

    def __init__(self, condition_expr, true_branch_expr, false_branch_expr):
        self.condition = condition_expr
        self.true_branch = true_branch_expr
        self.false_branch = false_branch_expr

    def eval(self, env):
        if self.condition.eval(env):
            return self.true_branch.eval(env)
        else:
            return self.false_branch.eval(env)

