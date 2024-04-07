#include "../includes/pyro.h"


static PyroValue enum_type_members(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroEnumType* enum_type = PYRO_AS_ENUM_TYPE(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)enum_type->values, PYRO_ITER_MAP_VALUES, vm);
    if (!iter) {
        pyro_panic(vm, "out of memory");
        return pyro_null();
    }

    return pyro_obj(iter);
}


static PyroValue enum_member_value(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroEnumMember* enum_member = PYRO_AS_ENUM_MEMBER(args[-1]);
    return enum_member->value;
}


static PyroValue enum_member_type(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroEnumMember* enum_member = PYRO_AS_ENUM_MEMBER(args[-1]);
    return pyro_obj(enum_member->enum_type);
}


static PyroValue fn_is_enum_type(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_ENUM_TYPE(args[0]));
}


static PyroValue fn_is_enum_member(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_ENUM_MEMBER(args[0]));
}


static PyroValue fn_is_enum_member_of_type(PyroVM* vm, size_t arg_count, PyroValue* args) {
    if (!PYRO_IS_ENUM_TYPE(args[1])) {
        pyro_panic(
            vm,
            "$is_enum_member_of_type(): invalid argument [type]: expected an enum type, found %s",
            pyro_get_type_name(vm, args[1])->bytes
        );
        return pyro_null();
    }

    if (!PYRO_IS_ENUM_MEMBER(args[0])) {
        return pyro_bool(false);
    }

    if (PYRO_AS_ENUM_MEMBER(args[0])->enum_type == PYRO_AS_ENUM_TYPE(args[1])) {
        return pyro_bool(true);
    }

    return pyro_bool(false);
}


void pyro_load_std_builtins_enum(PyroVM* vm) {
    // Functions.
    pyro_define_superglobal_fn(vm, "$is_enum_type", fn_is_enum_type, 1);
    pyro_define_superglobal_fn(vm, "$is_enum_member", fn_is_enum_member, 1);
    pyro_define_superglobal_fn(vm, "$is_enum_member_of_type", fn_is_enum_member_of_type, 2);

    // Enum Type methods.
    pyro_define_pri_method(vm, vm->class_enum_type, "$iter", enum_type_members, 0);
    pyro_define_pub_method(vm, vm->class_enum_type, "members", enum_type_members, 0);

    // Enum Value methods.
    pyro_define_pub_method(vm, vm->class_enum_member, "value", enum_member_value, 0);
    pyro_define_pub_method(vm, vm->class_enum_member, "type", enum_member_type, 0);
}
