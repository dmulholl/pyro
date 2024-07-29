#include "../includes/pyro.h"


PyroVM* pyro_new_vm(void) {
    PyroVM* vm = malloc(sizeof(PyroVM));
    if (!vm) {
        return NULL;
    }

    // Initialize or zero-out all fields before attempting to allocate memory.
    vm->bytes_allocated = sizeof(PyroVM);
    vm->stack = NULL;
    vm->stack_top = NULL;
    vm->stack_max = NULL;
    vm->call_stack = NULL;
    vm->call_stack_count = 0;
    vm->call_stack_capacity = 0;
    vm->class_buf = NULL;
    vm->class_err = NULL;
    vm->class_file = NULL;
    vm->class_iter = NULL;
    vm->class_map = NULL;
    vm->class_queue = NULL;
    vm->class_set = NULL;
    vm->class_stack = NULL;
    vm->class_str = NULL;
    vm->class_tup = NULL;
    vm->class_vec = NULL;
    vm->class_module = NULL;
    vm->class_rune = NULL;
    vm->class_enum_type = NULL;
    vm->class_enum_member = NULL;
    vm->empty_error = NULL;
    vm->empty_string = NULL;
    vm->empty_tuple = NULL;
    vm->exit_code = 0;
    vm->exit_flag = false;
    vm->gc_disallows = 0;
    vm->superglobals = NULL;
    vm->grey_stack_capacity = 0;
    vm->grey_stack_count = 0;
    vm->grey_stack = NULL;
    vm->halt_flag = false;
    vm->import_roots = NULL;
    vm->args = NULL;
    vm->in_repl = false;
    vm->main_module = NULL;
    vm->max_bytes = SIZE_MAX;
    vm->memory_allocation_failed = false;
    vm->module_cache = NULL;
    vm->next_gc_threshold = PYRO_INIT_GC_THRESHOLD;
    vm->objects = NULL;
    vm->open_upvalues = NULL;
    vm->panic_buffer = NULL;
    vm->panic_flag = false;
    vm->panic_count = 0;
    vm->stderr_file = NULL;
    vm->stdin_file = NULL;
    vm->stdout_file = NULL;
    vm->str_bool = NULL;
    vm->str_buf = NULL;
    vm->str_rune = NULL;
    vm->str_class = NULL;
    vm->str_dollar_call = NULL;
    vm->str_dollar_contains = NULL;
    vm->str_dollar_debug = NULL;
    vm->str_dollar_fmt = NULL;
    vm->str_dollar_get = NULL;
    vm->str_dollar_hash = NULL;
    vm->str_dollar_init = NULL;
    vm->str_dollar_iter = NULL;
    vm->str_dollar_next = NULL;
    vm->str_dollar_set = NULL;
    vm->str_dollar_str = NULL;
    vm->str_err = NULL;
    vm->str_false = NULL;
    vm->str_file = NULL;
    vm->str_func = NULL;
    vm->str_i64 = NULL;
    vm->str_instance = NULL;
    vm->str_iter = NULL;
    vm->str_map = NULL;
    vm->str_method = NULL;
    vm->str_module = NULL;
    vm->str_null = NULL;
    vm->str_op_binary_equals_equals = NULL;
    vm->str_op_binary_greater = NULL;
    vm->str_rop_binary_greater = NULL;
    vm->str_op_binary_greater_equals = NULL;
    vm->str_rop_binary_greater_equals = NULL;
    vm->str_op_binary_less = NULL;
    vm->str_rop_binary_less = NULL;
    vm->str_op_binary_less_equals = NULL;
    vm->str_rop_binary_less_equals = NULL;
    vm->str_op_binary_minus = NULL;
    vm->str_rop_binary_minus = NULL;
    vm->str_op_binary_plus = NULL;
    vm->str_rop_binary_plus = NULL;
    vm->str_op_binary_bar = NULL;
    vm->str_rop_binary_bar = NULL;
    vm->str_op_binary_amp = NULL;
    vm->str_rop_binary_amp = NULL;
    vm->str_op_binary_slash = NULL;
    vm->str_rop_binary_slash = NULL;
    vm->str_op_binary_star = NULL;
    vm->str_rop_binary_star = NULL;
    vm->str_op_binary_caret = NULL;
    vm->str_rop_binary_caret = NULL;
    vm->str_op_binary_percent = NULL;
    vm->str_rop_binary_percent = NULL;
    vm->str_op_binary_star_star = NULL;
    vm->str_rop_binary_star_star = NULL;
    vm->str_op_binary_slash_slash = NULL;
    vm->str_rop_binary_slash_slash = NULL;
    vm->str_op_unary_minus = NULL;
    vm->str_op_unary_plus = NULL;
    vm->str_queue = NULL;
    vm->str_set = NULL;
    vm->str_stack = NULL;
    vm->str_str = NULL;
    vm->str_true = NULL;
    vm->str_tup = NULL;
    vm->str_vec = NULL;
    vm->str_source = NULL;
    vm->str_line = NULL;
    vm->try_depth = 0;
    vm->with_stack = NULL;
    vm->with_stack_count = 0;
    vm->with_stack_capacity = 0;
    vm->str_dollar_end_with = NULL;
    vm->panic_source_id = NULL;
    vm->panic_line_number = 0;
    vm->str_op_binary_less_less = NULL;
    vm->str_rop_binary_less_less = NULL;
    vm->str_op_binary_greater_greater = NULL;
    vm->str_rop_binary_greater_greater = NULL;
    vm->str_op_unary_tilde = NULL;
    vm->trace_execution = false;
    vm->str_enum = NULL;
    vm->str_count = NULL;

    // We need to initialize these classes before we create any objects.
    vm->class_buf = PyroClass_new(vm);
    vm->class_err = PyroClass_new(vm);
    vm->class_file = PyroClass_new(vm);
    vm->class_iter = PyroClass_new(vm);
    vm->class_map = PyroClass_new(vm);
    vm->class_queue = PyroClass_new(vm);
    vm->class_set = PyroClass_new(vm);
    vm->class_stack = PyroClass_new(vm);
    vm->class_str = PyroClass_new(vm);
    vm->class_tup = PyroClass_new(vm);
    vm->class_vec = PyroClass_new(vm);
    vm->class_module = PyroClass_new(vm);
    vm->class_rune = PyroClass_new(vm);
    vm->class_enum_type = PyroClass_new(vm);
    vm->class_enum_member = PyroClass_new(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // We need to initialize the interned strings pool before we create any strings.
    PyroStrPool_init(&vm->string_pool);

    // Canned objects.
    vm->empty_string = PyroStr_COPY("");
    vm->empty_error = PyroErr_new(vm);
    vm->empty_tuple = PyroTup_new(0, vm);

    // Static strings.
    vm->str_dollar_init = PyroStr_COPY("$init");
    vm->str_dollar_str = PyroStr_COPY("$str");
    vm->str_true = PyroStr_COPY("true");
    vm->str_false = PyroStr_COPY("false");
    vm->str_null = PyroStr_COPY("null");
    vm->str_dollar_fmt = PyroStr_COPY("$fmt");
    vm->str_dollar_iter = PyroStr_COPY("$iter");
    vm->str_dollar_next = PyroStr_COPY("$next");
    vm->str_dollar_get = PyroStr_COPY("$get");
    vm->str_dollar_set = PyroStr_COPY("$set");
    vm->str_dollar_debug = PyroStr_COPY("$debug");
    vm->str_op_binary_equals_equals = PyroStr_COPY("$op_binary_equals_equals");
    vm->str_op_binary_less = PyroStr_COPY("$op_binary_less");
    vm->str_rop_binary_less = PyroStr_COPY("$rop_binary_less");
    vm->str_op_binary_less_equals = PyroStr_COPY("$op_binary_less_equals");
    vm->str_rop_binary_less_equals = PyroStr_COPY("$rop_binary_less_equals");
    vm->str_op_binary_greater = PyroStr_COPY("$op_binary_greater");
    vm->str_rop_binary_greater = PyroStr_COPY("$rop_binary_greater");
    vm->str_op_binary_greater_equals = PyroStr_COPY("$op_binary_greater_equals");
    vm->str_rop_binary_greater_equals = PyroStr_COPY("$rop_binary_greater_equals");
    vm->str_op_binary_plus = PyroStr_COPY("$op_binary_plus");
    vm->str_rop_binary_plus = PyroStr_COPY("$rop_binary_plus");
    vm->str_op_binary_minus = PyroStr_COPY("$op_binary_minus");
    vm->str_rop_binary_minus = PyroStr_COPY("$rop_binary_minus");
    vm->str_op_binary_bar = PyroStr_COPY("$op_binary_bar");
    vm->str_rop_binary_bar = PyroStr_COPY("$rop_binary_bar");
    vm->str_op_binary_amp = PyroStr_COPY("$op_binary_amp");
    vm->str_rop_binary_amp = PyroStr_COPY("$rop_binary_amp");
    vm->str_op_binary_star = PyroStr_COPY("$op_binary_star");
    vm->str_rop_binary_star = PyroStr_COPY("$rop_binary_star");
    vm->str_op_binary_slash = PyroStr_COPY("$op_binary_slash");
    vm->str_rop_binary_slash = PyroStr_COPY("$rop_binary_slash");
    vm->str_op_binary_caret = PyroStr_COPY("$op_binary_caret");
    vm->str_rop_binary_caret = PyroStr_COPY("$rop_binary_caret");
    vm->str_op_binary_percent = PyroStr_COPY("$op_binary_percent");
    vm->str_rop_binary_percent = PyroStr_COPY("$rop_binary_percent");
    vm->str_op_binary_star_star = PyroStr_COPY("$op_binary_star_star");
    vm->str_rop_binary_star_star = PyroStr_COPY("$rop_binary_star_star");
    vm->str_op_binary_slash_slash = PyroStr_COPY("$op_binary_slash_slash");
    vm->str_rop_binary_slash_slash = PyroStr_COPY("$rop_binary_slash_slash");
    vm->str_op_unary_plus = PyroStr_COPY("$op_unary_plus");
    vm->str_op_unary_minus = PyroStr_COPY("$op_unary_minus");
    vm->str_dollar_hash = PyroStr_COPY("$hash");
    vm->str_dollar_call = PyroStr_COPY("$call");
    vm->str_dollar_contains = PyroStr_COPY("$contains");
    vm->str_bool = PyroStr_COPY("bool");
    vm->str_i64 = PyroStr_COPY("i64");
    vm->str_f64 = PyroStr_COPY("f64");
    vm->str_rune = PyroStr_COPY("rune");
    vm->str_method = PyroStr_COPY("method");
    vm->str_buf = PyroStr_COPY("buf");
    vm->str_class = PyroStr_COPY("class");
    vm->str_func = PyroStr_COPY("func");
    vm->str_instance = PyroStr_COPY("instance");
    vm->str_file = PyroStr_COPY("file");
    vm->str_iter = PyroStr_COPY("iter");
    vm->str_map = PyroStr_COPY("map");
    vm->str_set = PyroStr_COPY("set");
    vm->str_vec = PyroStr_COPY("vec");
    vm->str_source = PyroStr_COPY("source");
    vm->str_line = PyroStr_COPY("line");
    vm->str_stack = PyroStr_COPY("stack");
    vm->str_queue = PyroStr_COPY("queue");
    vm->str_str = PyroStr_COPY("str");
    vm->str_module = PyroStr_COPY("module");
    vm->str_tup = PyroStr_COPY("tup");
    vm->str_err = PyroStr_COPY("err");
    vm->str_dollar_end_with = PyroStr_COPY("$end_with");
    vm->str_op_binary_less_less = PyroStr_COPY("$op_binary_less_less");
    vm->str_rop_binary_less_less = PyroStr_COPY("$rop_binary_less_less");
    vm->str_op_binary_greater_greater = PyroStr_COPY("$op_binary_greater_greater");
    vm->str_rop_binary_greater_greater = PyroStr_COPY("$rop_binary_greater_greater");
    vm->str_op_unary_tilde = PyroStr_COPY("$op_unary_tilde");
    vm->str_enum = PyroStr_COPY("enum");
    vm->str_count = PyroStr_COPY("count");

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Object fields.
    vm->superglobals = PyroMap_new(vm);
    vm->module_cache = PyroMap_new(vm);
    vm->main_module = PyroMod_new(vm);
    vm->import_roots = PyroVec_new(vm);
    vm->args = PyroVec_new(vm);
    vm->panic_buffer = PyroBuf_new_with_capacity(256, vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Standard IO streams.
    vm->stdout_file = PyroFile_new(vm, stdout);
    pyro_define_superglobal(vm, "$stdout", pyro_obj(vm->stdout_file));
    pyro_define_superglobal(vm, "$o", pyro_obj(vm->stdout_file));

    vm->stderr_file = PyroFile_new(vm, stderr);
    pyro_define_superglobal(vm, "$stderr", pyro_obj(vm->stderr_file));
    pyro_define_superglobal(vm, "$e", pyro_obj(vm->stderr_file));

    vm->stdin_file = PyroFile_new(vm, stdin);
    pyro_define_superglobal(vm, "$stdin", pyro_obj(vm->stdin_file));
    pyro_define_superglobal(vm, "$i", pyro_obj(vm->stdin_file));

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Initialize the global psuedo-random number generator.
    PyroBuf* buf = pyro_csrng_rand_bytes(vm, 8, "initializing prng");
    if (vm->halt_flag) {
        pyro_free_vm(vm);
        return NULL;
    }

    pyro_xoshiro256ss_init(&vm->prng_state, *((uint64_t*)buf->bytes));

    // Load builtins.
    pyro_load_superglobals(vm);
    pyro_load_builtin_type_map(vm);
    pyro_load_builtin_type_vec(vm);
    pyro_load_builtin_type_tup(vm);
    pyro_load_builtin_type_str(vm);
    pyro_load_builtin_type_buf(vm);
    pyro_load_builtin_type_file(vm);
    pyro_load_builtin_type_iter(vm);
    pyro_load_builtin_type_queue(vm);
    pyro_load_builtin_type_err(vm);
    pyro_load_builtin_type_module(vm);
    pyro_load_builtin_type_rune(vm);
    pyro_load_builtin_type_enum(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    return vm;
}


void pyro_free_vm(PyroVM* vm) {
    PyroObject* object = vm->objects;
    while (object != NULL) {
        PyroObject* next = object->next;
        pyro_free_object(vm, object);
        object = next;
    }

    PYRO_FREE_ARRAY(vm, PyroObject*, vm->grey_stack, vm->grey_stack_capacity);
    PYRO_FREE_ARRAY(vm, PyroCallFrame, vm->call_stack, vm->call_stack_capacity);
    PYRO_FREE_ARRAY(vm, PyroValue, vm->with_stack, vm->with_stack_capacity);
    PYRO_FREE_ARRAY(vm, PyroValue, vm->stack, vm->stack_max - vm->stack);

    PyroStrPool_free(&vm->string_pool, vm);

    assert(vm->bytes_allocated == sizeof(PyroVM));
    free(vm);
}


bool pyro_define_pri_member(PyroVM* vm, PyroMod* module, const char* name, PyroValue value) {
    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }
    PyroValue name_value = pyro_obj(name_string);

    size_t member_index = module->members->count;
    if (!PyroVec_append(module->members, value, vm)) {
        return false;
    }

    if (PyroMap_set(module->all_member_indexes, name_value, pyro_i64(member_index), vm) == 0) {
        module->members->count--;
        return false;
    }

    return true;
}


bool pyro_define_pub_member(PyroVM* vm, PyroMod* module, const char* name, PyroValue value) {
    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }
    PyroValue name_value = pyro_obj(name_string);

    size_t member_index = module->members->count;
    if (!PyroVec_append(module->members, value, vm)) {
        return false;
    }

    if (!PyroMap_set(module->all_member_indexes, name_value, pyro_i64(member_index), vm)) {
        module->members->count--;
        return false;
    }

    if (!PyroMap_set(module->pub_member_indexes, name_value, pyro_i64(member_index), vm)) {
        module->members->count--;
        PyroMap_fast_remove(module->all_member_indexes, name_string, vm);
        return false;
    }

    return true;
}


bool pyro_define_pri_member_fn(PyroVM* vm, PyroMod* module, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_pri_member(vm, module, name, pyro_obj(fn_obj));
}


bool pyro_define_pub_member_fn(PyroVM* vm, PyroMod* module, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_pub_member(vm, module, name, pyro_obj(fn_obj));
}


bool pyro_define_pub_method(PyroVM* vm, PyroClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    if (strlen(name) == 0 || name[0] == '$') {
        return false;
    }

    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }

    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }

    if (!PyroMap_set(class->all_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        return false;
    }

    if (!PyroMap_set(class->pub_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        PyroMap_fast_remove(class->all_instance_methods, name_string, vm);
        return false;
    }

    return true;
}


bool pyro_define_pri_method(PyroVM* vm, PyroClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }

    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }

    if (!PyroMap_set(class->all_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        return false;
    }

    if (name_string == vm->str_dollar_init) {
        class->init_method = pyro_obj(fn_obj);
    }

    return true;
}


bool pyro_define_pub_field(PyroVM* vm, PyroClass* class, const char* name, PyroValue default_value) {
    size_t field_index = class->default_field_values->count;

    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }

    if (!PyroVec_append(class->default_field_values, default_value, vm)) {
        return false;
    }

    if (PyroMap_set(class->all_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        class->default_field_values->count--;
        return false;
    }

    if (PyroMap_set(class->pub_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        class->default_field_values->count--;
        PyroMap_fast_remove(class->all_field_indexes, name_string, vm);
        return false;
    }

    return true;
}


bool pyro_define_pri_field(PyroVM* vm, PyroClass* class, const char* name, PyroValue default_value) {
    size_t field_index = class->default_field_values->count;

    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }

    if (!PyroVec_append(class->default_field_values, default_value, vm)) {
        return false;
    }

    if (PyroMap_set(class->all_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        class->default_field_values->count--;
        return false;
    }

    return true;
}


bool pyro_define_superglobal(PyroVM* vm, const char* name, PyroValue value) {
    PyroStr* name_string = PyroStr_COPY(name);
    if (!name_string) {
        return false;
    }

    if (PyroMap_set(vm->superglobals, pyro_obj(name_string), value, vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_superglobal_fn(PyroVM* vm, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_superglobal(vm, name, pyro_obj(fn_obj));
}


int64_t pyro_get_exit_code(PyroVM* vm) {
    return vm->exit_code;
}


bool pyro_get_exit_flag(PyroVM* vm) {
    return vm->exit_flag;
}


bool pyro_get_panic_flag(PyroVM* vm) {
    return vm->panic_flag;
}


bool pyro_append_arg(PyroVM* vm, char* arg) {
    PyroStr* string = PyroStr_COPY(arg);
    if (!string) {
        return false;
    }

    return PyroVec_append(vm->args, pyro_obj(string), vm);
}


bool pyro_append_args(PyroVM* vm, char** args, size_t arg_count) {
    for (size_t i = 0; i < arg_count; i++) {
        PyroStr* string = PyroStr_COPY(args[i]);
        if (!string) {
            return false;
        }

        if (!PyroVec_append(vm->args, pyro_obj(string), vm)) {
            return false;
        }
    }

    return true;
}


bool pyro_append_import_root(PyroVM* vm, const char* path, size_t path_length)  {
    if (!path) {
        return false;
    }

    PyroStr* string = PyroStr_copy(path, path_length, false, vm);
    if (!string) {
        return false;
    }

    return PyroVec_append(vm->import_roots, pyro_obj(string), vm);
}


void pyro_set_max_memory(PyroVM* vm, size_t bytes) {
    vm->max_bytes = bytes;
}


void pyro_set_repl_flag(PyroVM* vm, bool flag) {
    vm->in_repl = flag;
}

void pyro_set_trace_execution_flag(PyroVM* vm, bool flag) {
    vm->trace_execution = flag;
}
