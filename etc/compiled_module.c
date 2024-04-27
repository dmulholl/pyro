// This is a sample compiled module. After this file has been compiled as a dynamic library, you
// can import the resulting 'compiled_module.so' file just like a regular Pyro module, e.g.
//
//  import compiled_module;
//  echo compiled_module::add_values(1.23, 4.56);
//  echo compiled_module::add_values("foo", "bar");
//  echo compiled_module::add_i64s(123, 456);
//
// You can build this sample compiled module by running:
//
//  make compiled-module
//
// You can find the resulting 'compiled_module.so' file in the 'build/release' directory.
//
// For more examples, see the implementation of Pyro's builtin standard library modules in the
// Pyro repository's 'src/stdlib' directory.

#include "../src/includes/pyro.h"

// All Pyro functions and methods implemented in C have the same function signature.
// - [args] is a pointer to the function-call's arguments.
// - [arg_count] is the number of arguments in [args].
// - If the function has been registered as a method on a Pyro class, the object instance that the
//   method is being called on will be located at [args[-1]].
static PyroValue fn_add_values(PyroVM* vm, size_t arg_count, PyroValue* args) {
    // This is equivalent to returning the value of the expression [arg1 + arg2]. It will panic and
    // return null if the argument types are incompatible for the + operator.
    return pyro_op_binary_plus(vm, args[0], args[1]);
}

// This function adds [i64] values -- it will panic and return null if either argument (or both) are
// not of type [i64].
static PyroValue fn_add_i64s(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_I64(args[0])) {
        pyro_panic(
            vm,
            "add_i64s(): invalid argument, expected an i64, found %s",
            pyro_get_type_name(vm, args[0])->bytes
        );
        return pyro_null();
    }

    if (!PYRO_IS_I64(args[1])) {
        pyro_panic(
            vm,
            "add_i64s(): invalid argument, expected an i64, found %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
        return pyro_null();
    }

    int64_t arg1 = args[0].as.i64;
    int64_t arg2 = args[1].as.i64;

    return pyro_i64(arg1 + arg2);
}

// This is the module's initialization function. Pyro calls this function after loading the module
// as a dynamic library. Its job is to register the module's functions, classes, methods, etc. with
// the Pyro VM. The function name has the format [pyro_init_module_<module-name>].
bool pyro_init_module_compiled_module(PyroVM* vm, PyroMod* module) {
    // This function call registers the C-function [fn_add_values] as a public member function in
    // the module under the name [add_values]. This function call can fail in the (unlikely, but
    // possible) event that memory cannot be allocated for the new module member. In this case, we
    // return false to indicate that module initialization has failed.
    if (!pyro_define_pub_member_fn(vm, module, "add_values", fn_add_values, 2)) {
        return false;
    }

    // When you register a function or method, you can specify the exact number of arguments it
    // takes -- in this case the function takes exactly 2 arguments. Pyro will automatically panic
    // if code attempts to call the function with an invalid number of arguments. If you specify an
    // argument count of -1 the function or method will accept *any* number of arguments, i.e. it
    // will be variadic. In this case, it's up to you to handle the argument count appropriately in
    // the function definition.
    if (!pyro_define_pub_member_fn(vm, module, "add_i64s", fn_add_i64s, 2)) {
        return false;
    }

    return true;
}
