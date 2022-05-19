#include "setup.h"
#include "vm.h"
#include "heap.h"
#include "values.h"
#include "utils.h"
#include "panics.h"
#include "os.h"

#include "../std/std_lib.h"
#include "../lib/mt64/mt64.h"


PyroVM* pyro_new_vm(size_t stack_size) {
    PyroVM* vm = malloc(sizeof(PyroVM));
    if (!vm) {
        return NULL;
    }

    Value* stack = malloc(stack_size);
    if (!stack) {
        free(vm);
        return NULL;
    }

    vm->stack = stack;
    vm->stack_top = stack;
    vm->stack_max = stack + (stack_size/sizeof(Value));
    vm->stack_size = stack_size;

    // Initialize or zero-out all fields before attempting to allocate memory.
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
    vm->grey_count = 0;
    vm->grey_capacity = 0;
    vm->grey_stack = NULL;
    vm->bytes_allocated = sizeof(PyroVM) + stack_size;
    vm->next_gc_threshold = PYRO_INIT_GC_THRESHOLD;
    vm->halt_flag = false;
    vm->panic_flag = false;
    vm->hard_panic = false;
    vm->exit_flag = false;
    vm->exit_code = 0;
    vm->try_depth = 0;
    vm->objects = NULL;
    vm->class_map = NULL;
    vm->class_str = NULL;
    vm->class_tup = NULL;
    vm->class_vec = NULL;
    vm->class_buf = NULL;
    vm->strings = NULL;
    vm->globals = NULL;
    vm->modules = NULL;
    vm->empty_error = NULL;
    vm->empty_string = NULL;
    vm->str_dollar_init = NULL;
    vm->str_dollar_str = NULL;
    vm->str_true = NULL;
    vm->str_false = NULL;
    vm->str_null = NULL;
    vm->str_dollar_fmt = NULL;
    vm->str_dollar_iter = NULL;
    vm->str_dollar_next = NULL;
    vm->str_dollar_get_index = NULL;
    vm->str_dollar_set_index = NULL;
    vm->main_module = NULL;
    vm->import_roots = NULL;
    vm->class_file = NULL;
    vm->mt64 = NULL;
    vm->str_dollar_debug = NULL;
    vm->str_op_binary_equals_equals = NULL;
    vm->str_op_binary_less = NULL;
    vm->str_op_binary_less_equals = NULL;
    vm->str_op_binary_greater = NULL;
    vm->str_op_binary_greater_equals = NULL;
    vm->str_op_binary_plus = NULL;
    vm->str_op_binary_minus = NULL;
    vm->str_op_binary_star = NULL;
    vm->str_op_binary_slash = NULL;
    vm->str_op_unary_plus = NULL;
    vm->str_op_unary_minus = NULL;
    vm->str_dollar_contains = NULL;
    vm->str_dollar_hash = NULL;
    vm->str_dollar_call = NULL;
    vm->max_bytes = SIZE_MAX;
    vm->memory_allocation_failed = false;
    vm->gc_disallows = 0;
    vm->class_iter = NULL;
    vm->class_stack = NULL;
    vm->class_set = NULL;
    vm->class_queue = NULL;
    vm->in_repl = false;
    vm->stderr_stream = NULL;
    vm->stdout_stream = NULL;
    vm->stdin_stream = NULL;
    vm->panic_buffer = NULL;
    vm->panic_source_id = NULL;
    vm->panic_line_number = 0;
    vm->str_bool = NULL;
    vm->str_i64 = NULL;
    vm->str_char = NULL;
    vm->str_method = NULL;
    vm->str_buf = NULL;
    vm->str_class = NULL;
    vm->str_fn = NULL;
    vm->str_instance = NULL;
    vm->str_file = NULL;
    vm->str_iter = NULL;
    vm->str_map = NULL;
    vm->str_set = NULL;
    vm->str_vec = NULL;
    vm->str_stack = NULL;
    vm->str_queue = NULL;
    vm->str_str = NULL;
    vm->str_module = NULL;
    vm->str_tup = NULL;
    vm->str_err = NULL;

    // Initialize the MT64 PRNG.
    vm->mt64 = pyro_mt64_new();
    if (!vm->mt64) {
        pyro_free_vm(vm);
        return NULL;
    }
    vm->bytes_allocated += pyro_mt64_size();

    // We need to initialize these classes before we create any objects.
    vm->class_map = ObjClass_new(vm);
    vm->class_str = ObjClass_new(vm);
    vm->class_tup = ObjClass_new(vm);
    vm->class_vec = ObjClass_new(vm);
    vm->class_buf = ObjClass_new(vm);
    vm->class_file = ObjClass_new(vm);
    vm->class_iter = ObjClass_new(vm);
    vm->class_stack = ObjClass_new(vm);
    vm->class_set = ObjClass_new(vm);
    vm->class_queue = ObjClass_new(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // We need to initialize the interned strings pool before we create any strings.
    vm->strings = ObjMap_new_as_weakref(vm);
    if (!vm->strings) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Canned objects, mostly static strings.
    vm->empty_error = ObjTup_new_err(0, vm);
    vm->empty_string = ObjStr_empty(vm);
    vm->str_dollar_init = STR("$init");
    vm->str_dollar_str = STR("$str");
    vm->str_true = STR("true");
    vm->str_false = STR("false");
    vm->str_null = STR("null");
    vm->str_dollar_fmt = STR("$fmt");
    vm->str_dollar_iter = STR("$iter");
    vm->str_dollar_next = STR("$next");
    vm->str_dollar_get_index = STR("$get_index");
    vm->str_dollar_set_index = STR("$set_index");
    vm->str_dollar_debug = STR("$debug");
    vm->str_op_binary_equals_equals = STR("$op_binary_equals_equals");
    vm->str_op_binary_less = STR("$op_binary_less");
    vm->str_op_binary_less_equals = STR("$op_binary_less_equals");
    vm->str_op_binary_greater = STR("$op_binary_greater");
    vm->str_op_binary_greater_equals = STR("$op_binary_greater_equals");
    vm->str_op_binary_plus = STR("$op_binary_plus");
    vm->str_op_binary_minus = STR("$op_binary_minus");
    vm->str_op_binary_star = STR("$op_binary_star");
    vm->str_op_binary_slash = STR("$op_binary_slash");
    vm->str_op_unary_plus = STR("$op_unary_plus");
    vm->str_op_unary_minus = STR("$op_unary_minus");
    vm->str_dollar_hash = STR("$hash");
    vm->str_dollar_call = STR("$call");
    vm->str_dollar_contains = STR("$contains");
    vm->str_bool = STR("bool");
    vm->str_i64 = STR("i64");
    vm->str_f64 = STR("f64");
    vm->str_char = STR("char");
    vm->str_method = STR("method");
    vm->str_buf = STR("buf");
    vm->str_class = STR("class");
    vm->str_fn = STR("fn");
    vm->str_instance = STR("instance");
    vm->str_file = STR("file");
    vm->str_iter = STR("iter");
    vm->str_map = STR("map");
    vm->str_set = STR("set");
    vm->str_vec = STR("vec");
    vm->str_stack = STR("stack");
    vm->str_queue = STR("queue");
    vm->str_str = STR("str");
    vm->str_module = STR("module");
    vm->str_tup = STR("tup");
    vm->str_err = STR("err");

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // All other object fields.
    vm->globals = ObjMap_new(vm);
    vm->modules = ObjMap_new(vm);
    vm->main_module = ObjModule_new(vm);
    vm->import_roots = ObjVec_new(vm);
    vm->stdout_stream = (Obj*)ObjFile_new(vm, stdout);
    vm->stderr_stream = (Obj*)ObjFile_new(vm, stderr);
    vm->stdin_stream = (Obj*)ObjFile_new(vm, stdin);
    vm->panic_buffer = ObjBuf_new_with_cap(256, vm);

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

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    return vm;
}


void pyro_free_vm(PyroVM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        pyro_free_object(vm, object);
        object = next;
    }

    FREE_ARRAY(vm, Obj*, vm->grey_stack, vm->grey_capacity);

    if (vm->mt64) {
        pyro_mt64_free(vm->mt64);
        vm->bytes_allocated -= pyro_mt64_size();
    }

    free(vm->stack);
    vm->bytes_allocated -= vm->stack_size;

    assert(vm->bytes_allocated == sizeof(PyroVM));
    free(vm);
}


ObjModule* pyro_define_module_1(PyroVM* vm, const char* name) {
    ObjStr* name_string = STR(name);
    if (!name_string) {
        return NULL;
    }

    ObjModule* module = ObjModule_new(vm);
    if (!module) {
        return NULL;
    }

    if (ObjMap_set(vm->modules, MAKE_OBJ(name_string), MAKE_OBJ(module), vm) == 0) {
        return NULL;
    }

    return module;
}


ObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name) {
    ObjStr* parent_string = STR(parent);
    if (!parent_string) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(vm->modules, MAKE_OBJ(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_string = STR(name);
    if (!name_string) {
        return NULL;
    }
    Value name_value = MAKE_OBJ(name_string);

    ObjModule* module_object = ObjModule_new(vm);
    if (!module_object) {
        return NULL;
    }
    Value module_value = MAKE_OBJ(module_object);

    if (ObjMap_set(AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


ObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name) {
    ObjStr* grandparent_string = STR(parent);
    if (!grandparent_string) {
        return NULL;
    }

    Value grandparent_module;
    if (!ObjMap_get(vm->modules, MAKE_OBJ(grandparent_string), &grandparent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* parent_string = STR(parent);
    if (!parent_string) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(AS_MOD(grandparent_module)->submodules, MAKE_OBJ(parent_string), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_string = STR(name);
    if (!name_string) {
        return NULL;
    }
    Value name_value = MAKE_OBJ(name_string);

    ObjModule* module_object = ObjModule_new(vm);
    if (!module_object) {
        return NULL;
    }
    Value module_value = MAKE_OBJ(module_object);

    if (ObjMap_set(AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        return NULL;
    }

    return module_object;
}


bool pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value value) {
    ObjStr* name_string = STR(name);
    if (!name_string) {
        return false;
    }

    if (ObjMap_set(module->globals, MAKE_OBJ(name_string), value, vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_member(vm, module, name, MAKE_OBJ(func_object));
}


bool pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    ObjStr* name_string = STR(name);
    if (!name_string) {
        return false;
    }

    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }

    if (ObjMap_set(class->methods, MAKE_OBJ(name_string), MAKE_OBJ(func_object), vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_field(PyroVM* vm, ObjClass* class, const char* name, Value default_value) {
    size_t field_index = class->field_values->count;

    ObjStr* name_string = STR(name);
    if (!name_string) {
        return false;
    }

    if (!ObjVec_append(class->field_values, default_value, vm)) {
        return false;
    }

    if (ObjMap_set(class->field_indexes, MAKE_OBJ(name_string), MAKE_I64(field_index), vm) == 0) {
        class->field_values->count--;
        return false;
    }

    return true;
}


bool pyro_define_global(PyroVM* vm, const char* name, Value value) {
    ObjStr* name_string = STR(name);
    if (!name_string) {
        return false;
    }

    if (ObjMap_set(vm->globals, MAKE_OBJ(name_string), value, vm) == 0) {
        return false;
    }

    return true;
}


bool pyro_define_global_fn(PyroVM* vm, const char* name, pyro_native_fn_t fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_global(vm, name, MAKE_OBJ(func_object));
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


bool pyro_get_hard_panic_flag(PyroVM* vm) {
    return vm->hard_panic;
}


bool pyro_set_stderr(PyroVM* vm, FILE* stream) {
    if (stream == NULL) {
        vm->stderr_stream = NULL;
        return true;
    }

    ObjFile* file = ObjFile_new(vm, stream);
    if (!file) {
        return false;
    }

    vm->stderr_stream = (Obj*)file;
    return true;
}


bool pyro_set_stdout(PyroVM* vm, FILE* stream) {
    if (stream == NULL) {
        vm->stdout_stream = NULL;
        return true;
    }

    ObjFile* file = ObjFile_new(vm, stream);
    if (!file) {
        return false;
    }

    vm->stdout_stream = (Obj*)file;
    return true;
}


bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args) {
    ObjTup* tup = ObjTup_new(arg_count, vm);
    if (!tup) {
        return false;
    }

    for (size_t i = 0; i < arg_count; i++) {
        ObjStr* string = STR(args[i]);
        if (!string) {
            return false;
        }
        tup->values[i] = MAKE_OBJ(string);
    }

    pyro_define_global(vm, "$args", MAKE_OBJ(tup));
    return true;
}


bool pyro_add_import_root(PyroVM* vm, const char* path) {
    if (!path) {
        return false;
    }

    ObjStr* string = STR(path);
    if (!string) {
        return false;
    }

    return ObjVec_append(vm->import_roots, MAKE_OBJ(string), vm);
}


void pyro_set_max_memory(PyroVM* vm, size_t bytes) {
    vm->max_bytes = bytes;
}


void pyro_set_repl_flag(PyroVM* vm, bool flag) {
    vm->in_repl = flag;
}
