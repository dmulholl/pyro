#ifndef pyro_operators_h
#define pyro_operators_h

#include "common.h"
#include "values.h"

// Returns [a] + [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_plus(PyroVM* vm, Value a, Value b);

// Returns [a] - [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_minus(PyroVM* vm, Value a, Value b);

// Returns [a] * [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_star(PyroVM* vm, Value a, Value b);

// Returns [a] / [b]. Panics if the operation is not defined for the operand types.
// This function can call into Pyro code and can set the panic or exit flags.
Value pyro_op_binary_slash(PyroVM* vm, Value a, Value b);

// Returns true if [a] == [b].
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_eq(PyroVM* vm, Value a, Value b);

// Returns true if [a] < [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_lt(PyroVM* vm, Value a, Value b);

// Returns true if [a] <= [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_le(PyroVM* vm, Value a, Value b);

// Returns true if [a] > [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_gt(PyroVM* vm, Value a, Value b);

// Returns true if [a] >= [b]. Panics if the values are not comparable.
// This function can call into Pyro code and can set the panic or exit flags.
bool pyro_op_compare_ge(PyroVM* vm, Value a, Value b);

#endif
