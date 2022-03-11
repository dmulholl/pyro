#include "setup.h"
#include "vm.h"
#include "heap.h"
#include "values.h"
#include "utils.h"
#include "panics.h"
#include "os.h"

#include "../std/std_lib.h"
#include "../lib/mt64/mt64.h"


PyroVM* pyro_new_vm() {
    PyroVM* vm = malloc(sizeof(PyroVM));
    if (!vm) {
        return NULL;
    }

    // Initialize or zero-out all fields before attempting to allocate memory.
    vm->stack_top = vm->stack;
    vm->stack_max = vm->stack + PYRO_STACK_SIZE;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
    vm->grey_count = 0;
    vm->grey_capacity = 0;
    vm->grey_stack = NULL;
    vm->bytes_allocated = sizeof(PyroVM);
    vm->next_gc_threshold = PYRO_INIT_GC_THRESHOLD;
    vm->exit_flag = false;
    vm->panic_flag = false;
    vm->hard_panic = false;
    vm->halt_flag = false;
    vm->status_code = 0;
    vm->try_depth = 0;
    vm->objects = NULL;
    vm->map_class = NULL;
    vm->str_class = NULL;
    vm->tup_class = NULL;
    vm->vec_class = NULL;
    vm->buf_class = NULL;
    vm->strings = NULL;
    vm->globals = NULL;
    vm->modules = NULL;
    vm->empty_error = NULL;
    vm->empty_string = NULL;
    vm->str_init = NULL;
    vm->str_str = NULL;
    vm->str_true = NULL;
    vm->str_false = NULL;
    vm->str_null = NULL;
    vm->str_fmt = NULL;
    vm->str_iter = NULL;
    vm->str_next = NULL;
    vm->str_get_index = NULL;
    vm->str_set_index = NULL;
    vm->main_module = NULL;
    vm->import_roots = NULL;
    vm->file_class = NULL;
    vm->mt64 = NULL;
    vm->str_debug = NULL;
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
    vm->str_hash = NULL;
    vm->str_call = NULL;
    vm->max_bytes = SIZE_MAX;
    vm->memory_allocation_failed = false;
    vm->gc_disallows = 0;
    vm->iter_class = NULL;
    vm->stack_class = NULL;
    vm->set_class = NULL;
    vm->queue_class = NULL;
    vm->in_repl = false;
    vm->stderr_stream = NULL;
    vm->stdout_stream = NULL;
    vm->stdin_stream = NULL;
    vm->panic_buffer = NULL;
    vm->panic_source_id = NULL;
    vm->panic_line_number = 0;

    // Initialize the MT64 PRNG.
    vm->mt64 = pyro_mt64_new();
    if (!vm->mt64) {
        pyro_free_vm(vm);
        return NULL;
    }
    vm->bytes_allocated += pyro_mt64_size();

    // We need to initialize these classes before we create any objects.
    vm->map_class = ObjClass_new(vm);
    vm->str_class = ObjClass_new(vm);
    vm->tup_class = ObjClass_new(vm);
    vm->vec_class = ObjClass_new(vm);
    vm->buf_class = ObjClass_new(vm);
    vm->file_class = ObjClass_new(vm);
    vm->iter_class = ObjClass_new(vm);
    vm->stack_class = ObjClass_new(vm);
    vm->set_class = ObjClass_new(vm);
    vm->queue_class = ObjClass_new(vm);

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
    vm->str_init = STR("$init");
    vm->str_str = STR("$str");
    vm->str_true = STR("true");
    vm->str_false = STR("false");
    vm->str_null = STR("null");
    vm->str_fmt = STR("$fmt");
    vm->str_iter = STR("$iter");
    vm->str_next = STR("$next");
    vm->str_get_index = STR("$get_index");
    vm->str_set_index = STR("$set_index");
    vm->str_debug = STR("$debug");
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
    vm->str_hash = STR("$hash");
    vm->str_call = STR("$call");

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

    // Load individual standard library modules.
    pyro_load_std_math(vm);
    pyro_load_std_mt64(vm);
    pyro_load_std_prng(vm);
    pyro_load_std_pyro(vm);
    pyro_load_std_errors(vm);
    pyro_load_std_path(vm);
    pyro_load_std_sqlite(vm);

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

    pyro_mt64_free(vm->mt64);
    vm->bytes_allocated -= pyro_mt64_size();

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

    if (ObjMap_set(AS_MOD(parent_module)->globals, name_value, module_value, vm) == 0) {
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

    if (ObjMap_set(AS_MOD(parent_module)->globals, name_value, module_value, vm) == 0) {
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


bool pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_member(vm, module, name, MAKE_OBJ(func_object));
}


bool pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity) {
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

    if (ObjMap_set(class->field_indexes, MAKE_OBJ(name), MAKE_I64(field_index), vm) == 0) {
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


bool pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_global(vm, name, MAKE_OBJ(func_object));
}


int64_t pyro_get_status_code(PyroVM* vm) {
    return vm->status_code;
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
