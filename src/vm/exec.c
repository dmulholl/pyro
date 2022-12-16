#include "../inc/exec.h"
#include "../inc/vm.h"
#include "../inc/opcodes.h"
#include "../inc/debug.h"
#include "../inc/compiler.h"
#include "../inc/heap.h"
#include "../inc/values.h"
#include "../inc/utils.h"
#include "../inc/operators.h"
#include "../inc/setup.h"
#include "../inc/stringify.h"
#include "../inc/panics.h"
#include "../inc/imports.h"
#include "../inc/os.h"
#include "../inc/io.h"
#include "../inc/gc.h"


// Pushes a new frame onto the call stack. [frame_pointer] points to the frame's zeroth
// local variable (for instance methods, this will be 'self').
static void push_call_frame(PyroVM* vm, ObjClosure* closure, Value* frame_pointer) {
    if (vm->frame_count == vm->frame_capacity) {
        size_t new_capacity = GROW_CAPACITY(vm->frame_capacity);
        CallFrame* new_array = REALLOCATE_ARRAY(vm, CallFrame, vm->frames, vm->frame_capacity, new_capacity);
        if (!new_array) {
            pyro_panic(vm, "out of memory");
            return;
        }
        vm->frames = new_array;
        vm->frame_capacity = new_capacity;
    }

    CallFrame* frame = &vm->frames[vm->frame_count];
    vm->frame_count++;

    frame->closure = closure;
    frame->ip = closure->fn->code;
    frame->fp = frame_pointer;
    frame->with_stack_count_on_entry = vm->with_stack_count;
}


// - If we're calling [closure] as a function, the [closure] object and [arg_count] arguments should
//   be sitting on top of the stack.
// - If we're calling [closure] as a method, the receiver object and [arg_count] arguments should be
//   sitting on top of the stack.
static void call_closure(PyroVM* vm, ObjClosure* closure, uint8_t arg_count) {
    Value* frame_pointer = vm->stack_top - arg_count - 1;

    if (closure->fn->is_variadic) {
        size_t num_required_args = closure->fn->arity - 1;
        if (arg_count < num_required_args) {
            pyro_panic(vm,
                "%s(): expected at least %zu argument%s, found %zu",
                closure->fn->name->bytes,
                num_required_args,
                num_required_args == 1 ? "" : "s",
                arg_count
            );
            return;
        }
        size_t num_variadic_args = arg_count - num_required_args;
        ObjTup* tup = ObjTup_new(num_variadic_args, vm);
        if (!tup) {
            pyro_panic(vm, "out of memory");
            return;
        }
        if (num_variadic_args > 0) {
            memcpy(tup->values, vm->stack_top - num_variadic_args, sizeof(Value) * num_variadic_args);
            vm->stack_top -= num_variadic_args;
        }
        pyro_push(vm, pyro_make_obj(tup));
        push_call_frame(vm, closure, frame_pointer);
        return;
    }

    if (arg_count == closure->fn->arity) {
        push_call_frame(vm, closure, frame_pointer);
        return;
    }

    if (arg_count < closure->fn->arity && arg_count + closure->default_values->count >= closure->fn->arity) {
        size_t num_args_to_push = closure->fn->arity - arg_count;
        size_t start_index = closure->default_values->count - num_args_to_push;
        for (size_t i = 0; i < num_args_to_push; i++) {
            Value arg = closure->default_values->values[start_index + i];
            pyro_push(vm, arg);
        }
        push_call_frame(vm, closure, frame_pointer);
        return;
    }

    pyro_panic(vm,
        "%s(): expected %zu argument%s, found %zu",
        closure->fn->name->bytes,
        closure->fn->arity,
        closure->fn->arity == 1 ? "" : "s",
        arg_count
    );
}


// - If we're calling [fn] as a function, the [fn] object and [arg_count] arguments should
//   be sitting on top of the stack.
// - If we're calling [fn] as a method, the receiver object and [arg_count] arguments should be
//   sitting on top of the stack.
static void call_native_fn(PyroVM* vm, ObjNativeFn* fn, uint8_t arg_count) {
    if (fn->arity == arg_count || fn->arity == -1) {
        Value result = fn->fn_ptr(vm, arg_count, vm->stack_top - arg_count);
        *(vm->stack_top - arg_count - 1) = result;
        vm->stack_top -= arg_count;
        return;
    }
    pyro_panic(vm,
        "%s(): expected %zu argument%s, found %zu",
        fn->name->bytes,
        fn->arity,
        fn->arity == 1 ? "" : "s",
        arg_count
    );
}


// The value to be called and [arg_count] arguments should be sitting on top of the stack.
static void call_value(PyroVM* vm, uint8_t arg_count) {
    Value callee = pyro_peek(vm, arg_count);

    if (!IS_OBJ(callee)) {
        pyro_panic(vm, "value is not callable");
        return;
    }

    switch(AS_OBJ(callee)->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bm = AS_BOUND_METHOD(callee);
            vm->stack_top[-arg_count - 1] = bm->receiver;

            switch (bm->method->type) {
                case OBJ_CLOSURE:
                    call_closure(vm, (ObjClosure*)bm->method, arg_count);
                    break;

                case OBJ_NATIVE_FN:
                    call_native_fn(vm, (ObjNativeFn*)bm->method, arg_count);
                    break;

                default:
                    pyro_panic(vm, "invalid type for bound method");
                    break;
            }

            return;
        }

        case OBJ_CLASS: {
            ObjClass* class = AS_CLASS(callee);
            ObjInstance* instance = ObjInstance_new(vm, class);
            if (!instance) {
                pyro_panic(vm, "out of memory");
                return;
            }
            vm->stack_top[-arg_count - 1] = pyro_make_obj(instance);

            if (IS_NULL(class->init_method)) {
                if (arg_count > 0) {
                    pyro_panic(vm,
                        "%s(): expected 0 arguments for initializer, found %zu",
                        class->name->bytes,
                        arg_count
                    );
                }
                return;
            }

            switch (AS_OBJ(class->init_method)->type) {
                case OBJ_CLOSURE:
                    call_closure(vm, AS_CLOSURE(class->init_method), arg_count);
                    break;

                case OBJ_NATIVE_FN:
                    call_native_fn(vm, AS_NATIVE_FN(class->init_method), arg_count);
                    break;

                default:
                    pyro_panic(vm, "invalid type for init method");
                    break;
            }

            return;
        }

        case OBJ_CLOSURE: {
            call_closure(vm, AS_CLOSURE(callee), arg_count);
            return;
        }

        case OBJ_NATIVE_FN: {
            call_native_fn(vm, AS_NATIVE_FN(callee), arg_count);
            return;
        }

        case OBJ_INSTANCE: {
            ObjClass* class = AS_OBJ(callee)->class;

            Value call_method;
            if (!ObjMap_get(class->all_instance_methods, pyro_make_obj(vm->str_dollar_call), &call_method, vm)) {
                pyro_panic(vm, "object is not callable");
                return;
            }

            switch (AS_OBJ(call_method)->type) {
                case OBJ_CLOSURE:
                    call_closure(vm, AS_CLOSURE(call_method), arg_count);
                    break;

                case OBJ_NATIVE_FN:
                    call_native_fn(vm, AS_NATIVE_FN(call_method), arg_count);
                    break;

                default:
                    pyro_panic(vm, "invalid type for $call method");
                    break;
            }

            return;
        }

        default:
            break;
    }

    pyro_panic(vm, "value is not callable");
}


static ObjUpvalue* capture_upvalue(PyroVM* vm, Value* local) {
    // Before creating a new upvalue object, look for an existing one in the list of open upvalues.
    ObjUpvalue* prev_upvalue = NULL;
    ObjUpvalue* curr_upvalue = vm->open_upvalues;

    // The list is ordered with upvalues pointing to higher stack slots coming first.
    // We fast-forward through the list to the location of the possible match.
    while (curr_upvalue != NULL && curr_upvalue->location > local) {
        prev_upvalue = curr_upvalue;
        curr_upvalue = curr_upvalue->next;
    }
    if (curr_upvalue != NULL && curr_upvalue->location == local) {
        return curr_upvalue;
    }

    // No match so create a new upvalue object.
    ObjUpvalue* new_upvalue = ObjUpvalue_new(vm, local);
    if (!new_upvalue) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    // Insert the new upvalue into the linked list at the right location.
    new_upvalue->next = curr_upvalue;
    if (prev_upvalue == NULL) {
        vm->open_upvalues = new_upvalue;
    } else {
        prev_upvalue->next = new_upvalue;
    }

    return new_upvalue;
}


// This function closes every open upvalue it can find that points to the specified stack slot
// or to any slot above it on the stack.
static void close_upvalues(PyroVM* vm, Value* addr) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= addr) {
        ObjUpvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}


// Attempts to load the module identified by the [names] array. If the module has already been
// imported, this will return the cached module object. Otherwise, it will attempt to import,
// execute, and return the module. Note that this function will import ancestor modules along the
// [names] path if they haven't already been imported, i.e. if the path is 'foo::bar::baz' this
// function will first import 'foo' then 'foo::bar' then 'foo::bar::baz', returning 'baz'. This
// function can call into Pyro code and can set the panic or exit flags.
static ObjModule* load_module(PyroVM* vm, Value* names, size_t name_count) {
    ObjMap* supermod_modules_map = vm->modules;
    Value module_value;

    for (uint8_t i = 0; i < name_count; i++) {
        Value name = names[i];

        if (ObjMap_get(supermod_modules_map, name, &module_value, vm)) {
            supermod_modules_map = AS_MOD(module_value)->submodules;
            continue;
        }

        ObjModule* module_object = ObjModule_new(vm);
        if (!module_object) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        module_value = pyro_make_obj(module_object);
        if (ObjMap_set(supermod_modules_map, name, module_value, vm) == 0) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        pyro_import_module(vm, i + 1, names, module_object);
        if (vm->halt_flag) {
            ObjMap_remove(supermod_modules_map, name, vm);
            return NULL;
        }

        supermod_modules_map = module_object->submodules;
    }

    return AS_MOD(module_value);
}


void call_end_with_method(PyroVM* vm, Value receiver) {
    Value end_with_method = pyro_get_method(vm, receiver, vm->str_dollar_end_with);
    if (IS_NULL(end_with_method)) {
        return;
    }

    if (!pyro_push(vm, receiver)) {
        return;
    }

    bool stashed_halt_flag = vm->halt_flag;
    bool stashed_exit_flag = vm->exit_flag;
    bool stashed_panic_flag = vm->panic_flag;

    vm->halt_flag = false;
    vm->exit_flag = false;
    vm->panic_flag = false;

    pyro_call_method(vm, end_with_method, 0);
    bool had_exit = vm->exit_flag;
    bool had_panic = vm->panic_flag;

    vm->halt_flag = stashed_halt_flag;
    vm->exit_flag = stashed_exit_flag;
    vm->panic_flag = stashed_panic_flag;

    if (had_exit) {
        vm->halt_flag = true;
        vm->exit_flag = true;
    }

    if (had_panic) {
        vm->halt_flag = true;
        vm->panic_flag = true;
    }
}


static void run(PyroVM* vm) {
    size_t with_stack_count_on_entry = vm->with_stack_count;
    size_t frame_count_on_entry = vm->frame_count;
    assert(frame_count_on_entry >= 1);

    // Reads the next byte from the bytecode as a uint8_t value.
    #define READ_BYTE() (*frame->ip++)

    // Reads the next two bytes from the bytecode as a big-endian uint16_t value.
    #define READ_BE_U16() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

    // Reads the next two bytes from the bytecode as an index into the function's constant table.
    // Returns the constant as a Value.
    #define READ_CONSTANT() (frame->closure->fn->constants[READ_BE_U16()])

    // Reads the next two bytes from the bytecode as an index into the function's constant table
    // referencing a string value. Returns the value as an ObjStr*.
    #define READ_STRING() AS_STR(READ_CONSTANT())

    for (;;) {
        if (vm->halt_flag || vm->frame_count < frame_count_on_entry) {
            break;
        }

        // The last instruction may have changed the frame count or (this can lead to nasty bugs)
        // forced a reallocation of the frame stack so reset the frame pointer for every iteration.
        CallFrame* frame = &vm->frames[vm->frame_count - 1];

        #ifdef PYRO_DEBUG_STRESS_GC
            pyro_collect_garbage(vm);
            if (vm->halt_flag) {
                break;
            }
        #else
            if (vm->bytes_allocated > vm->next_gc_threshold) {
                pyro_collect_garbage(vm);
                if (vm->halt_flag) {
                    break;
                }
            }
        #endif

        #ifdef PYRO_DEBUG_TRACE_EXECUTION
            pyro_stdout_write(vm, "             ");
            if (vm->stack == vm->stack_top) {
                pyro_stdout_write(vm, "[ empty ]");
            }
            for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
                pyro_stdout_write(vm, "[ ");
                pyro_dump_value(vm, *slot);
                pyro_stdout_write(vm, " ]");
            }
            pyro_stdout_write(vm, "\n");
            size_t ip = frame->ip - frame->closure->fn->code;
            pyro_disassemble_instruction(vm, frame->closure->fn, ip);
        #endif

        switch (READ_BYTE()) {
            case OP_BINARY_PLUS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_plus(vm, a, b));
                break;
            }

            case OP_ASSERT: {
                Value test_expr = pyro_pop(vm);
                if (!pyro_is_truthy(test_expr)) {
                    pyro_panic(vm, "assertion failed");
                }
                break;
            }

            case OP_BINARY_AMP: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_amp(vm, a, b));
                break;
            }

            case OP_BINARY_BAR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_bar(vm, a, b));
                break;
            }

            case OP_UNARY_TILDE: {
                Value operand = pyro_pop(vm);
                if (IS_I64(operand)) {
                    pyro_push(vm, pyro_make_i64(~operand.as.i64));
                } else {
                    pyro_panic(vm, "bitwise '~' requires an integer operand");
                }
                break;
            }

            case OP_BINARY_CARET: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_caret(vm, a, b));
                break;
            }

            case OP_CALL_VALUE: {
                uint8_t arg_count = READ_BYTE();
                call_value(vm, arg_count);
                break;
            }

            case OP_CALL_VALUE_0: {
                call_value(vm, 0);
                break;
            }

            case OP_CALL_VALUE_1: {
                call_value(vm, 1);
                break;
            }

            case OP_CALL_VALUE_2: {
                call_value(vm, 2);
                break;
            }

            case OP_CALL_VALUE_3: {
                call_value(vm, 3);
                break;
            }

            case OP_CALL_VALUE_4: {
                call_value(vm, 4);
                break;
            }

            case OP_CALL_VALUE_5: {
                call_value(vm, 5);
                break;
            }

            case OP_CALL_VALUE_6: {
                call_value(vm, 6);
                break;
            }

            case OP_CALL_VALUE_7: {
                call_value(vm, 7);
                break;
            }

            case OP_CALL_VALUE_8: {
                call_value(vm, 8);
                break;
            }

            case OP_CALL_VALUE_9: {
                call_value(vm, 9);
                break;
            }

            case OP_CALL_VALUE_WITH_UNPACK: {
                uint8_t arg_count = READ_BYTE();
                Value last_arg = pyro_pop(vm);
                arg_count--;

                Value* values;
                size_t value_count;

                if (IS_TUP(last_arg)) {
                    values = AS_TUP(last_arg)->values;
                    value_count = AS_TUP(last_arg)->count;
                } else if (IS_VEC(last_arg)) {
                    values = AS_VEC(last_arg)->values;
                    value_count = AS_VEC(last_arg)->count;
                } else {
                    pyro_panic(vm, "can only unpack a vector or tuple");
                    break;
                }

                size_t total_args = (size_t)arg_count + value_count;
                if (total_args > 255) {
                    pyro_panic(vm, "too many arguments (max: 255)");
                    break;
                }

                for (size_t i = 0; i < value_count; i++) {
                    if (!pyro_push(vm, values[i])) {
                        break;
                    }
                }

                call_value(vm, total_args);
                break;
            }

            case OP_MAKE_CLASS: {
                ObjClass* class = ObjClass_new(vm);
                if (class) {
                    class->name = READ_STRING();
                    pyro_push(vm, pyro_make_obj(class));
                } else {
                    pyro_panic(vm, "out of memory");
                }
                break;
            }

            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack_top - 1);
                pyro_pop(vm);
                break;
            }

            case OP_MAKE_CLOSURE: {
                ObjPyroFn* fn = AS_PYRO_FN(READ_CONSTANT());
                ObjModule* module = frame->closure->module;
                ObjClosure* closure = ObjClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_push(vm, pyro_make_obj(closure));

                for (size_t i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->fp + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }

            case OP_MAKE_CLOSURE_WITH_DEF_ARGS: {
                ObjPyroFn* fn = AS_PYRO_FN(READ_CONSTANT());
                ObjModule* module = frame->closure->module;
                ObjClosure* closure = ObjClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                uint8_t default_value_count = READ_BYTE();
                for (uint8_t i = 0; i < default_value_count; i++) {
                    Value value = vm->stack_top[-default_value_count + i];
                    if (!ObjVec_append(closure->default_values, value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= default_value_count;
                pyro_push(vm, pyro_make_obj(closure));

                for (size_t i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->fp + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }

            case OP_DEFINE_PRI_GLOBAL: {
                ObjModule* module = frame->closure->module;
                Value name = READ_CONSTANT();

                if (AS_STR(name)->length == 1 && AS_STR(name)->bytes[0] == '_') {
                    pyro_pop(vm);
                    break;
                }

                if (ObjMap_contains(module->all_member_indexes, name, vm)) {
                    pyro_panic(vm, "the global variable '%s' already exists", AS_STR(name)->bytes);
                    break;
                }

                size_t member_index = module->members->count;
                if (!ObjVec_append(module->members, pyro_peek(vm, 0), vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(module->all_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_PUB_GLOBAL: {
                ObjModule* module = frame->closure->module;
                Value name = READ_CONSTANT();

                if (AS_STR(name)->length == 1 && AS_STR(name)->bytes[0] == '_') {
                    pyro_pop(vm);
                    break;
                }

                if (ObjMap_contains(module->all_member_indexes, name, vm)) {
                    pyro_panic(vm, "the global variable '%s' already exists", AS_STR(name)->bytes);
                    break;
                }

                size_t member_index = module->members->count;
                if (!ObjVec_append(module->members, pyro_peek(vm, 0), vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(module->all_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(module->pub_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                    ObjMap_remove(module->all_member_indexes, name, vm);
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_PRI_GLOBALS: {
                ObjModule* module = frame->closure->module;
                uint8_t count = READ_BYTE();

                for (uint8_t i = 0; i < count; i++) {
                    Value name = READ_CONSTANT();
                    if (AS_STR(name)->length == 1 && AS_STR(name)->bytes[0] == '_') {
                        continue;
                    }

                    if (ObjMap_contains(module->all_member_indexes, name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", AS_STR(name)->bytes);
                        break;
                    }

                    size_t member_index = module->members->count;
                    if (!ObjVec_append(module->members, pyro_peek(vm, count - 1 - i), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (ObjMap_set(module->all_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= count;
                break;
            }

            case OP_DEFINE_PUB_GLOBALS: {
                ObjModule* module = frame->closure->module;
                uint8_t count = READ_BYTE();

                for (uint8_t i = 0; i < count; i++) {
                    Value name = READ_CONSTANT();

                    if (AS_STR(name)->length == 1 && AS_STR(name)->bytes[0] == '_') {
                        continue;
                    }

                    if (ObjMap_contains(module->all_member_indexes, name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", AS_STR(name)->bytes);
                        break;
                    }

                    size_t member_index = module->members->count;
                    if (!ObjVec_append(module->members, pyro_peek(vm, count - 1 - i), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (ObjMap_set(module->all_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (ObjMap_set(module->pub_member_indexes, name, pyro_make_i64(member_index), vm) == 0) {
                        ObjMap_remove(module->all_member_indexes, name, vm);
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= count;
                break;
            }

            // Duplicate the top item on the stack.
            case OP_DUP: {
                pyro_push(vm, pyro_peek(vm, 0));
                break;
            }

            // Duplicate the top 2 items on the stack.
            case OP_DUP_2: {
                pyro_push(vm, pyro_peek(vm, 1));
                pyro_push(vm, pyro_peek(vm, 1));
                break;
            }

            case OP_ECHO: {
                uint8_t arg_count = READ_BYTE();

                for (uint8_t i = arg_count; i > 0; i--) {
                    Value value = vm->stack_top[-i];
                    ObjStr* string = pyro_stringify_value(vm, value);
                    if (vm->halt_flag) {
                        break;
                    }
                    int64_t n = pyro_stdout_write_f(vm, i > 1 ? "%s " : "%s\n", string->bytes);
                    if (n == -1) {
                        pyro_panic(vm, "echo: failed to write to the standard output stream");
                        break;
                    } else if (n == -2) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= arg_count;
                break;
            }

            case OP_BINARY_EQUAL_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_compare_eq(vm, a, b)));
                break;
            }

            case OP_BINARY_SLASH: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_slash(vm, a, b));
                break;
            }

            case OP_DEFINE_PRI_FIELD: {
                // The field's default value will be sitting on top of the stack.
                Value default_value = pyro_peek(vm, 0);

                // The class object will be on the stack just below the default value.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* field_name = READ_STRING();
                if (ObjMap_contains(class->all_field_indexes, pyro_make_obj(field_name), vm)) {
                    pyro_panic(vm, "the field '%s' already exists", field_name->bytes);
                    break;
                }

                size_t field_index = class->default_field_values->count;
                if (!ObjVec_append(class->default_field_values, default_value, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(class->all_field_indexes, pyro_make_obj(field_name), pyro_make_i64(field_index), vm) == 0) {
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the default value but leave the class object on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_PUB_FIELD: {
                // The field's default value will be sitting on top of the stack.
                Value default_value = pyro_peek(vm, 0);

                // The class object will be on the stack just below the default value.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* field_name = READ_STRING();
                if (ObjMap_contains(class->all_field_indexes, pyro_make_obj(field_name), vm)) {
                    pyro_panic(vm, "the field '%s' already exists", field_name->bytes);
                    break;
                }

                size_t field_index = class->default_field_values->count;
                if (!ObjVec_append(class->default_field_values, default_value, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(class->all_field_indexes, pyro_make_obj(field_name), pyro_make_i64(field_index), vm) == 0) {
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (ObjMap_set(class->pub_field_indexes, pyro_make_obj(field_name), pyro_make_i64(field_index), vm) == 0) {
                    ObjMap_remove(class->all_field_indexes, pyro_make_obj(field_name), vm);
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the default value but leave the class object on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_STATIC_FIELD: {
                // The field's default value will be sitting on top of the stack.
                Value default_value = pyro_peek(vm, 0);

                // The class object will be on the stack just below the default value.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* field_name = READ_STRING();
                if (ObjMap_contains(class->static_fields, pyro_make_obj(field_name), vm)) {
                    pyro_panic(vm, "the static field '%s' already exists", field_name->bytes);
                    break;
                }

                if (ObjMap_set(class->static_fields, pyro_make_obj(field_name), default_value, vm) == 0) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the default value but leave the class object on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_GET_FIELD: {
                Value field_name = READ_CONSTANT();
                Value receiver = pyro_peek(vm, 0);

                if (IS_INSTANCE(receiver)) {
                    ObjInstance* instance = AS_INSTANCE(receiver);
                    Value field_index;
                    if (ObjMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        pyro_pop(vm); // pop the instance
                        pyro_push(vm, instance->fields[field_index.as.i64]);
                        break;
                    }
                } else if (IS_CLASS(receiver)) {
                    ObjClass* class = AS_CLASS(receiver);
                    Value value;
                    if (ObjMap_get(class->static_fields, field_name, &value, vm)) {
                        pyro_pop(vm); // pop the class
                        pyro_push(vm, value);
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", AS_STR(field_name)->bytes);
                break;
            }

            case OP_GET_PUB_FIELD: {
                Value field_name = READ_CONSTANT();
                Value receiver = pyro_peek(vm, 0);

                if (IS_INSTANCE(receiver)) {
                    ObjInstance* instance = AS_INSTANCE(receiver);
                    Value field_index;
                    if (ObjMap_get(instance->obj.class->pub_field_indexes, field_name, &field_index, vm)) {
                        pyro_pop(vm); // pop the instance
                        pyro_push(vm, instance->fields[field_index.as.i64]);
                        break;
                    } else if (ObjMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        pyro_panic(vm, "field '%s' is private", AS_STR(field_name)->bytes);
                        break;
                    }
                } else if (IS_CLASS(receiver)) {
                    ObjClass* class = AS_CLASS(receiver);
                    Value value;
                    if (ObjMap_get(class->static_fields, field_name, &value, vm)) {
                        pyro_pop(vm); // pop the class
                        pyro_push(vm, value);
                        break;
                    }
                } else if (IS_MOD(receiver)) {
                    pyro_panic(vm,
                        "invalid field name '%s'; receiver is a module, did you mean to use '::'",
                        AS_STR(field_name)->bytes
                    );
                    break;
                }

                pyro_panic(vm, "invalid field name '%s'", AS_STR(field_name)->bytes);
                break;
            }

            case OP_GET_GLOBAL: {
                Value name = READ_CONSTANT();

                Value member_index;
                if (ObjMap_get(frame->closure->module->all_member_indexes, name, &member_index, vm)) {
                    Value value = frame->closure->module->members->values[member_index.as.i64];
                    pyro_push(vm, value);
                    break;
                }

                Value value;
                if (ObjMap_get(vm->superglobals, name, &value, vm)) {
                    pyro_push(vm, value);
                    break;
                }

                pyro_panic(vm, "undefined variable '%s'", AS_STR(name)->bytes);
                break;
            }

            case OP_GET_INDEX: {
                Value key = pyro_pop(vm);
                Value receiver = pyro_pop(vm);
                pyro_push(vm, pyro_op_get_index(vm, receiver, key));
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t index = READ_BYTE();
                pyro_push(vm, frame->fp[index]);
                break;
            }

            case OP_GET_LOCAL_0: {
                pyro_push(vm, frame->fp[0]);
                break;
            }

            case OP_GET_LOCAL_1: {
                pyro_push(vm, frame->fp[1]);
                break;
            }

            case OP_GET_LOCAL_2: {
                pyro_push(vm, frame->fp[2]);
                break;
            }

            case OP_GET_LOCAL_3: {
                pyro_push(vm, frame->fp[3]);
                break;
            }

            case OP_GET_LOCAL_4: {
                pyro_push(vm, frame->fp[4]);
                break;
            }

            case OP_GET_LOCAL_5: {
                pyro_push(vm, frame->fp[5]);
                break;
            }

            case OP_GET_LOCAL_6: {
                pyro_push(vm, frame->fp[6]);
                break;
            }

            case OP_GET_LOCAL_7: {
                pyro_push(vm, frame->fp[7]);
                break;
            }

            case OP_GET_LOCAL_8: {
                pyro_push(vm, frame->fp[8]);
                break;
            }

            case OP_GET_LOCAL_9: {
                pyro_push(vm, frame->fp[9]);
                break;
            }

            case OP_GET_MEMBER: {
                Value member_name = READ_CONSTANT();
                Value receiver = pyro_pop(vm);

                if (!IS_MOD(receiver)) {
                    if (IS_INSTANCE(receiver)) {
                        pyro_panic(vm,
                            "invalid member access '%s', receiver is an object instance, did you mean to use ':'",
                            AS_STR(member_name)->bytes
                        );
                    } else if (IS_CLASS(receiver)) {
                        pyro_panic(vm,
                            "invalid member access '%s', receiver is a class, did you mean to use ':'",
                            AS_STR(member_name)->bytes
                        );
                    } else {
                        pyro_panic(vm,
                            "invalid member access '%s', receiver is not a module",
                            AS_STR(member_name)->bytes
                        );
                    }
                    break;
                }

                ObjModule* module = AS_MOD(receiver);
                Value member_index;

                if (ObjMap_get(module->pub_member_indexes, member_name, &member_index, vm)) {
                    pyro_push(vm, module->members->values[member_index.as.i64]);
                } else if (ObjMap_get(module->all_member_indexes, member_name, &member_index, vm)) {
                    pyro_panic(vm, "module member '%s' is private", AS_STR(member_name)->bytes);
                } else {
                    pyro_panic(vm, "module has no member '%s'", AS_STR(member_name)->bytes);
                }

                break;
            }

            case OP_GET_METHOD: {
                Value receiver = pyro_peek(vm, 0);
                ObjStr* method_name = READ_STRING();

                Value method = pyro_get_method(vm, receiver, method_name);
                if (IS_NULL(method)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                ObjBoundMethod* bound_method = ObjBoundMethod_new(vm, receiver, AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the receiver and replace it with the bound method.
                vm->stack_top[-1] = pyro_make_obj(bound_method);
                break;
            }

            case OP_GET_PUB_METHOD: {
                Value receiver = pyro_peek(vm, 0);
                ObjStr* method_name = READ_STRING();

                Value method = pyro_get_pub_method(vm, receiver, method_name);
                if (IS_NULL(method)) {
                    if (IS_NULL(pyro_get_method(vm, receiver, method_name))) {
                        if (IS_MOD(receiver)) {
                            pyro_panic(vm,
                                "invalid method name '%s'; receiver is a module, did you mean to use '::'",
                                method_name->bytes
                            );
                        } else {
                            pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                        }
                    } else {
                        pyro_panic(vm, "method '%s' is private", method_name->bytes);
                    }
                    break;
                }

                ObjBoundMethod* bound_method = ObjBoundMethod_new(vm, receiver, AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the receiver and replace it with the bound method.
                vm->stack_top[-1] = pyro_make_obj(bound_method);
                break;
            }

            case OP_GET_SUPER_METHOD: {
                ObjStr* method_name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pyro_pop(vm));
                Value receiver = pyro_peek(vm, 0);

                Value method;
                if (!ObjMap_get(superclass->all_instance_methods, pyro_make_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                ObjBoundMethod* bound_method = ObjBoundMethod_new(vm, receiver, AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the receiver and replace it with the bound method.
                vm->stack_top[-1] = pyro_make_obj(bound_method);
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t index = READ_BYTE();
                pyro_push(vm, *frame->closure->upvalues[index]->location);
                break;
            }

            case OP_BINARY_GREATER: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_compare_gt(vm, a, b)));
                break;
            }

            case OP_BINARY_GREATER_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_compare_ge(vm, a, b)));
                break;
            }

            // The import path is stored on the stack as an array of [arg_count] strings.
            case OP_IMPORT_MODULE: {
                uint8_t arg_count = READ_BYTE();
                Value* args = vm->stack_top - arg_count;

                ObjModule* module = load_module(vm, args, arg_count);
                if (vm->halt_flag) {
                    break;
                }

                vm->stack_top -= arg_count;
                pyro_push(vm, pyro_make_obj(module));
                break;
            }

            case OP_IMPORT_ALL_MEMBERS: {
                uint8_t arg_count = READ_BYTE();
                Value* args = vm->stack_top - arg_count;

                ObjModule* current_module = frame->closure->module;
                ObjModule* imported_module = load_module(vm, args, arg_count);
                if (vm->halt_flag) {
                    break;
                }

                for (size_t i = 0; i < imported_module->pub_member_indexes->entry_array_count; i++) {
                    MapEntry* entry = &imported_module->pub_member_indexes->entry_array[i];
                    if (IS_TOMBSTONE(entry->key)) {
                        continue;
                    }

                    Value member_name = entry->key;
                    Value member_index_in_imported_module = entry->value;
                    Value value = imported_module->members->values[member_index_in_imported_module.as.i64];

                    if (ObjMap_contains(current_module->all_member_indexes, member_name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", AS_STR(member_name)->bytes);
                        break;
                    }

                    size_t member_index_in_current_module = current_module->members->count;
                    if (!ObjVec_append(current_module->members, value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (ObjMap_set(current_module->all_member_indexes, member_name, pyro_make_i64(member_index_in_current_module), vm) == 0) {
                        current_module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= arg_count;
                break;
            }

            case OP_IMPORT_NAMED_MEMBERS: {
                uint8_t module_count = READ_BYTE();
                uint8_t member_count = READ_BYTE();
                Value* args = vm->stack_top - module_count - member_count;

                ObjModule* module = load_module(vm, args, module_count);
                if (vm->halt_flag) {
                    break;
                }

                for (uint8_t i = 0; i < member_count; i++) {
                    Value name = args[module_count + i];
                    Value member_index;
                    if (!ObjMap_get(module->all_member_indexes, name, &member_index, vm)) {
                        pyro_panic(vm, "module has no member '%s'", AS_STR(name)->bytes);
                        break;
                    }
                    args[i] = module->members->values[member_index.as.i64];
                }

                vm->stack_top -= module_count;
                break;
            }

            case OP_INHERIT: {
                if (!IS_CLASS(pyro_peek(vm, 1))) {
                    pyro_panic(vm, "invalid superclass value (not a class)");
                    break;
                }

                ObjClass* superclass = AS_CLASS(pyro_peek(vm, 1));
                ObjClass* subclass = AS_CLASS(pyro_peek(vm, 0));

                if (superclass == subclass) {
                    pyro_panic(vm, "a class cannot inherit from itself");
                    break;
                }

                // "Copy-down inheritance". We copy all the superclass's methods, field indexes,
                // and field initializers to the subclass. This means that there's no extra
                // runtime work involved in looking up inherited methods or fields.
                if (!ObjMap_copy_entries(superclass->all_instance_methods, subclass->all_instance_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->pub_instance_methods, subclass->pub_instance_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->all_field_indexes, subclass->all_field_indexes, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->pub_field_indexes, subclass->pub_field_indexes, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjVec_copy_entries(superclass->default_field_values, subclass->default_field_values, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->static_methods, subclass->static_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->static_fields, subclass->static_fields, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                subclass->init_method = superclass->init_method;
                subclass->superclass = superclass;

                // Pop the subclass, leave the superclass behind as the [super] variable.
                pyro_pop(vm);
                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case OP_CALL_METHOD: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                Value receiver = pyro_peek(vm, arg_count);

                Value method = pyro_get_method(vm, receiver, method_name);
                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
                } else if (IS_CLOSURE(method)) {
                    call_closure(vm, AS_CLOSURE(method), arg_count);
                } else {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case OP_CALL_PUB_METHOD: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                Value receiver = pyro_peek(vm, arg_count);

                Value method = pyro_get_pub_method(vm, receiver, method_name);
                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
                } else if (IS_CLOSURE(method)) {
                    call_closure(vm, AS_CLOSURE(method), arg_count);
                } else {
                    if (IS_NULL(pyro_get_method(vm, receiver, method_name))) {
                        if (IS_MOD(receiver)) {
                            pyro_panic(vm,
                                "invalid method name '%s'; receiver is a module, did you mean to use '::'",
                                method_name->bytes
                            );
                        } else {
                            pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                        }
                    } else {
                        pyro_panic(vm, "method '%s' is private", method_name->bytes);
                    }
                    break;
                }

                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case OP_CALL_METHOD_WITH_UNPACK: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                Value receiver = pyro_peek(vm, arg_count);
                Value last_arg = pyro_pop(vm);
                arg_count--;

                Value* values;
                size_t value_count;

                if (IS_TUP(last_arg)) {
                    values = AS_TUP(last_arg)->values;
                    value_count = AS_TUP(last_arg)->count;
                } else if (IS_VEC(last_arg)) {
                    values = AS_VEC(last_arg)->values;
                    value_count = AS_VEC(last_arg)->count;
                } else {
                    pyro_panic(vm, "can only unpack a vector or tuple");
                    break;
                }

                size_t total_args = (size_t)arg_count + value_count;
                if (total_args > 255) {
                    pyro_panic(vm, "too many arguments to unpack");
                    break;
                }

                for (size_t i = 0; i < value_count; i++) {
                    if (!pyro_push(vm, values[i])) {
                        break;
                    }
                }

                Value method = pyro_get_method(vm, receiver, method_name);
                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), total_args);
                } else if (IS_CLOSURE(method)) {
                    call_closure(vm, AS_CLOSURE(method), total_args);
                } else {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case OP_CALL_PUB_METHOD_WITH_UNPACK: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                Value receiver = pyro_peek(vm, arg_count);
                Value last_arg = pyro_pop(vm);
                arg_count--;

                Value* values;
                size_t value_count;

                if (IS_TUP(last_arg)) {
                    values = AS_TUP(last_arg)->values;
                    value_count = AS_TUP(last_arg)->count;
                } else if (IS_VEC(last_arg)) {
                    values = AS_VEC(last_arg)->values;
                    value_count = AS_VEC(last_arg)->count;
                } else {
                    pyro_panic(vm, "can only unpack a vector or tuple");
                    break;
                }

                size_t total_args = (size_t)arg_count + value_count;
                if (total_args > 255) {
                    pyro_panic(vm, "too many arguments to unpack");
                    break;
                }

                for (size_t i = 0; i < value_count; i++) {
                    if (!pyro_push(vm, values[i])) {
                        break;
                    }
                }

                Value method = pyro_get_pub_method(vm, receiver, method_name);
                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), total_args);
                } else if (IS_CLOSURE(method)) {
                    call_closure(vm, AS_CLOSURE(method), total_args);
                } else {
                    if (IS_MOD(receiver)) {
                        pyro_panic(vm,
                            "invalid method name '%s'; receiver is a module, did you mean to use '::'",
                            method_name->bytes
                        );
                    } else {
                        pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    }
                }

                break;
            }

            // The [receiver], [arg_count] args, and the [superclass] are sitting on top of the stack.
            case OP_CALL_SUPER_METHOD: {
                ObjClass* superclass = AS_CLASS(pyro_pop(vm));
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();

                Value method;
                if (!ObjMap_get(superclass->all_instance_methods, pyro_make_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
                } else {
                    call_closure(vm, AS_CLOSURE(method), arg_count);
                }

                break;
            }

            case OP_CALL_SUPER_METHOD_WITH_UNPACK: {
                ObjClass* superclass = AS_CLASS(pyro_pop(vm));
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                Value last_arg = pyro_pop(vm);
                arg_count--;

                Value* values;
                size_t value_count;

                if (IS_TUP(last_arg)) {
                    values = AS_TUP(last_arg)->values;
                    value_count = AS_TUP(last_arg)->count;
                } else if (IS_VEC(last_arg)) {
                    values = AS_VEC(last_arg)->values;
                    value_count = AS_VEC(last_arg)->count;
                } else {
                    pyro_panic(vm, "can only unpack a vector or tuple");
                    break;
                }

                size_t total_args = (size_t)arg_count + value_count;
                if (total_args > 255) {
                    pyro_panic(vm, "too many arguments to unpack");
                    break;
                }

                for (size_t i = 0; i < value_count; i++) {
                    if (!pyro_push(vm, values[i])) {
                        break;
                    }
                }

                Value method;
                if (!ObjMap_get(superclass->all_instance_methods, pyro_make_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                if (IS_NATIVE_FN(method)) {
                    call_native_fn(vm, AS_NATIVE_FN(method), total_args);
                } else {
                    call_closure(vm, AS_CLOSURE(method), total_args);
                }

                break;
            }

            case OP_GET_ITERATOR: {
                Value receiver = pyro_peek(vm, 0);
                Value iter_method = pyro_get_method(vm, receiver, vm->str_dollar_iter);

                if (IS_NATIVE_FN(iter_method)) {
                    call_native_fn(vm, AS_NATIVE_FN(iter_method), 0);
                } else if (IS_CLOSURE(iter_method)) {
                    call_closure(vm, AS_CLOSURE(iter_method), 0);
                } else {
                    pyro_panic(vm, "value is not iterable");
                    break;
                }

                break;
            }

            case OP_GET_NEXT_FROM_ITERATOR: {
                Value receiver = pyro_peek(vm, 0);

                if (IS_ITER(receiver)) {
                    Value next_value = ObjIter_next(AS_ITER(receiver), vm);
                    pyro_push(vm, next_value);
                    break;
                }

                Value next_method = pyro_get_method(vm, receiver, vm->str_dollar_next);

                if (IS_NATIVE_FN(next_method)) {
                    pyro_push(vm, receiver);
                    call_native_fn(vm, AS_NATIVE_FN(next_method), 0);
                } else if (IS_CLOSURE(next_method)) {
                    pyro_push(vm, receiver);
                    call_closure(vm, AS_CLOSURE(next_method), 0);
                } else {
                    pyro_panic(vm, "invalid iterator: no $next() method");
                    break;
                }

                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_BE_U16();
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_ERR: {
                uint16_t offset = READ_BE_U16();
                if (IS_ERR(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_BE_U16();
                if (!pyro_is_truthy(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_NOT_ERR: {
                uint16_t offset = READ_BE_U16();
                if (!IS_ERR(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            // The set of 'kinda-falsey' values is: false, null, err, 0, 0.0, "".
            case OP_JUMP_IF_NOT_KINDA_FALSEY: {
                uint16_t offset = READ_BE_U16();
                Value value = pyro_peek(vm, 0);
                bool is_kinda_falsey = false;

                if (IS_BOOL(value) && value.as.boolean == false) {
                    is_kinda_falsey = true;
                } else if (IS_NULL(value)) {
                    is_kinda_falsey = true;
                } else if (IS_ERR(value)) {
                    is_kinda_falsey = true;
                } else if (IS_I64(value) && value.as.i64 == 0) {
                    is_kinda_falsey = true;
                } else if (IS_CHAR(value) && value.as.u32 == 0) {
                    is_kinda_falsey = true;
                } else if (IS_F64(value) && value.as.f64 == 0.0) {
                    is_kinda_falsey = true;
                } else if (IS_STR(value) && AS_STR(value)->length == 0) {
                    is_kinda_falsey = true;
                }

                if (!is_kinda_falsey) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_NOT_NULL: {
                uint16_t offset = READ_BE_U16();
                if (!IS_NULL(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_BE_U16();
                if (pyro_is_truthy(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_BINARY_LESS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_compare_lt(vm, a, b)));
                break;
            }

            case OP_BINARY_LESS_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_compare_le(vm, a, b)));
                break;
            }

            case OP_BINARY_IN: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(pyro_op_in(vm, a, b)));
                break;
            }

            case OP_LOAD_CONSTANT: {
                Value constant = READ_CONSTANT();
                pyro_push(vm, constant);
                break;
            }

            case OP_LOAD_CONSTANT_0: {
                pyro_push(vm, frame->closure->fn->constants[0]);
                break;
            }

            case OP_LOAD_CONSTANT_1: {
                pyro_push(vm, frame->closure->fn->constants[1]);
                break;
            }

            case OP_LOAD_CONSTANT_2: {
                pyro_push(vm, frame->closure->fn->constants[2]);
                break;
            }

            case OP_LOAD_CONSTANT_3: {
                pyro_push(vm, frame->closure->fn->constants[3]);
                break;
            }

            case OP_LOAD_CONSTANT_4: {
                pyro_push(vm, frame->closure->fn->constants[4]);
                break;
            }

            case OP_LOAD_CONSTANT_5: {
                pyro_push(vm, frame->closure->fn->constants[5]);
                break;
            }

            case OP_LOAD_CONSTANT_6: {
                pyro_push(vm, frame->closure->fn->constants[6]);
                break;
            }

            case OP_LOAD_CONSTANT_7: {
                pyro_push(vm, frame->closure->fn->constants[7]);
                break;
            }

            case OP_LOAD_CONSTANT_8: {
                pyro_push(vm, frame->closure->fn->constants[8]);
                break;
            }

            case OP_LOAD_CONSTANT_9: {
                pyro_push(vm, frame->closure->fn->constants[9]);
                break;
            }

            case OP_LOAD_FALSE:
                pyro_push(vm, pyro_make_bool(false));
                break;

            case OP_LOAD_I64_0:
                pyro_push(vm, pyro_make_i64(0));
                break;

            case OP_LOAD_I64_1:
                pyro_push(vm, pyro_make_i64(1));
                break;

            case OP_LOAD_I64_2:
                pyro_push(vm, pyro_make_i64(2));
                break;

            case OP_LOAD_I64_3:
                pyro_push(vm, pyro_make_i64(3));
                break;

            case OP_LOAD_I64_4:
                pyro_push(vm, pyro_make_i64(4));
                break;

            case OP_LOAD_I64_5:
                pyro_push(vm, pyro_make_i64(5));
                break;

            case OP_LOAD_I64_6:
                pyro_push(vm, pyro_make_i64(6));
                break;

            case OP_LOAD_I64_7:
                pyro_push(vm, pyro_make_i64(7));
                break;

            case OP_LOAD_I64_8:
                pyro_push(vm, pyro_make_i64(8));
                break;

            case OP_LOAD_I64_9:
                pyro_push(vm, pyro_make_i64(9));
                break;

            case OP_LOAD_NULL:
                pyro_push(vm, pyro_make_null());
                break;

            case OP_LOAD_TRUE:
                pyro_push(vm, pyro_make_bool(true));
                break;

            case OP_JUMP_BACK: {
                uint16_t offset = READ_BE_U16();
                frame->ip -= offset;
                break;
            }

            case OP_BINARY_LESS_LESS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 >= 0) {
                        pyro_push(vm, pyro_make_i64(a.as.i64 << b.as.i64));
                    } else {
                        pyro_panic(vm, "right operand to '<<' cannot be negative");
                    }
                } else {
                    pyro_panic(vm, "operands to '<<' must both be integers");
                }
                break;
            }

            case OP_MAKE_MAP: {
                uint16_t entry_count = READ_BE_U16();

                ObjMap* map = ObjMap_new(vm);
                if (!map) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // The entries are stored on the stack as [..][key][value][..] pairs.
                for (Value* slot = vm->stack_top - entry_count * 2; slot < vm->stack_top; slot += 2) {
                    if (!ObjMap_set(map, slot[0], slot[1], vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= (entry_count * 2);
                pyro_push(vm, pyro_make_obj(map));
                break;
            }

            case OP_MAKE_SET: {
                uint16_t entry_count = READ_BE_U16();

                ObjMap* map = ObjMap_new_as_set(vm);
                if (!map) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                for (Value* slot = vm->stack_top - entry_count; slot < vm->stack_top; slot++) {
                    if (!ObjMap_set(map, *slot, pyro_make_null(), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= entry_count;
                pyro_push(vm, pyro_make_obj(map));
                break;
            }

            case OP_MAKE_VEC: {
                uint16_t item_count = READ_BE_U16();

                ObjVec* vec = ObjVec_new_with_cap(item_count, vm);
                if (!vec) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (item_count == 0) {
                    pyro_push(vm, pyro_make_obj(vec));
                    break;
                }

                memcpy(vec->values, vm->stack_top - item_count, sizeof(Value) * item_count);
                vec->count = item_count;

                vm->stack_top -= item_count;
                pyro_push(vm, pyro_make_obj(vec));
                break;
            }

            case OP_DEFINE_PRI_METHOD: {
                // The method's ObjClosure will be sitting on top of the stack.
                Value method = pyro_peek(vm, 0);

                // The class object will be on the stack just below the method.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* name = READ_STRING();
                if (ObjMap_contains(class->pub_instance_methods, pyro_make_obj(name), vm)) {
                    pyro_panic(vm, "cannot override public method '%s' as private", name->bytes);
                    break;
                }

                if (!ObjMap_set(class->all_instance_methods, pyro_make_obj(name), method, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (name == vm->str_dollar_init) {
                    class->init_method = method;
                }

                // Pop the method but leave the class behind on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_PUB_METHOD: {
                // The method's ObjClosure will be sitting on top of the stack.
                Value method = pyro_peek(vm, 0);

                // The class object will be on the stack just below the method.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* name = READ_STRING();
                if (ObjMap_contains(class->all_instance_methods, pyro_make_obj(name), vm)) {
                    if (!ObjMap_contains(class->pub_instance_methods, pyro_make_obj(name), vm)) {
                        pyro_panic(vm, "cannot override private method '%s' as public", name->bytes);
                        break;
                    }
                }
                if (name->length > 0 && name->bytes[0] == '$') {
                    pyro_panic(vm, "$-prefixed method cannot be declared public");
                    break;
                }

                if (!ObjMap_set(class->all_instance_methods, pyro_make_obj(name), method, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (!ObjMap_set(class->pub_instance_methods, pyro_make_obj(name), method, vm)) {
                    ObjMap_remove(class->all_instance_methods, pyro_make_obj(name), vm);
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the method but leave the class behind on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_STATIC_METHOD: {
                // The method's ObjClosure will be sitting on top of the stack.
                Value method = pyro_peek(vm, 0);

                // The class object will be on the stack just below the method.
                ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

                ObjStr* name = READ_STRING();
                if (ObjMap_set(class->static_methods, pyro_make_obj(name), method, vm) == 0) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // Pop the method but leave the class behind on the stack.
                pyro_pop(vm);
                break;
            }

            case OP_BINARY_PERCENT: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_percent(vm, a, b));
                break;
            }

            case OP_BINARY_STAR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_star(vm, a, b));
                break;
            }

            case OP_UNARY_MINUS: {
                Value operand = pyro_pop(vm);
                pyro_push(vm, pyro_op_unary_minus(vm, operand));
                break;
            }

            case OP_UNARY_PLUS: {
                Value operand = pyro_pop(vm);
                pyro_push(vm, pyro_op_unary_plus(vm, operand));
                break;
            }

            case OP_UNARY_BANG: {
                Value operand = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(!pyro_is_truthy(operand)));
                break;
            }

            case OP_BINARY_BANG_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_make_bool(!pyro_op_compare_eq(vm, a, b)));
                break;
            }

            case OP_POP:
                pyro_pop(vm);
                break;

            case OP_POP_ECHO_IN_REPL: {
                Value value = pyro_peek(vm, 0);

                if (vm->in_repl && !IS_NULL(value)) {
                    ObjStr* string = pyro_debugify_value(vm, value);
                    if (vm->halt_flag) {
                        break;
                    }
                    pyro_stdout_write_f(vm, "%s\n", string->bytes);
                }

                pyro_pop(vm);
                break;
            }

            case OP_POP_JUMP_IF_FALSE: {
                uint16_t offset = READ_BE_U16();
                if (!pyro_is_truthy(pyro_pop(vm))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_BINARY_STAR_STAR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_star_star(vm, a, b));
                break;
            }

            case OP_RETURN: {
                while (vm->with_stack_count > frame->with_stack_count_on_entry) {
                    Value receiver = vm->with_stack[vm->with_stack_count - 1];
                    call_end_with_method(vm, receiver);
                    vm->with_stack_count--;
                }

                Value result = pyro_pop(vm);
                close_upvalues(vm, frame->fp);
                vm->stack_top = frame->fp;
                pyro_push(vm, result);

                vm->frame_count--;
                break;
            }

            case OP_BINARY_GREATER_GREATER: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 >= 0) {
                        pyro_push(vm, pyro_make_i64(a.as.i64 >> b.as.i64));
                    } else {
                        pyro_panic(vm, "right operand to '>>' cannot be negative");
                    }
                } else {
                    pyro_panic(vm, "operands to '>>' must both be integers");
                }
                break;
            }

            case OP_SET_FIELD: {
                Value field_name = READ_CONSTANT();
                Value receiver = pyro_peek(vm, 1);

                if (IS_INSTANCE(receiver)) {
                    ObjInstance* instance = AS_INSTANCE(receiver);
                    Value field_index;
                    if (ObjMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        Value new_value = pyro_pop(vm); // pop the value
                        pyro_pop(vm); // pop the instance
                        instance->fields[field_index.as.i64] = new_value;
                        pyro_push(vm, new_value);
                        break;
                    }
                } else if (IS_CLASS(receiver)) {
                    ObjClass* class = AS_CLASS(receiver);
                    if (ObjMap_contains(class->static_fields, field_name, vm)) {
                        Value new_value = pyro_pop(vm); // pop the value
                        pyro_pop(vm); // pop the class
                        ObjMap_set(class->static_fields, field_name, new_value, vm);
                        pyro_push(vm, new_value);
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", AS_STR(field_name)->bytes);
                break;
            }

            case OP_SET_PUB_FIELD: {
                Value field_name = READ_CONSTANT();
                Value receiver = pyro_peek(vm, 1);

                if (IS_INSTANCE(receiver)) {
                    ObjInstance* instance = AS_INSTANCE(receiver);
                    Value field_index;
                    if (ObjMap_get(instance->obj.class->pub_field_indexes, field_name, &field_index, vm)) {
                        Value new_value = pyro_pop(vm); // pop the value
                        pyro_pop(vm); // pop the instance
                        instance->fields[field_index.as.i64] = new_value;
                        pyro_push(vm, new_value);
                        break;
                    }
                } else if (IS_CLASS(receiver)) {
                    ObjClass* class = AS_CLASS(receiver);
                    if (ObjMap_contains(class->static_fields, field_name, vm)) {
                        Value new_value = pyro_pop(vm); // pop the value
                        pyro_pop(vm); // pop the class
                        ObjMap_set(class->static_fields, field_name, new_value, vm);
                        pyro_push(vm, new_value);
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", AS_STR(field_name)->bytes);
                break;
            }

            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                Value value = pyro_peek(vm, 0);

                Value member_index;
                if (ObjMap_get(frame->closure->module->all_member_indexes, name, &member_index, vm)) {
                    frame->closure->module->members->values[member_index.as.i64] = value;
                    break;
                }

                if (ObjMap_contains(vm->superglobals, name, vm)) {
                    pyro_panic(vm, "cannot assign to superglobal '%s'", AS_STR(name)->bytes);
                    break;
                }

                pyro_panic(vm, "undefined variable '%s'", AS_STR(name)->bytes);
                break;
            }

            case OP_SET_INDEX: {
                Value value = pyro_pop(vm);
                Value key = pyro_pop(vm);
                Value receiver = pyro_pop(vm);
                pyro_push(vm, pyro_op_set_index(vm, receiver, key, value));
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t index = READ_BYTE();
                frame->fp[index] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_0: {
                frame->fp[0] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_1: {
                frame->fp[1] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_2: {
                frame->fp[2] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_3: {
                frame->fp[3] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_4: {
                frame->fp[4] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_5: {
                frame->fp[5] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_6: {
                frame->fp[6] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_7: {
                frame->fp[7] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_8: {
                frame->fp[8] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_LOCAL_9: {
                frame->fp[9] = pyro_peek(vm, 0);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t index = READ_BYTE();
                *frame->closure->upvalues[index]->location = pyro_peek(vm, 0);
                break;
            }

            case OP_BINARY_MINUS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_minus(vm, a, b));
                break;
            }

            case OP_BINARY_SLASH_SLASH: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_slash_slash(vm, a, b));
                break;
            }

            case OP_TRY: {
                Value* stashed_stack_top = vm->stack_top;
                size_t stashed_frame_count = vm->frame_count;

                vm->try_depth++;
                call_value(vm, 0);
                run(vm);
                vm->try_depth--;

                if (vm->exit_flag) {
                    break;
                }

                if (vm->panic_flag) {
                    vm->halt_flag = false;
                    vm->panic_flag = false;
                    vm->panic_count = 0;
                    vm->exit_code = 0;
                    vm->memory_allocation_failed = false;
                    vm->stack_top = stashed_stack_top - 1;
                    close_upvalues(vm, stashed_stack_top);
                    vm->frame_count = stashed_frame_count;

                    ObjStr* error_message = ObjBuf_to_str(vm->panic_buffer, vm);
                    if (!error_message) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    ObjErr* err = ObjErr_new(vm);
                    if (!err) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                    err->message = error_message;

                    if (!ObjMap_set(err->details, pyro_make_obj(vm->str_source), pyro_make_obj(vm->panic_source_id), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (!ObjMap_set(err->details, pyro_make_obj(vm->str_line), pyro_make_i64(vm->panic_line_number), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    pyro_push(vm, pyro_make_obj(err));
                }

                assert(vm->stack_top == stashed_stack_top);
                assert(vm->frame_count == stashed_frame_count);
                break;
            }

            case OP_UNPACK: {
                Value value = pyro_pop(vm);
                uint8_t count = READ_BYTE();

                if (IS_TUP(value) || IS_ERR(value)) {
                    ObjTup* tup = AS_TUP(value);
                    if (tup->count < count) {
                        pyro_panic(
                            vm,
                            "tuple contains %zu value(s), requires %zu for unpacking", tup->count, count
                        );
                    } else {
                        for (size_t i = 0; i < count; i++) {
                            pyro_push(vm, tup->values[i]);
                        }
                    }
                } else if (IS_VEC(value)) {
                    ObjVec* vec = AS_VEC(value);
                    if (vec->count < count) {
                        pyro_panic(
                            vm,
                            "vector contains %zu value(s), requires %zu for unpacking",
                            vec->count, count
                        );
                    } else {
                        for (size_t i = 0; i < count; i++) {
                            pyro_push(vm, vec->values[i]);
                        }
                    }
                } else {
                    pyro_panic(vm, "value cannot be unpacked");
                }

                break;
            }

            case OP_START_WITH: {
                if (vm->with_stack_count == vm->with_stack_capacity) {
                    size_t new_capacity = GROW_CAPACITY(vm->with_stack_capacity);
                    Value* new_array = REALLOCATE_ARRAY(vm, Value, vm->with_stack, vm->with_stack_capacity, new_capacity);
                    if (!new_array) {
                        pyro_panic(vm, "out of memory: failed to allocate memory for the 'with' stack");
                        break;
                    }
                    vm->with_stack_capacity = new_capacity;
                    vm->with_stack = new_array;
                }
                vm->with_stack[vm->with_stack_count++] = pyro_peek(vm, 0);
                break;
            }

            case OP_END_WITH: {
                Value receiver = vm->with_stack[vm->with_stack_count - 1];
                call_end_with_method(vm, receiver);
                vm->with_stack_count--;
                break;
            }

            default:
                pyro_panic(vm, "invalid opcode");
                break;
        }
    }

    while (vm->with_stack_count > with_stack_count_on_entry) {
        Value receiver = vm->with_stack[vm->with_stack_count - 1];
        call_end_with_method(vm, receiver);
        vm->with_stack_count--;
    }

    #undef READ_BE_U16
    #undef READ_STRING
    #undef READ_CONSTANT
    #undef READ_BYTE
}


void pyro_reset_vm(PyroVM* vm) {
    vm->memory_allocation_failed = false;
    vm->halt_flag = false;
    vm->panic_flag = false;
    vm->panic_count = 0;
    vm->exit_flag = false;
    vm->exit_code = 0;
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}


void pyro_exec_code_as_main(PyroVM* vm, const char* code, size_t code_length, const char* source_id) {
    ObjPyroFn* fn = pyro_compile(vm, code, code_length, source_id);
    if (vm->halt_flag) {
        return;
    }

    ObjClosure* closure = ObjClosure_new(vm, fn, vm->main_module);
    if (!closure) {
        pyro_panic(vm, "out of memory");
        return;
    }

    #ifdef DEBUG
        Value* saved_stack_top = vm->stack_top;
    #endif

    pyro_push(vm, pyro_make_obj(closure));
    call_value(vm, 0);
    run(vm);
    pyro_pop(vm);

    #ifdef DEBUG
        if (!vm->halt_flag) {
            assert(vm->stack_top == saved_stack_top);
        }
    #endif
}


void pyro_exec_file_as_main(PyroVM* vm, const char* path) {
    ObjStr* path_string = ObjStr_new(path, vm);
    if (!path_string) {
        pyro_panic(vm, "out of memory");
        return;
    }

    if (!pyro_define_pri_member(vm, vm->main_module, "$filepath", pyro_make_obj(path_string))) {
        pyro_panic(vm, "out of memory");
        return;
    }

    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }

    pyro_exec_code_as_main(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
}


void pyro_exec_path_as_main(PyroVM* vm, const char* path) {
    if (pyro_is_file(path)) {
        pyro_exec_file_as_main(vm, path);
        return;
    }

    if (pyro_is_dir(path)) {
        size_t path_length = strlen(path);
        char* filepath = malloc(path_length + strlen("/self.pyro") + 1);
        if (!filepath) {
            pyro_panic(vm, "out of memory");
            return;
        }

        memcpy(filepath, path, path_length);
        memcpy(&filepath[path_length], "/self.pyro", strlen("/self.pyro"));
        filepath[path_length + strlen("/self.pyro")] = '\0';

        if (pyro_is_file(filepath)) {
            pyro_exec_file_as_main(vm, filepath);
        } else {
            pyro_panic(vm, "directory has no 'self.pyro' file: '%s'", path);
        }

        free(filepath);
        return;
    }

    pyro_panic(vm, "invalid path '%s'", path);
}


void pyro_try_compile_file(PyroVM* vm, const char* path) {
    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }
    pyro_compile(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
}


void pyro_exec_code_as_module(
    PyroVM* vm,
    const char* code,
    size_t code_length,
    const char* source_id,
    ObjModule* module
) {
    ObjPyroFn* fn = pyro_compile(vm, code, code_length, source_id);
    if (vm->halt_flag) {
        return;
    }

    ObjClosure* closure = ObjClosure_new(vm, fn, module);
    if (!closure) {
        pyro_panic(vm, "out of memory");
        return;
    }

    #ifdef DEBUG
        Value* saved_stack_top = vm->stack_top;
    #endif

    pyro_push(vm, pyro_make_obj(closure));
    call_value(vm, 0);
    run(vm);
    pyro_pop(vm);

    #ifdef DEBUG
        if (!vm->halt_flag) {
            assert(vm->stack_top == saved_stack_top);
        }
    #endif
}


void pyro_exec_file_as_module(PyroVM* vm, const char* path, ObjModule* module) {
    ObjStr* path_string = ObjStr_new(path, vm);
    if (!path_string) {
        pyro_panic(vm, "out of memory");
        return;
    }

    if (!pyro_define_pri_member(vm, module, "$filepath", pyro_make_obj(path_string))) {
        pyro_panic(vm, "out of memory");
        return;
    }

    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }

    ObjPyroFn* fn = pyro_compile(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
    if (vm->halt_flag) {
        return;
    }

    ObjClosure* closure = ObjClosure_new(vm, fn, module);
    if (!closure) {
        pyro_panic(vm, "out of memory");
        return;
    }

    pyro_push(vm, pyro_make_obj(closure));
    call_value(vm, 0);
    run(vm);
    pyro_pop(vm);
}


void pyro_run_main_func(PyroVM* vm) {
    ObjStr* main_string = ObjStr_new("$main", vm);
    if (!main_string) {
        pyro_panic(vm, "out of memory");
        return;
    }

    Value main_index;
    if (ObjMap_get(vm->main_module->all_member_indexes, pyro_make_obj(main_string), &main_index, vm)) {
        Value main_value = vm->main_module->members->values[main_index.as.i64];
        if (IS_CLOSURE(main_value)) {
            if (AS_CLOSURE(main_value)->fn->arity == 0) {
                pyro_push(vm, main_value);
                call_value(vm, 0);
                run(vm);
                pyro_pop(vm);
            } else {
                pyro_panic(vm, "invalid $main() function, must take 0 arguments");
            }
        } else {
            pyro_panic(vm, "invalid $main, must be a function");
        }
    }
}


Value pyro_call_method(PyroVM* vm, Value method, uint8_t arg_count) {
    if (IS_NATIVE_FN(method)) {
        call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
        return pyro_pop(vm);
    } else if (IS_CLOSURE(method)) {
        call_closure(vm, AS_CLOSURE(method), arg_count);
        run(vm);
        return pyro_pop(vm);
    } else {
        pyro_panic(vm, "value is not callable as a method");
        return pyro_make_null();
    }
}


Value pyro_call_function(PyroVM* vm, uint8_t arg_count) {
    Value value = pyro_peek(vm, arg_count);
    if (IS_NATIVE_FN(value)) {
        call_native_fn(vm, AS_NATIVE_FN(value), arg_count);
        return pyro_pop(vm);
    } else {
        call_value(vm, arg_count);
        run(vm);
        return pyro_pop(vm);
    }
}
