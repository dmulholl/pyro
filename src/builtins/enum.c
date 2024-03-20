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


void pyro_load_std_builtins_enum(PyroVM* vm) {
    // Functions.
    // pyro_define_superglobal_fn(vm, "$is_map", fn_is_map, 1);

    // Enum Type methods.
    pyro_define_pri_method(vm, vm->class_enum_type, "$iter", enum_type_members, 0);
    pyro_define_pub_method(vm, vm->class_enum_type, "members", enum_type_members, 0);

    // Enum Value methods.
    pyro_define_pub_method(vm, vm->class_enum_member, "value", enum_member_value, 0);
    pyro_define_pub_method(vm, vm->class_enum_member, "type", enum_member_type, 0);
}
