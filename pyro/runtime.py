import sys
from . import builtins


def error(message, token=None):
    if token:
        sys.exit(f"Runtime Error: Line {token.line}. {message}")
    else:
        sys.exit(f"Runtime Error: {message}")


def is_truthy(value):
    return bool(value)


def stringify(value):
    if value is None:
        return "null"
    if value is True:
        return "true"
    if value is False:
        return "false"
    if isinstance(value, float):
        text = str(value)
        if text.endswith('.0'):
            text = text[:-2]
        return text
    return str(value)


def check_operands_are_floats_or_strings(left, right, op_token):
    if isinstance(left, float) and isinstance(right, float):
        return True
    if isinstance(left, str) and isinstance(right, str):
        return True
    error(f"Incompatible operands for the '{op_token.text}' operator.", op_token)


def check_operands_are_floats(left, right, op_token):
    if isinstance(left, float) and isinstance(right, float):
        return True
    error(f"Incompatible operands for the '{op_token.text}' operator.", op_token)


def check_operand_is_float(value, op_token):
    if isinstance(value, float):
        return True
    error(f"Incompatible operand for the '{op_token.text}' operator.", op_token)


class Break(Exception):
    pass


class Continue(Exception):
    pass


class Return(Exception):
    def __init__(self, value):
        self.value = value


class Environment:

    def __init__(self, enclosing_env=None):
        self.values = {}
        self.enclosing_env = enclosing_env

    def define(self, name, value):
        self.values[name] = value

    def get(self, token):
        if token.text in self.values:
            return self.values[token.text]
        elif self.enclosing_env is not None:
            return self.enclosing_env.get(token)
        else:
            error(f"Cannot get undefined variable '{token.text}'.", token)

    def set(self, token, value):
        if token.text in self.values:
            self.values[token.text] = value
        elif self.enclosing_env is not None:
            self.enclosing_env.set(token, value)
        else:
            error(f"Cannot set undefined variable '{token.text}'.", token)

    def get_str(self, name):
        if name in self.values:
            return self.values[name]
        elif self.enclosing_env is not None:
            return self.enclosing_env.get_str(name)
        else:
            error(f"Cannot get undefined variable '{name}'.")


class Interpreter:

    def __init__(self):
        self.env = Environment()
        self.env.define("$clock", builtins.Clock())
        self.env.define("$print", builtins.Print())
        self.env.define("$println", builtins.PrintLine())
        self.env.define("$str", builtins.Stringify())

    def run(self, statements):
        for statement in statements:
            statement.exec(self.env)
        if main := self.env.values.get("$main"):
            if main.arity() != 0:
                error("The $main() function does not accept arguments.")
            main.call([])


class PyroFunction:

    def __init__(self, func_stmt, enclosing_env):
        self.func_stmt = func_stmt
        self.env = enclosing_env

    def __str__(self):
        return f"<PyroFunction {self.func_stmt.name_token.text}>"

    def arity(self):
        return len(self.func_stmt.parameters)

    def call(self, args):
        env = Environment(self.env)
        for i in range(self.arity()):
            env.define(self.func_stmt.parameters[i].text, args[i])
        try:
            self.func_stmt.body.exec(env)
        except Return as ret:
            return ret.value
        return None

    def bind(self, instance_obj):
        instance_env = Environment(self.env)
        instance_env.define("self", instance_obj)
        return PyroFunction(self.func_stmt, instance_env)


class PyroClass:

    def __init__(self, class_stmt, superclass_obj, method_objs, enclosing_env):
        self.class_stmt = class_stmt
        self.superclass_obj = superclass_obj
        self.methods = method_objs
        self.env = enclosing_env
        self.superclass_chain = [self]
        current = superclass_obj
        while current is not None:
            self.superclass_chain.append(current)
            current = current.superclass_obj

    def __str__(self):
        return f"<PyroClass {self.class_stmt.name_token.text}>"

    def call(self, args):
        instance_obj = PyroInstance(self)

        for class_obj in reversed(self.superclass_chain):
            for field_dec in class_obj.class_stmt.field_decs:
                value = None
                if field_dec.initializer is not None:
                    value = field_dec.initializer.eval(class_obj.env)
                instance_obj.fields[field_dec.name_token.text] = value

        constructor = self.find_and_bind_method(instance_obj, "$init")
        if constructor is not None:
            constructor.call(args)

        return instance_obj

    def arity(self):
        constructor = self.find_and_bind_method(None, "$init")
        if constructor is not None:
            return constructor.arity()
        return 0

    def find_and_bind_method(self, instance_obj, method_name):
        if method_name in self.methods:
            return self.methods[method_name].bind(instance_obj)
        if self.superclass_obj is not None:
            return self.superclass_obj.find_and_bind_method(instance_obj, method_name)
        return None


class PyroInstance:

    def __init__(self, class_obj):
       self.class_obj = class_obj
       self.fields = {}

    def __str__(self):
        return f"<PyroInstance {self.class_obj.class_stmt.name_token.text}>"

    def get(self, attr_token):
        if attr_token.text in self.fields:
            return self.fields[attr_token.text]
        bound_func_obj = self.class_obj.find_and_bind_method(self, attr_token.text)
        if bound_func_obj is not None:
            return bound_func_obj
        error(f"Cannot get undefined attribute '{attr_token.text}'.", attr_token)

    def set(self, attr_token, value):
        if attr_token.text in self.fields:
            self.fields[attr_token.text] = value
        else:
            error(f"Cannot set undefined attribute '{attr_token.text}'.", attr_token)

