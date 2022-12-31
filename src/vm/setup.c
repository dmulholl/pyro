#include "../inc/setup.h"
#include "../inc/vm.h"
#include "../inc/heap.h"
#include "../inc/values.h"
#include "../inc/utils.h"
#include "../inc/panics.h"
#include "../inc/os.h"
#include "../inc/std_lib.h"
#include "../../lib/mt64/mt64.h"


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
    vm->class_buf = PyroObjClass_new(vm);
    vm->class_err = PyroObjClass_new(vm);
    vm->class_file = PyroObjClass_new(vm);
    vm->class_iter = PyroObjClass_new(vm);
    vm->class_map = PyroObjClass_new(vm);
    vm->class_queue = PyroObjClass_new(vm);
    vm->class_set = PyroObjClass_new(vm);
    vm->class_stack = PyroObjClass_new(vm);
    vm->class_str = PyroObjClass_new(vm);
    vm->class_tup = PyroObjClass_new(vm);
    vm->class_vec = PyroObjClass_new(vm);
    vm->class_module = PyroObjClass_new(vm);
    vm->class_char = PyroObjClass_new(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // We need to initialize the interned strings pool before we create any strings.
    vm->strings = PyroObjMap_new_as_weakref(vm);
    if (!vm->strings) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Canned objects.
    vm->empty_string = PyroObjStr_empty(vm);
    vm->error = PyroObjErr_new(vm);
    if (!vm->error) {
        pyro_free_vm(vm);
        return NULL;
    }
    vm->error->message = PyroObjStr_new("error", vm);

    // Static strings.
    vm->str_dollar_init = PyroObjStr_new("$init", vm);
    vm->str_dollar_str = PyroObjStr_new("$str", vm);
    vm->str_true = PyroObjStr_new("true", vm);
    vm->str_false = PyroObjStr_new("false", vm);
    vm->str_null = PyroObjStr_new("null", vm);
    vm->str_dollar_fmt = PyroObjStr_new("$fmt", vm);
    vm->str_dollar_iter = PyroObjStr_new("$iter", vm);
    vm->str_dollar_next = PyroObjStr_new("$next", vm);
    vm->str_dollar_get_index = PyroObjStr_new("$get_index", vm);
    vm->str_dollar_set_index = PyroObjStr_new("$set_index", vm);
    vm->str_dollar_debug = PyroObjStr_new("$debug", vm);
    vm->str_op_binary_equals_equals = PyroObjStr_new("$op_binary_equals_equals", vm);
    vm->str_op_binary_less = PyroObjStr_new("$op_binary_less", vm);
    vm->str_op_binary_less_equals = PyroObjStr_new("$op_binary_less_equals", vm);
    vm->str_op_binary_greater = PyroObjStr_new("$op_binary_greater", vm);
    vm->str_op_binary_greater_equals = PyroObjStr_new("$op_binary_greater_equals", vm);
    vm->str_op_binary_plus = PyroObjStr_new("$op_binary_plus", vm);
    vm->str_op_binary_minus = PyroObjStr_new("$op_binary_minus", vm);
    vm->str_op_binary_bar = PyroObjStr_new("$op_binary_bar", vm);
    vm->str_op_binary_amp = PyroObjStr_new("$op_binary_amp", vm);
    vm->str_op_binary_star = PyroObjStr_new("$op_binary_star", vm);
    vm->str_op_binary_slash = PyroObjStr_new("$op_binary_slash", vm);
    vm->str_op_binary_caret = PyroObjStr_new("$op_binary_caret", vm);
    vm->str_op_binary_percent = PyroObjStr_new("$op_binary_percent", vm);
    vm->str_op_binary_star_star = PyroObjStr_new("$op_binary_star_star", vm);
    vm->str_op_binary_slash_slash = PyroObjStr_new("$op_binary_slash_slash", vm);
    vm->str_op_unary_plus = PyroObjStr_new("$op_unary_plus", vm);
    vm->str_op_unary_minus = PyroObjStr_new("$op_unary_minus", vm);
    vm->str_dollar_hash = PyroObjStr_new("$hash", vm);
    vm->str_dollar_call = PyroObjStr_new("$call", vm);
    vm->str_dollar_contains = PyroObjStr_new("$contains", vm);
    vm->str_bool = PyroObjStr_new("bool", vm);
    vm->str_i64 = PyroObjStr_new("i64", vm);
    vm->str_f64 = PyroObjStr_new("f64", vm);
    vm->str_char = PyroObjStr_new("char", vm);
    vm->str_method = PyroObjStr_new("method", vm);
    vm->str_buf = PyroObjStr_new("buf", vm);
    vm->str_class = PyroObjStr_new("class", vm);
    vm->str_fn = PyroObjStr_new("fn", vm);
    vm->str_instance = PyroObjStr_new("instance", vm);
    vm->str_file = PyroObjStr_new("file", vm);
    vm->str_iter = PyroObjStr_new("iter", vm);
    vm->str_map = PyroObjStr_new("map", vm);
    vm->str_set = PyroObjStr_new("set", vm);
    vm->str_vec = PyroObjStr_new("vec", vm);
    vm->str_source = PyroObjStr_new("source", vm);
    vm->str_line = PyroObjStr_new("line", vm);
    vm->str_stack = PyroObjStr_new("stack", vm);
    vm->str_queue = PyroObjStr_new("queue", vm);
    vm->str_str = PyroObjStr_new("str", vm);
    vm->str_module = PyroObjStr_new("module", vm);
    vm->str_tup = PyroObjStr_new("tup", vm);
    vm->str_err = PyroObjStr_new("err", vm);
    vm->str_dollar_end_with = PyroObjStr_new("$end_with", vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // All other object fields.
    vm->superglobals = PyroObjMap_new(vm);
    vm->modules = PyroObjMap_new(vm);
    vm->main_module = PyroObjModule_new(vm);
    vm->import_roots = PyroObjVec_new(vm);
    vm->stdout_file = PyroObjFile_new(vm, stdout);
    vm->stderr_file = PyroObjFile_new(vm, stderr);
    vm->stdin_file = PyroObjFile_new(vm, stdin);
    vm->panic_buffer = PyroObjBuf_new_with_cap(256, vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Load the core standard library -- global functions and builtin types.
    pyro_load_std_core(vm);
    pyro_load_std_core_map(vm);
    pyro_load_std_core_vec(vm);
    pyro_load_std_core_tup(vm);
    pyro_load_std_core_str(vm);
    pyro_load_std_core_buf(vm);
    pyro_load_std_core_file(vm);
    pyro_load_std_core_iter(vm);
    pyro_load_std_core_queue(vm);
    pyro_load_std_core_err(vm);
    pyro_load_std_core_mod(vm);
    pyro_load_std_core_char(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    return vm;
}


void pyro_free_vm(PyroVM* vm) {
    PyroObj* object = vm->objects;
    while (object != NULL) {
        PyroObj* next = object->next;
        pyro_free_object(vm, object);
        object = next;
    }

    PYRO_FREE_ARRAY(vm, PyroObj*, vm->grey_stack, vm->grey_stack_capacity);
    PYRO_FREE_ARRAY(vm, CallFrame, vm->frames, vm->frame_capacity);
    PYRO_FREE_ARRAY(vm, PyroValue, vm->with_stack, vm->with_stack_capacity);

    free(vm->stack);
    vm->bytes_allocated -= vm->stack_size;

    assert(vm->bytes_allocated == sizeof(PyroVM));
    free(vm);
}


PyroObjModule* pyro_define_module_1(PyroVM* vm, const char* name) {
    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }

    PyroObjModule* module = PyroObjModule_new(vm);
    if (!module) {
        return NULL;
    }

    if (PyroObjMap_set(vm->modules, pyro_obj(name_string), pyro_obj(module), vm) == 0) {
        return NULL;
    }

    return module;
}


PyroObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name) {
    PyroObjStr* parent_string = PyroObjStr_new(parent, vm);
    if (!parent_string) {
        return NULL;
    }

    PyroValue parent_module;
    if (!PyroObjMap_get(vm->modules, pyro_obj(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }
    PyroValue name_value = pyro_obj(name_string);

    PyroObjModule* module_object = PyroObjModule_new(vm);
    if (!module_object) {
        return NULL;
    }
    PyroValue module_value = pyro_obj(module_object);

    if (PyroObjMap_set(PYRO_AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


PyroObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name) {
    PyroObjStr* grandparent_string = PyroObjStr_new(parent, vm);
    if (!grandparent_string) {
        return NULL;
    }

    PyroValue grandparent_module;
    if (!PyroObjMap_get(vm->modules, pyro_obj(grandparent_string), &grandparent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroObjStr* parent_string = PyroObjStr_new(parent, vm);
    if (!parent_string) {
        return NULL;
    }

    PyroValue parent_module;
    if (!PyroObjMap_get(PYRO_AS_MOD(grandparent_module)->submodules, pyro_obj(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return NULL;
    }
    PyroValue name_value = pyro_obj(name_string);

    PyroObjModule* module_object = PyroObjModule_new(vm);
    if (!module_object) {
        return NULL;
    }
    PyroValue module_value = pyro_obj(module_object);

    if (PyroObjMap_set(PYRO_AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


bool pyro_define_pri_member(PyroVM* vm, PyroObjModule* module, const char* name, PyroValue value) {
    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }
    PyroValue name_value = pyro_obj(name_string);

    size_t member_index = module->members->count;
    if (!PyroObjVec_append(module->members, value, vm)) {
        return false;
    }

    if (PyroObjMap_set(module->all_member_indexes, name_value, pyro_i64(member_index), vm) == 0) {
        module->members->count--;
        return false;
    }

    return true;
}


bool pyro_define_pub_member(PyroVM* vm, PyroObjModule* module, const char* name, PyroValue value) {
    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }
    PyroValue name_value = pyro_obj(name_string);

    size_t member_index = module->members->count;
    if (!PyroObjVec_append(module->members, value, vm)) {
        return false;
    }

    if (PyroObjMap_set(module->all_member_indexes, name_value, pyro_i64(member_index), vm) == 0) {
        module->members->count--;
        return false;
    }

    if (PyroObjMap_set(module->pub_member_indexes, name_value, pyro_i64(member_index), vm) == 0) {
        PyroObjMap_remove(module->all_member_indexes, name_value, vm);
        module->members->count--;
        return false;
    }

    return true;
}


bool pyro_define_pri_member_fn(PyroVM* vm, PyroObjModule* module, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroObjNativeFn* fn_obj = PyroObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_pri_member(vm, module, name, pyro_obj(fn_obj));
}


bool pyro_define_pub_member_fn(PyroVM* vm, PyroObjModule* module, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroObjNativeFn* fn_obj = PyroObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }
    return pyro_define_pub_member(vm, module, name, pyro_obj(fn_obj));
}


bool pyro_define_pub_method(PyroVM* vm, PyroObjClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    if (strlen(name) == 0 || name[0] == '$') {
        return false;
    }

    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    PyroObjNativeFn* fn_obj = PyroObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }

    if (!PyroObjMap_set(class->all_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        return false;
    }

    if (!PyroObjMap_set(class->pub_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        PyroObjMap_remove(class->all_instance_methods, pyro_obj(name_string), vm);
        return false;
    }

    return true;
}


bool pyro_define_pri_method(PyroVM* vm, PyroObjClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    PyroObjNativeFn* fn_obj = PyroObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!fn_obj) {
        return false;
    }

    if (!PyroObjMap_set(class->all_instance_methods, pyro_obj(name_string), pyro_obj(fn_obj), vm)) {
        return false;
    }

    if (name_string == vm->str_dollar_init) {
        class->init_method = pyro_obj(fn_obj);
    }

    return true;
}


bool pyro_define_pub_field(PyroVM* vm, PyroObjClass* class, const char* name, PyroValue default_value) {
    size_t field_index = class->default_field_values->count;

    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    if (!PyroObjVec_append(class->default_field_values, default_value, vm)) {
        return false;
    }

    if (PyroObjMap_set(class->all_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        class->default_field_values->count--;
        return false;
    }

    if (PyroObjMap_set(class->pub_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        PyroObjMap_remove(class->all_field_indexes, pyro_obj(name_string), vm);
        class->default_field_values->count--;
        return false;
    }

    return true;
}


bool pyro_define_pri_field(PyroVM* vm, PyroObjClass* class, const char* name, PyroValue default_value) {
    size_t field_index = class->default_field_values->count;

    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    if (!PyroObjVec_append(class->default_field_values, default_value, vm)) {
        return false;
    }

    if (PyroObjMap_set(class->all_field_indexes, pyro_obj(name_string), pyro_i64(field_index), vm) == 0) {
        class->default_field_values->count--;
        return false;
    }

    return true;
}


bool pyro_define_global(PyroVM* vm, const char* name, PyroValue value) {
    PyroObjStr* name_string = PyroObjStr_new(name, vm);
    if (!name_string) {
        return false;
    }

    if (PyroObjMap_set(vm->superglobals, pyro_obj(name_string), value, vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_global_fn(PyroVM* vm, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    PyroObjNativeFn* fn_obj = PyroObjNativeFn_new(vm, fn_ptr, name, arity);
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

    PyroObjFile* file = PyroObjFile_new(vm, stream);
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

    PyroObjFile* file = PyroObjFile_new(vm, stream);
    if (!file) {
        return false;
    }

    vm->stdout_file = file;
    return true;
}


bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args) {
    PyroObjTup* tup = PyroObjTup_new(arg_count, vm);
    if (!tup) {
        return false;
    }

    for (size_t i = 0; i < arg_count; i++) {
        PyroObjStr* string = PyroObjStr_new(args[i], vm);
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

    PyroObjStr* string = PyroObjStr_new(path, vm);
    if (!string) {
        return false;
    }

    return PyroObjVec_append(vm->import_roots, pyro_obj(string), vm);
}


void pyro_set_max_memory(PyroVM* vm, size_t bytes) {
    vm->max_bytes = bytes;
}


void pyro_set_repl_flag(PyroVM* vm, bool flag) {
    vm->in_repl = flag;
}
