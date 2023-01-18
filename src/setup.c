#include "../inc/pyro.h"


PyroVM* pyro_new_vm(size_t stack_size) {
    PyroVM* vm = malloc(sizeof(PyroVM));
    if (!vm) {
        return NULL;
    }

    PyroValue* stack = malloc(stack_size);
    if (!stack) {
        free(vm);
        return NULL;
    }
    vm->stack = stack;
    vm->stack_top = stack;
    vm->stack_max = stack + (stack_size/sizeof(PyroValue));
    vm->stack_size = stack_size;

    // Initialize or zero-out all fields before attempting to allocate memory.
    vm->bytes_allocated = sizeof(PyroVM) + stack_size;
    vm->frames = NULL;
    vm->frame_count = 0;
    vm->frame_capacity = 0;
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
    vm->class_char = NULL;
    vm->error = NULL;
    vm->empty_string = NULL;
    vm->exit_code = 0;
    vm->exit_flag = false;
    vm->gc_disallows = 0;
    vm->superglobals = NULL;
    vm->grey_stack_capacity = 0;
    vm->grey_stack_count = 0;
    vm->grey_stack = NULL;
    vm->halt_flag = false;
    vm->import_roots = NULL;
    vm->in_repl = false;
    vm->main_module = NULL;
    vm->max_bytes = SIZE_MAX;
    vm->memory_allocation_failed = false;
    vm->modules = NULL;
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
    vm->str_char = NULL;
    vm->str_class = NULL;
    vm->str_dollar_call = NULL;
    vm->str_dollar_contains = NULL;
    vm->str_dollar_debug = NULL;
    vm->str_dollar_fmt = NULL;
    vm->str_dollar_get_index = NULL;
    vm->str_dollar_hash = NULL;
    vm->str_dollar_init = NULL;
    vm->str_dollar_iter = NULL;
    vm->str_dollar_next = NULL;
    vm->str_dollar_set_index = NULL;
    vm->str_dollar_str = NULL;
    vm->str_err = NULL;
    vm->str_false = NULL;
    vm->str_file = NULL;
    vm->str_fn = NULL;
    vm->str_i64 = NULL;
    vm->str_instance = NULL;
    vm->str_iter = NULL;
    vm->str_map = NULL;
    vm->str_method = NULL;
    vm->str_module = NULL;
    vm->str_null = NULL;
    vm->str_op_binary_equals_equals = NULL;
    vm->str_op_binary_greater = NULL;
    vm->str_op_binary_greater_equals = NULL;
    vm->str_op_binary_less = NULL;
    vm->str_op_binary_less_equals = NULL;
    vm->str_op_binary_minus = NULL;
    vm->str_op_binary_plus = NULL;
    vm->str_op_binary_bar = NULL;
    vm->str_op_binary_amp = NULL;
    vm->str_op_binary_slash = NULL;
    vm->str_op_binary_star = NULL;
    vm->str_op_binary_caret = NULL;
    vm->str_op_binary_percent = NULL;
    vm->str_op_binary_star_star = NULL;
    vm->str_op_binary_slash_slash = NULL;
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
    vm->strings = NULL;
    vm->try_depth = 0;
    vm->with_stack = NULL;
    vm->with_stack_count = 0;
    vm->with_stack_capacity = 0;
    vm->str_dollar_end_with = NULL;
    vm->panic_source_id = NULL;
    vm->panic_line_number = 0;

    // Initialize the [frames] stack.
    vm->frames = PYRO_ALLOCATE_ARRAY(vm, CallFrame, PYRO_INITIAL_CALL_FRAME_CAPACITY);
    if (!vm->frames) {
        pyro_free_vm(vm);
        return NULL;
    }
    vm->frame_capacity = PYRO_INITIAL_CALL_FRAME_CAPACITY;

    // Initialize the MT64 PRNG.
    mt64_init(&vm->mt64);

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
    vm->class_char = PyroClass_new(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // We need to initialize the interned strings pool before we create any strings.
    vm->strings = PyroMap_new_as_weakref(vm);
    if (!vm->strings) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Canned objects.
    vm->empty_string = PyroStr_empty(vm);
    vm->error = PyroErr_new(vm);
    if (!vm->error) {
        pyro_free_vm(vm);
        return NULL;
    }
    vm->error->message = PyroStr_new("error", vm);

    // Static strings.
    vm->str_dollar_init = PyroStr_new("$init", vm);
    vm->str_dollar_str = PyroStr_new("$str", vm);
    vm->str_true = PyroStr_new("true", vm);
    vm->str_false = PyroStr_new("false", vm);
    vm->str_null = PyroStr_new("null", vm);
    vm->str_dollar_fmt = PyroStr_new("$fmt", vm);
    vm->str_dollar_iter = PyroStr_new("$iter", vm);
    vm->str_dollar_next = PyroStr_new("$next", vm);
    vm->str_dollar_get_index = PyroStr_new("$get_index", vm);
    vm->str_dollar_set_index = PyroStr_new("$set_index", vm);
    vm->str_dollar_debug = PyroStr_new("$debug", vm);
    vm->str_op_binary_equals_equals = PyroStr_new("$op_binary_equals_equals", vm);
    vm->str_op_binary_less = PyroStr_new("$op_binary_less", vm);
    vm->str_op_binary_less_equals = PyroStr_new("$op_binary_less_equals", vm);
    vm->str_op_binary_greater = PyroStr_new("$op_binary_greater", vm);
    vm->str_op_binary_greater_equals = PyroStr_new("$op_binary_greater_equals", vm);
    vm->str_op_binary_plus = PyroStr_new("$op_binary_plus", vm);
    vm->str_op_binary_minus = PyroStr_new("$op_binary_minus", vm);
    vm->str_op_binary_bar = PyroStr_new("$op_binary_bar", vm);
    vm->str_op_binary_amp = PyroStr_new("$op_binary_amp", vm);
    vm->str_op_binary_star = PyroStr_new("$op_binary_star", vm);
    vm->str_op_binary_slash = PyroStr_new("$op_binary_slash", vm);
    vm->str_op_binary_caret = PyroStr_new("$op_binary_caret", vm);
    vm->str_op_binary_percent = PyroStr_new("$op_binary_percent", vm);
    vm->str_op_binary_star_star = PyroStr_new("$op_binary_star_star", vm);
    vm->str_op_binary_slash_slash = PyroStr_new("$op_binary_slash_slash", vm);
    vm->str_op_unary_plus = PyroStr_new("$op_unary_plus", vm);
    vm->str_op_unary_minus = PyroStr_new("$op_unary_minus", vm);
    vm->str_dollar_hash = PyroStr_new("$hash", vm);
    vm->str_dollar_call = PyroStr_new("$call", vm);
    vm->str_dollar_contains = PyroStr_new("$contains", vm);
    vm->str_bool = PyroStr_new("bool", vm);
    vm->str_i64 = PyroStr_new("i64", vm);
    vm->str_f64 = PyroStr_new("f64", vm);
    vm->str_char = PyroStr_new("char", vm);
    vm->str_method = PyroStr_new("method", vm);
    vm->str_buf = PyroStr_new("buf", vm);
    vm->str_class = PyroStr_new("class", vm);
    vm->str_fn = PyroStr_new("fn", vm);
    vm->str_instance = PyroStr_new("instance", vm);
    vm->str_file = PyroStr_new("file", vm);
    vm->str_iter = PyroStr_new("iter", vm);
    vm->str_map = PyroStr_new("map", vm);
    vm->str_set = PyroStr_new("set", vm);
    vm->str_vec = PyroStr_new("vec", vm);
    vm->str_source = PyroStr_new("source", vm);
    vm->str_line = PyroStr_new("line", vm);
    vm->str_stack = PyroStr_new("stack", vm);
    vm->str_queue = PyroStr_new("queue", vm);
    vm->str_str = PyroStr_new("str", vm);
    vm->str_module = PyroStr_new("module", vm);
    vm->str_tup = PyroStr_new("tup", vm);
    vm->str_err = PyroStr_new("err", vm);
    vm->str_dollar_end_with = PyroStr_new("$end_with", vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // All other object fields.
    vm->superglobals = PyroMap_new(vm);
    vm->modules = PyroMap_new(vm);
    vm->main_module = PyroMod_new(vm);
    vm->import_roots = PyroVec_new(vm);
    vm->stdout_file = PyroFile_new(vm, stdout);
    vm->stderr_file = PyroFile_new(vm, stderr);
    vm->stdin_file = PyroFile_new(vm, stdin);
    vm->panic_buffer = PyroBuf_new_with_cap(256, vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Load the core standard library -- superglobal functions and builtin types.
    pyro_load_std_builtins(vm);
    pyro_load_std_builtins_map(vm);
    pyro_load_std_builtins_vec(vm);
    pyro_load_std_builtins_tup(vm);
    pyro_load_std_builtins_str(vm);
    pyro_load_std_builtins_buf(vm);
    pyro_load_std_builtins_file(vm);
    pyro_load_std_builtins_iter(vm);
    pyro_load_std_builtins_queue(vm);
    pyro_load_std_builtins_err(vm);
    pyro_load_std_builtins_mod(vm);
    pyro_load_std_builtins_char(vm);

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
    PYRO_FREE_ARRAY(vm, CallFrame, vm->frames, vm->frame_capacity);
    PYRO_FREE_ARRAY(vm, PyroValue, vm->with_stack, vm->with_stack_capacity);

    free(vm->stack);
    vm->bytes_allocated -= vm->stack_size;

    assert(vm->bytes_allocated == sizeof(PyroVM));
    free(vm);
}


PyroMod* pyro_define_module_1(PyroVM* vm, const char* name) {
    PyroStr* name_string = PyroStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }

    PyroMod* module = PyroMod_new(vm);
    if (!module) {
        return NULL;
    }

    if (PyroMap_set(vm->modules, pyro_obj(name_string), pyro_obj(module), vm) == 0) {
        return NULL;
    }

    return module;
}


PyroMod* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name) {
    PyroStr* parent_string = PyroStr_new(parent, vm);
    if (!parent_string) {
        return NULL;
    }

    PyroValue parent_module;
    if (!PyroMap_get(vm->modules, pyro_obj(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroStr* name_string = PyroStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }
    PyroValue name_value = pyro_obj(name_string);

    PyroMod* module_object = PyroMod_new(vm);
    if (!module_object) {
        return NULL;
    }
    PyroValue module_value = pyro_obj(module_object);

    if (PyroMap_set(PYRO_AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


PyroMod* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name) {
    PyroStr* grandparent_string = PyroStr_new(parent, vm);
    if (!grandparent_string) {
        return NULL;
    }

    PyroValue grandparent_module;
    if (!PyroMap_get(vm->modules, pyro_obj(grandparent_string), &grandparent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroStr* parent_string = PyroStr_new(parent, vm);
    if (!parent_string) {
        return NULL;
    }

    PyroValue parent_module;
    if (!PyroMap_get(PYRO_AS_MOD(grandparent_module)->submodules, pyro_obj(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroStr* name_string = PyroStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }
    PyroValue name_value = pyro_obj(name_string);

    PyroMod* module_object = PyroMod_new(vm);
    if (!module_object) {
        return NULL;
    }
    PyroValue module_value = pyro_obj(module_object);

    if (PyroMap_set(PYRO_AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


bool pyro_define_pri_member(PyroVM* vm, PyroMod* module, const char* name, PyroValue value) {
    PyroStr* name_string = PyroStr_new(name, vm);
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
    PyroStr* name_string = PyroStr_new(name, vm);
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

    if (PyroMap_set(module->pub_member_indexes, name_value, pyro_i64(member_index), vm) == 0) {
        PyroMap_remove(module->all_member_indexes, name_value, vm);
        module->members->count--;
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

    PyroStr* name_string = PyroStr_new(name, vm);
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
        PyroMap_remove(class->all_instance_methods, pyro_obj(name_string), vm);
        return false;
    }

    return true;
}


bool pyro_define_pri_method(PyroVM* vm, PyroClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroStr* name_string = PyroStr_new(name, vm);
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

    PyroStr* name_string = PyroStr_new(name, vm);
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
        PyroMap_remove(class->all_field_indexes, pyro_obj(name_string), vm);
        class->default_field_values->count--;
        return false;
    }

    return true;
}


bool pyro_define_pri_field(PyroVM* vm, PyroClass* class, const char* name, PyroValue default_value) {
    size_t field_index = class->default_field_values->count;

    PyroStr* name_string = PyroStr_new(name, vm);
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


bool pyro_define_global(PyroVM* vm, const char* name, PyroValue value) {
    PyroStr* name_string = PyroStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    if (PyroMap_set(vm->superglobals, pyro_obj(name_string), value, vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_global_fn(PyroVM* vm, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroNativeFn* fn_obj = PyroNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_global(vm, name, pyro_obj(fn_obj));
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


bool pyro_set_stderr(PyroVM* vm, FILE* stream) {
    if (stream == NULL) {
        vm->stderr_file = NULL;
        return true;
    }

    PyroFile* file = PyroFile_new(vm, stream);
    if (!file) {
        return false;
    }

    vm->stderr_file = file;
    return true;
}


bool pyro_set_stdout(PyroVM* vm, FILE* stream) {
    if (stream == NULL) {
        vm->stdout_file = NULL;
        return true;
    }

    PyroFile* file = PyroFile_new(vm, stream);
    if (!file) {
        return false;
    }

    vm->stdout_file = file;
    return true;
}


bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args) {
    PyroTup* tup = PyroTup_new(arg_count, vm);
    if (!tup) {
        return false;
    }

    for (size_t i = 0; i < arg_count; i++) {
        PyroStr* string = PyroStr_new(args[i], vm);
        if (!string) {
            return false;
        }
        tup->values[i] = pyro_obj(string);
    }

    pyro_define_global(vm, "$args", pyro_obj(tup));
    return true;
}


bool pyro_add_import_root(PyroVM* vm, const char* path) {
    if (!path) {
        return false;
    }

    PyroStr* string = PyroStr_new(path, vm);
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
