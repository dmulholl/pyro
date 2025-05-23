#ifndef pyro_operators_h
#define pyro_operators_h

// Returns [left] + [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic and/or exit flags.
PyroValue pyro_op_binary_plus(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] - [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_minus(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] * [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_star(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] / [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_slash(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] // [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_slash_slash(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] | [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_bar(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] & [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_amp(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] ^ [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_caret(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] % [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_percent(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] rem [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_rem(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] mod [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_mod(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] ** [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_star_star(PyroVM* vm, PyroValue left, PyroValue right);

// Returns +[operand]. Panics if the operation is not defined for the operand type.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_unary_plus(PyroVM* vm, PyroValue operand);

// Returns -[operand]. Panics if the operation is not defined for the operand type.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_unary_minus(PyroVM* vm, PyroValue operand);

// Returns true if [left] == [right].
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_eq(PyroVM* vm, PyroValue left, PyroValue right);

// Returns true if [left] < [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_lt(PyroVM* vm, PyroValue left, PyroValue right);

// Returns true if [left] <= [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_le(PyroVM* vm, PyroValue left, PyroValue right);

// Returns true if [left] > [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_gt(PyroVM* vm, PyroValue left, PyroValue right);

// Returns true if [left] >= [right]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_ge(PyroVM* vm, PyroValue left, PyroValue right);

// Implements the get-index operator, [].
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_get_index(PyroVM* vm, PyroValue receiver, PyroValue key);

// Implements the set-index operator, [].
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_set_index(PyroVM* vm, PyroValue receiver, PyroValue key, PyroValue value);

// Returns [left] << [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_less_less(PyroVM* vm, PyroValue left, PyroValue right);

// Returns [left] >> [right]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_binary_greater_greater(PyroVM* vm, PyroValue left, PyroValue right);

// Returns ~[operand]. Panics if the operation is not defined for the operand type.
// This function can call into Pyro code and can set the panic or exit flags.
PyroValue pyro_op_unary_tilde(PyroVM* vm, PyroValue operand);

#endif
