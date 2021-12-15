#include "setup.h"
#include "vm.h"
#include "heap.h"
#include "values.h"
#include "utils.h"

#include "../std/std_core.h"
#include "../std/std_math.h"
#include "../std/std_pyro.h"
#include "../std/std_prng.h"
#include "../std/std_errors.h"
#include "../std/std_path.h"

#include "../lib/os/os.h"
#include "../lib/mt64/mt64.h"


PyroVM* pyro_new_vm() {
    PyroVM* vm = malloc(sizeof(PyroVM));
    if (!vm) {
        return NULL;
    }

    // Initialize or zero-out all fields before attempting to allocate memory. This needs to be
    // done before any objects are allocated or the GC will segfault.
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
    vm->out_file = stdout;
    vm->err_file = stderr;
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

    // Disable garbage collection until the VM has been fully initialized. This is to avoid the
    // possibility of the GC triggering and panicking if it fails to allocate memory for the
    // grey stack. With the GC disabled, we can guarantee that initialization will never panic;
    // the initializer will simply return NULL if sufficient memory cannot be allocated.
    vm->gc_disallows++;

    // Initialize the PRNG.
    vm->mt64 = pyro_mt64_new();
    if (!vm->mt64) {
        pyro_free_vm(vm);
        return NULL;
    }

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
    pyro_load_std_prng(vm);
    pyro_load_std_pyro(vm);
    pyro_load_std_errors(vm);
    pyro_load_std_path(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    vm->gc_disallows--;
    return vm;
}


void pyro_free_vm(PyroVM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        pyro_free_object(vm, object);
        object = next;
    }

    free(vm->grey_stack);
    vm->bytes_allocated -= vm->grey_capacity;

    pyro_mt64_free(vm->mt64);

    assert(vm->bytes_allocated == sizeof(PyroVM));
    free(vm);
}


ObjModule* pyro_define_module_1(PyroVM* vm, const char* name) {
    ObjStr* name_object = STR(name);
    if (!name_object) {
        return NULL;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjModule* module = ObjModule_new(vm);
    if (!module) {
        pyro_pop(vm); // name_value
        return NULL;
    }
    Value module_value = OBJ_VAL(module);
    pyro_push(vm, module_value);

    if (ObjMap_set(vm->modules, name_value, module_value, vm) == 0) {
        pyro_pop(vm); // module_value
        pyro_pop(vm); // name_value
        return NULL;
    }

    pyro_pop(vm); // module_value
    pyro_pop(vm); // name_value
    return module;
}


ObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name) {
    ObjStr* parent_object = STR(parent);
    if (!parent_object) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(vm->modules, OBJ_VAL(parent_object), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_object = STR(name);
    if (!name_object) {
        return NULL;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjModule* module_object = ObjModule_new(vm);
    if (!module_object) {
        pyro_pop(vm); // name_value
        return NULL;
    }
    Value module_value = OBJ_VAL(module_object);
    pyro_push(vm, module_value);

    if (ObjMap_set(AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        pyro_pop(vm); // module_value
        pyro_pop(vm); // name_value
        return NULL;
    }

    if (ObjMap_set(AS_MOD(parent_module)->globals, name_value, module_value, vm) == 0) {
        pyro_pop(vm); // module_value
        pyro_pop(vm); // name_value
        return NULL;
    }

    pyro_pop(vm); // module_value
    pyro_pop(vm); // name_value
    return module_object;
}


ObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name) {
    ObjStr* grandparent_object = STR(parent);
    if (!grandparent_object) {
        return NULL;
    }

    Value grandparent_module;
    if (!ObjMap_get(vm->modules, OBJ_VAL(grandparent_object), &grandparent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* parent_object = STR(parent);
    if (!parent_object) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(AS_MOD(grandparent_module)->submodules, OBJ_VAL(parent_object), &parent_module, vm)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_object = STR(name);
    if (!name_object) {
        return NULL;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjModule* module_object = ObjModule_new(vm);
    if (!module_object) {
        pyro_pop(vm); // name_value
        return NULL;
    }
    Value module_value = OBJ_VAL(module_object);
    pyro_push(vm, module_value);

    if (ObjMap_set(AS_MOD(parent_module)->submodules, name_value, module_value, vm) == 0) {
        pyro_pop(vm); // module_value
        pyro_pop(vm); // name_value
        return NULL;
    }

    if (ObjMap_set(AS_MOD(parent_module)->globals, name_value, module_value, vm) == 0) {
        pyro_pop(vm); // module_value
        pyro_pop(vm); // name_value
        return NULL;
    }

    pyro_pop(vm); // module_value
    pyro_pop(vm); // name_value
    return module_object;
}


bool pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value value) {
    pyro_push(vm, value);

    ObjStr* name_object = STR(name);
    if (!name_object) {
        pyro_pop(vm); // value
        return false;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    if (ObjMap_set(module->globals, name_value, value, vm) == 0) {
        pyro_pop(vm); // name_value
        pyro_pop(vm); // value
        return false;
    }

    pyro_pop(vm); // name_value
    pyro_pop(vm); // value
    return true;
}


bool pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_member(vm, module, name, OBJ_VAL(func_object));
}


bool pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity) {
    ObjStr* name_object = STR(name);
    if (!name_object) {
        return false;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        pyro_pop(vm); // name_value
        return false;
    }
    Value func_value = OBJ_VAL(func_object);
    pyro_push(vm, func_value);

    if (ObjMap_set(class->methods, name_value, func_value, vm) == 0) {
        pyro_pop(vm); // func_value
        pyro_pop(vm); // name_value
        return false;
    }

    pyro_pop(vm); // func_value
    pyro_pop(vm); // name_value
    return true;
}


bool pyro_define_global(PyroVM* vm, const char* name, Value value) {
    pyro_push(vm, value);

    ObjStr* name_object = STR(name);
    if (!name_object) {
        pyro_pop(vm); // value
        return false;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    if (ObjMap_set(vm->globals, name_value, value, vm) == 0) {
        pyro_pop(vm); // name_value
        pyro_pop(vm); // value
        return false;
    }

    pyro_pop(vm); // name_value
    pyro_pop(vm); // value
    return true;
}


bool pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return false;
    }
    return pyro_define_global(vm, name, OBJ_VAL(func_object));
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


void pyro_set_err_file(PyroVM* vm, FILE* file) {
    vm->err_file = file;
}


void pyro_set_out_file(PyroVM* vm, FILE* file) {
    vm->out_file = file;
}


bool pyro_set_args(PyroVM* vm, size_t arg_count, char** args) {
    ObjTup* tup = ObjTup_new(arg_count, vm);
    if (!tup) {
        return false;
    }

    pyro_push(vm, OBJ_VAL(tup));

    for (size_t i = 0; i < arg_count; i++) {
        ObjStr* string = STR(args[i]);
        if (!string) {
            pyro_pop(vm);
            return false;
        }
        tup->values[i] = OBJ_VAL(string);
    }

    pyro_define_global(vm, "$args", OBJ_VAL(tup));
    pyro_pop(vm);
    return true;
}


bool pyro_add_import_root(PyroVM* vm, const char* path) {
    if (!path) {
        return false;
    }
    ObjStr* string = STR(path);
    if (string) {
        pyro_push(vm, OBJ_VAL(string));
        bool result = ObjVec_append(vm->import_roots, OBJ_VAL(string), vm);
        pyro_pop(vm);
        return result;
    }
    return false;
}


void pyro_set_max_memory(PyroVM* vm, size_t bytes) {
    vm->max_bytes = bytes;
}


void pyro_set_repl_flag(PyroVM* vm, bool flag) {
    vm->in_repl = flag;
}
