#include "../inc/pyro.h"


// Pushes a new call frame onto the call stack.
// - [frame_pointer] points to the frame's zeroth local variable slot on the value stack.
// - If [closure] is a function, the zeroth local variable slot will be unused.
// - If [closure] is a method, the zeroth local variable slot will contain 'self'.
static void push_call_frame(PyroVM* vm, PyroClosure* closure, PyroValue* frame_pointer) {
    if (vm->frame_count == vm->frame_capacity) {
        size_t new_capacity = PYRO_GROW_CAPACITY(vm->frame_capacity);
        PyroCallFrame* new_array = PYRO_REALLOCATE_ARRAY(vm, PyroCallFrame, vm->frames, vm->frame_capacity, new_capacity);
        if (!new_array) {
            pyro_panic(vm, "out of memory");
            return;
        }
        vm->frames = new_array;
        vm->frame_capacity = new_capacity;
    }

    PyroCallFrame* frame = &vm->frames[vm->frame_count];
    vm->frame_count++;

    frame->closure = closure;
    frame->ip = closure->fn->code;
    frame->fp = frame_pointer;
    frame->with_stack_count_on_entry = vm->with_stack_count;
}


// - If we're calling [closure] as a function, the [closure] object and [arg_count] arguments
//   should be sitting on top of the stack.
// - If we're calling [closure] as a method, the receiver object and [arg_count] arguments
//   should be sitting on top of the stack.
static void call_closure(PyroVM* vm, PyroClosure* closure, uint8_t arg_count) {
    PyroValue* frame_pointer = vm->stack_top - arg_count - 1;

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
        PyroTup* tup = PyroTup_new(num_variadic_args, vm);
        if (!tup) {
            pyro_panic(vm, "out of memory");
            return;
        }
        if (num_variadic_args > 0) {
            memcpy(tup->values, vm->stack_top - num_variadic_args, sizeof(PyroValue) * num_variadic_args);
            vm->stack_top -= num_variadic_args;
        }
        pyro_push(vm, pyro_obj(tup));
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
            PyroValue arg = closure->default_values->values[start_index + i];
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
static void call_native_fn(PyroVM* vm, PyroNativeFn* fn, uint8_t arg_count) {
    if (fn->arity == arg_count || fn->arity == -1) {
        PyroValue result = fn->fn_ptr(vm, arg_count, vm->stack_top - arg_count);
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
    PyroValue callee = vm->stack_top[-(int)arg_count - 1];

    if (!PYRO_IS_OBJ(callee)) {
        pyro_panic(vm, "value is not callable");
        return;
    }

    switch(PYRO_AS_OBJ(callee)->type) {
        case PYRO_OBJECT_BOUND_METHOD: {
            PyroBoundMethod* bm = PYRO_AS_BOUND_METHOD(callee);
            vm->stack_top[-arg_count - 1] = bm->receiver;

            switch (bm->method->type) {
                case PYRO_OBJECT_CLOSURE:
                    call_closure(vm, (PyroClosure*)bm->method, arg_count);
                    break;

                case PYRO_OBJECT_NATIVE_FN:
                    call_native_fn(vm, (PyroNativeFn*)bm->method, arg_count);
                    break;

                default:
                    pyro_panic(vm, "invalid type for bound method");
                    break;
            }

            return;
        }

        case PYRO_OBJECT_CLASS: {
            PyroClass* class = PYRO_AS_CLASS(callee);
            PyroInstance* instance = PyroInstance_new(vm, class);
            if (!instance) {
                pyro_panic(vm, "out of memory");
                return;
            }
            vm->stack_top[-arg_count - 1] = pyro_obj(instance);

            if (PYRO_IS_NULL(class->init_method)) {
                if (arg_count > 0) {
                    pyro_panic(vm,
                        "%s(): expected 0 arguments for initializer, found %zu",
                        class->name->bytes,
                        arg_count
                    );
                }
                return;
            }

            switch (PYRO_AS_OBJ(class->init_method)->type) {
                case PYRO_OBJECT_CLOSURE:
                    call_closure(vm, PYRO_AS_CLOSURE(class->init_method), arg_count);
                    break;

                case PYRO_OBJECT_NATIVE_FN:
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(class->init_method), arg_count);
                    break;

                default:
                    pyro_panic(vm, "invalid type for init method");
                    break;
            }

            return;
        }

        case PYRO_OBJECT_CLOSURE: {
            call_closure(vm, PYRO_AS_CLOSURE(callee), arg_count);
            return;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            call_native_fn(vm, PYRO_AS_NATIVE_FN(callee), arg_count);
            return;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroClass* class = PYRO_AS_OBJ(callee)->class;

            PyroValue call_method;
            if (!PyroMap_get(class->all_instance_methods, pyro_obj(vm->str_dollar_call), &call_method, vm)) {
                pyro_panic(vm, "object is not callable");
                return;
            }

            switch (PYRO_AS_OBJ(call_method)->type) {
                case PYRO_OBJECT_CLOSURE:
                    call_closure(vm, PYRO_AS_CLOSURE(call_method), arg_count);
                    break;

                case PYRO_OBJECT_NATIVE_FN:
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(call_method), arg_count);
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


static PyroUpvalue* capture_upvalue(PyroVM* vm, PyroValue* local) {
    PyroUpvalue* prev_upvalue = NULL;
    PyroUpvalue* curr_upvalue = vm->open_upvalues;

    // The list is ordered with upvalues pointing to higher stack slots coming first.
    // We fast-forward through the list to the location of the possible match.
    while (curr_upvalue != NULL && curr_upvalue->location > local) {
        prev_upvalue = curr_upvalue;
        curr_upvalue = curr_upvalue->next;
    }

    // Check for an existing upvalue.
    if (curr_upvalue != NULL && curr_upvalue->location == local) {
        return curr_upvalue;
    }

    // No match so create a new upvalue object.
    PyroUpvalue* new_upvalue = PyroUpvalue_new(vm, local);
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
static void close_upvalues(PyroVM* vm, PyroValue* slot) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= slot) {
        PyroUpvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}


// Attempts to load the module identified by the [names] array. If the module has already been
// imported, this will return the cached module object. Otherwise, it will attempt to import,
// execute, and return the named module.
//
// This function will import ancestor modules along the [names] path if they haven't already
// been imported, i.e. if the path is 'foo::bar::baz' this function will first import 'foo',
// then 'foo::bar', then 'foo::bar::baz', returning the module 'baz'.
//
// This function can call into Pyro code and can set the panic or exit flags.
static PyroMod* load_module(PyroVM* vm, PyroValue* names, size_t name_count) {
    if (name_count > 16) {
        pyro_panic(vm, "import list is too long (max: 16 module names)");
        return NULL;
    }

    // Make a local copy of the [names] array in case the stack gets reallocated while we're
    // iterating over the values, invalidating the [names] pointer.
    PyroValue names_array[16];
    memcpy(names_array, names, name_count * sizeof(PyroValue));

    PyroMap* supermod_modules_map = vm->modules;
    PyroValue module_value;

    for (uint8_t i = 0; i < name_count; i++) {
        PyroValue name = names_array[i];

        if (PyroMap_get(supermod_modules_map, name, &module_value, vm)) {
            supermod_modules_map = PYRO_AS_MOD(module_value)->submodules;
            continue;
        }

        PyroMod* module_object = PyroMod_new(vm);
        if (!module_object) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        module_value = pyro_obj(module_object);
        if (PyroMap_set(supermod_modules_map, name, module_value, vm) == 0) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }

        pyro_import_module(vm, i + 1, names_array, module_object);
        if (vm->halt_flag) {
            PyroMap_remove(supermod_modules_map, name, vm);
            return NULL;
        }

        supermod_modules_map = module_object->submodules;
    }

    return PYRO_AS_MOD(module_value);
}

// Returns true if the value is 'kinda-falsey', i.e. one of: [false, null, err, 0, 0.0, ""].
static inline bool is_kinda_falsey(PyroValue value) {
    switch (value.type) {
        case PYRO_VALUE_BOOL:
            return value.as.boolean == false;
        case PYRO_VALUE_NULL:
            return true;
        case PYRO_VALUE_I64:
            return value.as.i64 == 0;
        case PYRO_VALUE_CHAR:
            return value.as.u32 == 0;
        case PYRO_VALUE_F64:
            return value.as.f64 == 0.0;
        case PYRO_VALUE_OBJ:
            return PYRO_IS_ERR(value) || (PYRO_IS_STR(value) && PYRO_AS_STR(value)->count == 0);
        default:
            return false;
    }
}

void call_end_with_method(PyroVM* vm, PyroValue receiver) {
    PyroValue end_with_method = pyro_get_method(vm, receiver, vm->str_dollar_end_with);
    if (PYRO_IS_NULL(end_with_method)) {
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

    // Reads the next two bytes from the bytecode as an index into the function's constant
    // table. Returns the constant as a [PyroValue].
    #define READ_CONSTANT() (frame->closure->fn->constants[READ_BE_U16()])

    // Reads the next two bytes from the bytecode as an index into the function's constant
    // table referencing a string value. Returns the value as a [PyroStr*].
    #define READ_STRING() PYRO_AS_STR(READ_CONSTANT())

    for (;;) {
        if (vm->halt_flag || vm->frame_count < frame_count_on_entry) {
            break;
        }

        // The last instruction may have changed the frame count or (this can lead to nasty
        // bugs) forced a reallocation of the frame stack so reset the frame pointer for
        // every iteration.
        PyroCallFrame* frame = &vm->frames[vm->frame_count - 1];

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

        #ifdef PYRO_DEBUG
            if (vm->trace_execution) {
                size_t ip = frame->ip - frame->closure->fn->code;
                pyro_disassemble_instruction(vm, frame->closure->fn, ip);

                pyro_stdout_write(vm, "             ");
                if (vm->stack == vm->stack_top) {
                    pyro_stdout_write(vm, "[ empty ]");
                }
                for (PyroValue* slot = vm->stack; slot < vm->stack_top; slot++) {
                    pyro_stdout_write(vm, "[ ");
                    pyro_dump_value(vm, *slot);
                    pyro_stdout_write(vm, " ]");
                }
                pyro_stdout_write(vm, "\n");
            }
        #endif

        switch (READ_BYTE()) {
            // Implements the expression: [left_operand + right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_PLUS: {
                vm->stack_top[-2] = pyro_op_binary_plus(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand - right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_MINUS: {
                vm->stack_top[-2] = pyro_op_binary_minus(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand * right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_STAR: {
                vm->stack_top[-2] = pyro_op_binary_star(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand ** right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_STAR_STAR: {
                vm->stack_top[-2] = pyro_op_binary_star_star(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand / right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_SLASH: {
                vm->stack_top[-2] = pyro_op_binary_slash(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand // right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_SLASH_SLASH: {
                vm->stack_top[-2] = pyro_op_binary_slash_slash(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand % right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_PERCENT: {
                vm->stack_top[-2] = pyro_op_binary_percent(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand & right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_AMP: {
                vm->stack_top[-2] = pyro_op_binary_amp(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand | right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_BAR: {
                vm->stack_top[-2] = pyro_op_binary_bar(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand ^ right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_CARET: {
                vm->stack_top[-2] = pyro_op_binary_caret(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand << right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_LESS_LESS: {
                vm->stack_top[-2] = pyro_op_binary_less_less(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }


            // Implements the expression: [left_operand >> right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_GREATER_GREATER: {
                vm->stack_top[-2] = pyro_op_binary_greater_greater(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand == right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_EQUAL_EQUAL: {
                vm->stack_top[-2] = pyro_bool(
                    pyro_op_compare_eq(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand != right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_BANG_EQUAL: {
                vm->stack_top[-2] = pyro_bool(
                    !pyro_op_compare_eq(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand > right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_GREATER: {
                vm->stack_top[-2] = pyro_bool(
                    pyro_op_compare_gt(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand >= right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_GREATER_EQUAL: {
                vm->stack_top[-2] = pyro_bool(
                    pyro_op_compare_ge(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand < right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_LESS: {
                vm->stack_top[-2] = pyro_bool(
                    pyro_op_compare_lt(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [left_operand <= right_operand].
            // Before: [ ... ][ left_operand ][ right_operand ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_LESS_EQUAL: {
                vm->stack_top[-2] = pyro_bool(
                    pyro_op_compare_le(
                        vm,
                        vm->stack_top[-2],
                        vm->stack_top[-1]
                    )
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [needle in haystack].
            // Before: [ ... ][ needle ][ haystack ]
            // After:  [ ... ][ result ]
            case PYRO_OPCODE_BINARY_IN: {
                PyroValue method = pyro_get_method(vm, vm->stack_top[-1], vm->str_dollar_contains);
                if (PYRO_IS_NULL(method)) {
                    pyro_panic(vm,
                        "invalid operand type for 'in' operator: '%s' has no $contains() method",
                        pyro_get_type_name(vm, vm->stack_top[-1])->bytes
                    );
                    break;
                }

                // Swap the values on the stack so we have:
                // Stack: [ ... ][ haystack ][ needle ]
                PyroValue temp = vm->stack_top[-2];
                vm->stack_top[-2] = vm->stack_top[-1];
                vm->stack_top[-1] = temp;

                PyroValue result = pyro_call_method(vm, method, 1);
                if (vm->halt_flag) {
                    break;
                }

                pyro_push(vm, pyro_bool(pyro_is_truthy(result)));
                break;
            }

            // Implements the expression: [~operand].
            // Before: [ ... ][ operand ]
            // After:  [ ... ][ ~operand ]
            case PYRO_OPCODE_UNARY_TILDE: {
                vm->stack_top[-1] = pyro_op_unary_tilde(vm, vm->stack_top[-1]);
                break;
            }

            // Implements the expression: [-operand].
            // Before: [ ... ][ operand ]
            // After:  [ ... ][ -operand ]
            case PYRO_OPCODE_UNARY_MINUS: {
                vm->stack_top[-1] = pyro_op_unary_minus(vm, vm->stack_top[-1]);
                break;
            }

            // Implements the expression: [+operand].
            // Before: [ ... ][ operand ]
            // After:  [ ... ][ +operand ]
            case PYRO_OPCODE_UNARY_PLUS: {
                vm->stack_top[-1] = pyro_op_unary_plus(vm, vm->stack_top[-1]);
                break;
            }

            // Implements the expression: [!operand].
            // Before: [ ... ][ operand ]
            // After:  [ ... ][ !operand ]
            case PYRO_OPCODE_UNARY_BANG: {
                vm->stack_top[-1] = pyro_bool(!pyro_is_truthy(vm->stack_top[-1]));
                break;
            }

            // Duplicates the top item on the stack.
            // Before: [ ... ][ value ]
            // After:  [ ... ][ value ][ value ]
            case PYRO_OPCODE_DUP: {
                pyro_push(vm, vm->stack_top[-1]);
                break;
            }

            // Duplicates the top 2 items on the stack.
            // Before: [ ... ][ value1 ][ value2 ]
            // After:  [ ... ][ value1 ][ value2 ][ value1 ][ value2 ]
            case PYRO_OPCODE_DUP_2: {
                pyro_push(vm, vm->stack_top[-2]);
                pyro_push(vm, vm->stack_top[-2]);
                break;
            }

            // Implements the statement: [assert <expression>].
            // Before: [ ... ][ value ]
            // After:  [ ... ]
            case PYRO_OPCODE_ASSERT: {
                PyroValue expr = pyro_pop(vm);
                if (!pyro_is_truthy(expr)) {
                    pyro_panic(vm, "assertion failed");
                }
                break;
            }

            // Implements the expression: [callee(arg1, arg2, arg3, ...)].
            // Before: [ ... ][ callee ][ arg1 ][ arg2 ][ arg3 ]
            // After:  [ ... ][ return_value ]
            case PYRO_OPCODE_CALL_VALUE: {
                uint8_t arg_count = READ_BYTE();
                call_value(vm, arg_count);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_0: {
                call_value(vm, 0);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_1: {
                call_value(vm, 1);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_2: {
                call_value(vm, 2);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_3: {
                call_value(vm, 3);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_4: {
                call_value(vm, 4);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_5: {
                call_value(vm, 5);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_6: {
                call_value(vm, 6);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_7: {
                call_value(vm, 7);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_8: {
                call_value(vm, 8);
                break;
            }

            case PYRO_OPCODE_CALL_VALUE_9: {
                call_value(vm, 9);
                break;
            }

            // Implements the expression: [callee(arg1, arg2, ..., *args)], where [args] is a
            // vector or tuple to be unpacked.
            // Before: [ ... ][ callee ][ arg1 ][ arg2 ][ args ]
            // After:  [ ... ][ return_value ]
            case PYRO_OPCODE_CALL_VALUE_WITH_UNPACK: {
                uint8_t arg_count = READ_BYTE();
                PyroValue last_arg = pyro_pop(vm);
                arg_count--;

                PyroValue* values;
                size_t value_count;

                if (PYRO_IS_TUP(last_arg)) {
                    values = PYRO_AS_TUP(last_arg)->values;
                    value_count = PYRO_AS_TUP(last_arg)->count;
                } else if (PYRO_IS_VEC(last_arg)) {
                    values = PYRO_AS_VEC(last_arg)->values;
                    value_count = PYRO_AS_VEC(last_arg)->count;
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

            // Pushes a new class object onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_MAKE_CLASS: {
                PyroClass* class = PyroClass_new(vm);
                if (!class) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                class->name = READ_STRING();
                pyro_push(vm, pyro_obj(class));
                break;
            }

            // Implements 'copy-down' inheritance for classes. The superclass object is left
            // behind on the stack as the target for the [super] variable.
            // Before: [ ... ][ superclass_object ][ subclass_object ]
            // After:  [ ... ][ superclass_object ]
            case PYRO_OPCODE_INHERIT: {
                if (!PYRO_IS_CLASS(vm->stack_top[-2])) {
                    pyro_panic(vm, "invalid superclass value (not a class)");
                    break;
                }

                PyroClass* superclass = PYRO_AS_CLASS(vm->stack_top[-2]);
                PyroClass* subclass = PYRO_AS_CLASS(vm->stack_top[-1]);

                if (superclass == subclass) {
                    pyro_panic(vm, "a class cannot inherit from itself");
                    break;
                }

                // "Copy-down inheritance". We copy all the superclass's methods, field indexes,
                // and field initializers to the subclass. This means that there's no extra
                // runtime work involved in looking up inherited methods or fields.
                if (!PyroMap_copy_entries(superclass->all_instance_methods, subclass->all_instance_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroMap_copy_entries(superclass->pub_instance_methods, subclass->pub_instance_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroMap_copy_entries(superclass->all_field_indexes, subclass->all_field_indexes, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroMap_copy_entries(superclass->pub_field_indexes, subclass->pub_field_indexes, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroVec_copy_entries(superclass->default_field_values, subclass->default_field_values, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroMap_copy_entries(superclass->static_methods, subclass->static_methods, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                if (!PyroMap_copy_entries(superclass->static_fields, subclass->static_fields, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                subclass->init_method = superclass->init_method;
                subclass->superclass = superclass;

                pyro_pop(vm);
                break;
            }

            // Implements the expression: [receiver.field_name].
            // Before: [ ... ][ receiver ]
            // After:  [ ... ][ field_value ]
            case PYRO_OPCODE_GET_FIELD: {
                PyroValue field_name = READ_CONSTANT();
                PyroValue receiver = vm->stack_top[-1];

                if (PYRO_IS_INSTANCE(receiver)) {
                    PyroInstance* instance = PYRO_AS_INSTANCE(receiver);
                    PyroValue field_index;
                    if (PyroMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        // Replace the instance with the field value.
                        vm->stack_top[-1] = instance->fields[field_index.as.i64];
                        break;
                    }
                }

                if (PYRO_IS_CLASS(receiver)) {
                    PyroClass* class = PYRO_AS_CLASS(receiver);
                    PyroValue value;
                    if (PyroMap_get(class->static_fields, field_name, &value, vm)) {
                        // Replace the class with the field value.
                        vm->stack_top[-1] = value;
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", PYRO_AS_STR(field_name)->bytes);
                break;
            }

            // Implements the expression: [receiver.field_name].
            // Before: [ ... ][ receiver ]
            // After:  [ ... ][ field_value ]
            case PYRO_OPCODE_GET_PUB_FIELD: {
                PyroValue field_name = READ_CONSTANT();
                PyroValue receiver = vm->stack_top[-1];

                if (PYRO_IS_INSTANCE(receiver)) {
                    PyroInstance* instance = PYRO_AS_INSTANCE(receiver);
                    PyroValue field_index;
                    if (PyroMap_get(instance->obj.class->pub_field_indexes, field_name, &field_index, vm)) {
                        // Replace the instance with the field value.
                        vm->stack_top[-1] = instance->fields[field_index.as.i64];
                        break;
                    } else if (PyroMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        pyro_panic(vm, "field '%s' is private", PYRO_AS_STR(field_name)->bytes);
                        break;
                    }
                }

                if (PYRO_IS_CLASS(receiver)) {
                    PyroClass* class = PYRO_AS_CLASS(receiver);
                    PyroValue value;
                    if (PyroMap_get(class->static_fields, field_name, &value, vm)) {
                        // Replace the class with the field value.
                        vm->stack_top[-1] = value;
                        break;
                    }
                }

                if (PYRO_IS_MOD(receiver)) {
                    pyro_panic(vm,
                        "invalid field name '.%s'; receiver is a module, did you mean to use '::'?",
                        PYRO_AS_STR(field_name)->bytes
                    );
                    break;
                }

                pyro_panic(vm, "invalid field name '%s'", PYRO_AS_STR(field_name)->bytes);
                break;
            }

            // Implements the expression: [receiver.field_name = value].
            // Before: [ ... ][ receiver ][ value ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_SET_FIELD: {
                PyroValue field_name = READ_CONSTANT();
                PyroValue receiver = vm->stack_top[-2];
                PyroValue value = vm->stack_top[-1];

                if (PYRO_IS_INSTANCE(receiver)) {
                    PyroInstance* instance = PYRO_AS_INSTANCE(receiver);
                    PyroValue field_index;
                    if (PyroMap_get(instance->obj.class->all_field_indexes, field_name, &field_index, vm)) {
                        instance->fields[field_index.as.i64] = value;
                        vm->stack_top[-2] = value;
                        vm->stack_top--;
                        break;
                    }
                }

                if (PYRO_IS_CLASS(receiver)) {
                    PyroClass* class = PYRO_AS_CLASS(receiver);
                    if (PyroMap_contains(class->static_fields, field_name, vm)) {
                        PyroMap_set(class->static_fields, field_name, value, vm);
                        vm->stack_top[-2] = value;
                        vm->stack_top--;
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", PYRO_AS_STR(field_name)->bytes);
                break;
            }

            // Implements the expression: [receiver.field_name = value].
            // Before: [ ... ][ receiver ][ value ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_SET_PUB_FIELD: {
                PyroValue field_name = READ_CONSTANT();
                PyroValue receiver = vm->stack_top[-2];
                PyroValue value = vm->stack_top[-1];

                if (PYRO_IS_INSTANCE(receiver)) {
                    PyroInstance* instance = PYRO_AS_INSTANCE(receiver);
                    PyroValue field_index;
                    if (PyroMap_get(instance->obj.class->pub_field_indexes, field_name, &field_index, vm)) {
                        instance->fields[field_index.as.i64] = value;
                        vm->stack_top[-2] = value;
                        vm->stack_top--;
                        break;
                    }
                }

                if (PYRO_IS_CLASS(receiver)) {
                    PyroClass* class = PYRO_AS_CLASS(receiver);
                    if (PyroMap_contains(class->static_fields, field_name, vm)) {
                        PyroMap_set(class->static_fields, field_name, value, vm);
                        vm->stack_top[-2] = value;
                        vm->stack_top--;
                        break;
                    }
                }

                pyro_panic(vm, "invalid field name '%s'", PYRO_AS_STR(field_name)->bytes);
                break;
            }

            // Implements the expression: [receiver[key]].
            // Before: [ ... ][ receiver ][ key ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_GET_INDEX: {
                vm->stack_top[-2] = pyro_op_get_index(
                    vm,
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top--;
                break;
            }

            // Implements the expression: [receiver[key] = value].
            // Before: [ ... ][ receiver ][ key ][ value ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_SET_INDEX: {
                vm->stack_top[-3] = pyro_op_set_index(
                    vm,
                    vm->stack_top[-3],
                    vm->stack_top[-2],
                    vm->stack_top[-1]
                );
                vm->stack_top -= 2;
                break;
            }

            // Loads the local variable at the specified index onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_GET_LOCAL: {
                uint8_t index = READ_BYTE();
                pyro_push(vm, frame->fp[index]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_0: {
                pyro_push(vm, frame->fp[0]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_1: {
                pyro_push(vm, frame->fp[1]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_2: {
                pyro_push(vm, frame->fp[2]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_3: {
                pyro_push(vm, frame->fp[3]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_4: {
                pyro_push(vm, frame->fp[4]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_5: {
                pyro_push(vm, frame->fp[5]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_6: {
                pyro_push(vm, frame->fp[6]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_7: {
                pyro_push(vm, frame->fp[7]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_8: {
                pyro_push(vm, frame->fp[8]);
                break;
            }

            case PYRO_OPCODE_GET_LOCAL_9: {
                pyro_push(vm, frame->fp[9]);
                break;
            }

            // Loads a global variable onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_GET_GLOBAL: {
                PyroValue name = READ_CONSTANT();

                PyroValue member_index;
                if (PyroMap_get(frame->closure->module->all_member_indexes, name, &member_index, vm)) {
                    PyroValue value = frame->closure->module->members->values[member_index.as.i64];
                    pyro_push(vm, value);
                    break;
                }

                PyroValue value;
                if (PyroMap_get(vm->superglobals, name, &value, vm)) {
                    pyro_push(vm, value);
                    break;
                }

                pyro_panic(vm, "undefined variable '%s'", PYRO_AS_STR(name)->bytes);
                break;
            }

            // Assigns to a global variable.
            // Before: [ ... ][ value ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_SET_GLOBAL: {
                PyroValue name = READ_CONSTANT();
                PyroValue value = vm->stack_top[-1];

                PyroValue member_index;
                if (PyroMap_get(frame->closure->module->all_member_indexes, name, &member_index, vm)) {
                    frame->closure->module->members->values[member_index.as.i64] = value;
                    break;
                }

                if (PyroMap_contains(vm->superglobals, name, vm)) {
                    pyro_panic(vm, "cannot assign to superglobal '%s'", PYRO_AS_STR(name)->bytes);
                    break;
                }

                pyro_panic(vm, "undefined variable '%s'", PYRO_AS_STR(name)->bytes);
                break;
            }

            // Pushes the value [null] onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ null ]
            case PYRO_OPCODE_LOAD_NULL:
                pyro_push(vm, pyro_null());
                break;

            // Pushes the value [true] onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ true ]
            case PYRO_OPCODE_LOAD_TRUE:
                pyro_push(vm, pyro_bool(true));
                break;

            // Pushes the value [false] onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ false ]
            case PYRO_OPCODE_LOAD_FALSE:
                pyro_push(vm, pyro_bool(false));
                break;

            // Pushes a value from the function's constant table onto the stack.
            // Before: [ ... ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_LOAD_CONSTANT: {
                PyroValue constant = READ_CONSTANT();
                pyro_push(vm, constant);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_0: {
                pyro_push(vm, frame->closure->fn->constants[0]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_1: {
                pyro_push(vm, frame->closure->fn->constants[1]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_2: {
                pyro_push(vm, frame->closure->fn->constants[2]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_3: {
                pyro_push(vm, frame->closure->fn->constants[3]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_4: {
                pyro_push(vm, frame->closure->fn->constants[4]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_5: {
                pyro_push(vm, frame->closure->fn->constants[5]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_6: {
                pyro_push(vm, frame->closure->fn->constants[6]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_7: {
                pyro_push(vm, frame->closure->fn->constants[7]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_8: {
                pyro_push(vm, frame->closure->fn->constants[8]);
                break;
            }

            case PYRO_OPCODE_LOAD_CONSTANT_9: {
                pyro_push(vm, frame->closure->fn->constants[9]);
                break;
            }

            case PYRO_OPCODE_LOAD_I64_0:
                pyro_push(vm, pyro_i64(0));
                break;

            case PYRO_OPCODE_LOAD_I64_1:
                pyro_push(vm, pyro_i64(1));
                break;

            case PYRO_OPCODE_LOAD_I64_2:
                pyro_push(vm, pyro_i64(2));
                break;

            case PYRO_OPCODE_LOAD_I64_3:
                pyro_push(vm, pyro_i64(3));
                break;

            case PYRO_OPCODE_LOAD_I64_4:
                pyro_push(vm, pyro_i64(4));
                break;

            case PYRO_OPCODE_LOAD_I64_5:
                pyro_push(vm, pyro_i64(5));
                break;

            case PYRO_OPCODE_LOAD_I64_6:
                pyro_push(vm, pyro_i64(6));
                break;

            case PYRO_OPCODE_LOAD_I64_7:
                pyro_push(vm, pyro_i64(7));
                break;

            case PYRO_OPCODE_LOAD_I64_8:
                pyro_push(vm, pyro_i64(8));
                break;

            case PYRO_OPCODE_LOAD_I64_9:
                pyro_push(vm, pyro_i64(9));
                break;

            // Stringifies the value on top of the stack.
            // Before: [ ... ][ value ]
            // After:  [ ... ][ string ]
            case PYRO_OPCODE_STRINGIFY: {
                vm->stack_top[-1] = pyro_obj(
                    pyro_stringify_value(vm, vm->stack_top[-1])
                );
                break;
            }

            // Formats a value using a format string and pushes the result onto the stack.
            // Before: [ ... ][ value ][ format_string ]
            // After:  [ ... ][ string ]
            case PYRO_OPCODE_FORMAT: {
                vm->stack_top[-2] = pyro_obj(
                    pyro_format_value(
                        vm,
                        vm->stack_top[-2],
                        PYRO_AS_STR(vm->stack_top[-1])->bytes,
                        "formatting error"
                    )
                );
                vm->stack_top--;
                break;
            }

            // Concatenates [arg_count] strings, where [arg_count >= 2].
            // Before: [ ... ][ string1 ][ string2 ][ string3 ]
            // After:  [ ... ][ string ]
            case PYRO_OPCODE_CONCAT_STRINGS: {
                int arg_count = (int)READ_BE_U16();

                size_t required_capacity = 0;
                for (int i = 0; i < arg_count; i++) {
                    required_capacity += PYRO_AS_STR(vm->stack_top[-arg_count + i])->count;
                }
                required_capacity += 1;

                char* array = PYRO_ALLOCATE_ARRAY(vm, char, required_capacity);
                if (!array) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                size_t byte_count = 0;
                for (int i = 0; i < arg_count; i++) {
                    PyroStr* string = PYRO_AS_STR(vm->stack_top[-arg_count + i]);
                    memcpy(&array[byte_count], string->bytes, string->count);
                    byte_count += string->count;
                }
                array[byte_count] = '\0';

                PyroStr* result = PyroStr_take(array, byte_count, required_capacity, vm);
                if (!result) {
                    pyro_panic(vm, "out of memory");
                    PYRO_FREE_ARRAY(vm, char, array, required_capacity);
                    break;
                }

                vm->stack_top[-arg_count] = pyro_obj(result);
                vm->stack_top -= (arg_count - 1);
                break;
            }

            // Pops and discards the value on top of the stack.
            // Before: [ ... ][ value ]
            // After:  [ ... ]
            case PYRO_OPCODE_POP:
                vm->stack_top--;
                break;

            // Pushes the value of a module member onto the stack.
            // Before: [ ... ][ module ]
            // After:  [ ... ][ value ]
            case PYRO_OPCODE_GET_MEMBER: {
                PyroValue member_name = READ_CONSTANT();
                PyroValue receiver = vm->stack_top[-1];

                if (!PYRO_IS_MOD(receiver)) {
                    if (PYRO_IS_INSTANCE(receiver)) {
                        pyro_panic(vm,
                            "invalid member access '::%s', receiver is an object instance, did you mean to use ':'?",
                            PYRO_AS_STR(member_name)->bytes
                        );
                        break;
                    }
                    if (PYRO_IS_CLASS(receiver)) {
                        pyro_panic(vm,
                            "invalid member access '::%s', receiver is a class, did you mean to use ':'?",
                            PYRO_AS_STR(member_name)->bytes
                        );
                        break;
                    }
                    pyro_panic(vm,
                        "invalid member access '::%s', receiver is not a module",
                        PYRO_AS_STR(member_name)->bytes
                    );
                    break;
                }

                PyroMod* module = PYRO_AS_MOD(receiver);
                PyroValue member_index;
                if (PyroMap_get(module->pub_member_indexes, member_name, &member_index, vm)) {
                    vm->stack_top[-1] = module->members->values[member_index.as.i64];
                    break;
                }

                if (PyroMap_get(module->all_member_indexes, member_name, &member_index, vm)) {
                    pyro_panic(vm, "module member '%s' is private", PYRO_AS_STR(member_name)->bytes);
                    break;
                }

                pyro_panic(vm, "module has no member '%s'", PYRO_AS_STR(member_name)->bytes);
                break;
            }

            // Loads a bound method onto the stack.
            // Before: [ ... ][ receiver ]
            // After:  [ ... ][ bound_method ]
            case PYRO_OPCODE_GET_METHOD: {
                PyroStr* method_name = READ_STRING();
                PyroValue receiver = vm->stack_top[-1];

                PyroValue method = pyro_get_method(vm, receiver, method_name);
                if (PYRO_IS_NULL(method)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                PyroBoundMethod* bound_method = PyroBoundMethod_new(vm, receiver, PYRO_AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                vm->stack_top[-1] = pyro_obj(bound_method);
                break;
            }

            // Loads a bound method onto the stack.
            // Before: [ ... ][ receiver ]
            // After:  [ ... ][ bound_method ]
            case PYRO_OPCODE_GET_PUB_METHOD: {
                PyroStr* method_name = READ_STRING();
                PyroValue receiver = vm->stack_top[-1];

                PyroValue method = pyro_get_pub_method(vm, receiver, method_name);
                if (PYRO_IS_NULL(method)) {
                    if (PYRO_IS_NULL(pyro_get_method(vm, receiver, method_name))) {
                        if (PYRO_IS_MOD(receiver)) {
                            pyro_panic(vm,
                                "invalid method name ':%s'; receiver is a module, did you mean to use '::'?",
                                method_name->bytes
                            );
                            break;
                        }
                        pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                        break;
                    }
                    pyro_panic(vm, "method '%s' is private", method_name->bytes);
                    break;
                }

                PyroBoundMethod* bound_method = PyroBoundMethod_new(vm, receiver, PYRO_AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                vm->stack_top[-1] = pyro_obj(bound_method);
                break;
            }

            // Loads a bound superclass-method onto the stack.
            // Before: [ ... ][ receiver ][ superclass ]
            // After:  [ ... ][ bound_method ]
            case PYRO_OPCODE_GET_SUPER_METHOD: {
                PyroStr* method_name = READ_STRING();
                PyroValue receiver = vm->stack_top[-2];
                PyroClass* superclass = PYRO_AS_CLASS(vm->stack_top[-1]);

                PyroValue method;
                if (!PyroMap_get(superclass->all_instance_methods, pyro_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                PyroBoundMethod* bound_method = PyroBoundMethod_new(vm, receiver, PYRO_AS_OBJ(method));
                if (!bound_method) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                vm->stack_top[-2] = pyro_obj(bound_method);
                vm->stack_top--;
                break;
            }

            // Replaces the iterable value on top of the stack with an iterator.
            // Before: [ ... ][ iterable ]
            // After:  [ ... ][ iterator ]
            case PYRO_OPCODE_GET_ITERATOR: {
                PyroValue receiver = vm->stack_top[-1];
                PyroValue iter_method = pyro_get_method(vm, receiver, vm->str_dollar_iter);

                if (PYRO_IS_NATIVE_FN(iter_method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(iter_method), 0);
                    break;
                }

                if (PYRO_IS_CLOSURE(iter_method)) {
                    call_closure(vm, PYRO_AS_CLOSURE(iter_method), 0);
                    break;
                }

                pyro_panic(vm,
                    "type '%s' is not iterable",
                    pyro_get_type_name(vm, receiver)->bytes
                );
                break;
            }

            // Pushes the next value from an iterator onto the stack.
            // Before: [ ... ][ iterator ]
            // After:  [ ... ][ iterator ][ value ]
            case PYRO_OPCODE_GET_NEXT_FROM_ITERATOR: {
                PyroValue receiver = vm->stack_top[-1];

                if (PYRO_IS_ITER(receiver)) {
                    PyroValue next_value = PyroIter_next(PYRO_AS_ITER(receiver), vm);
                    pyro_push(vm, next_value);
                    break;
                }

                PyroValue next_method = pyro_get_method(vm, receiver, vm->str_dollar_next);

                if (PYRO_IS_NATIVE_FN(next_method)) {
                    if (!pyro_push(vm, receiver)) break;
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(next_method), 0);
                    break;
                }

                if (PYRO_IS_CLOSURE(next_method)) {
                    if (!pyro_push(vm, receiver)) break;
                    call_closure(vm, PYRO_AS_CLOSURE(next_method), 0);
                    break;
                }

                pyro_panic(vm, "invalid iterator: no :$next() method");
                break;
            }

            // Calls a method and pushes its return value on to the stack.
            // Before: [ ... ][ receiver ][ arg1 ][ arg2 ][ arg3 ]
            // After:  [ ... ][ return_value ]
            case PYRO_OPCODE_CALL_METHOD: {
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                PyroValue receiver = vm->stack_top[-(int)arg_count - 1];
                PyroValue method = pyro_get_method(vm, receiver, method_name);

                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), arg_count);
                    break;
                }

                if (PYRO_IS_CLOSURE(method)) {
                    call_closure(vm, PYRO_AS_CLOSURE(method), arg_count);
                    break;
                }

                pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                break;
            }

            // Calls a method and pushes its return value on to the stack.
            // Before: [ ... ][ receiver ][ arg1 ][ arg2 ][ arg3 ]
            // After:  [ ... ][ return_value ]
            case PYRO_OPCODE_CALL_PUB_METHOD: {
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                PyroValue receiver = vm->stack_top[-(int)arg_count - 1];
                PyroValue method = pyro_get_pub_method(vm, receiver, method_name);

                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), arg_count);
                    break;
                }

                if (PYRO_IS_CLOSURE(method)) {
                    call_closure(vm, PYRO_AS_CLOSURE(method), arg_count);
                    break;
                }

                if (!PYRO_IS_NULL(pyro_get_method(vm, receiver, method_name))) {
                    pyro_panic(vm, "method '%s' is private", method_name->bytes);
                    break;
                }

                if (PYRO_IS_MOD(receiver)) {
                    pyro_panic(vm,
                        "invalid method name ':%s'; receiver is a module, did you mean to use '::'?",
                        method_name->bytes
                    );
                    break;
                }

                pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP: {
                uint16_t offset = READ_BE_U16();
                frame->ip += offset;
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_BACK: {
                uint16_t offset = READ_BE_U16();
                frame->ip -= offset;
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_TRUE: {
                uint16_t offset = READ_BE_U16();
                if (pyro_is_truthy(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_FALSE: {
                uint16_t offset = READ_BE_U16();
                if (!pyro_is_truthy(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_POP_JUMP_IF_FALSE: {
                uint16_t offset = READ_BE_U16();
                if (!pyro_is_truthy(pyro_pop(vm))) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_ERR: {
                uint16_t offset = READ_BE_U16();
                if (PYRO_IS_ERR(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_NOT_ERR: {
                uint16_t offset = READ_BE_U16();
                if (!PYRO_IS_ERR(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_NOT_NULL: {
                uint16_t offset = READ_BE_U16();
                if (!PYRO_IS_NULL(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Jumps the instruction pointer to the specified offset in the bytecode.
            case PYRO_OPCODE_JUMP_IF_NOT_KINDA_FALSEY: {
                uint16_t offset = READ_BE_U16();
                if (!is_kinda_falsey(vm->stack_top[-1])) {
                    frame->ip += offset;
                }
                break;
            }

            // Sets the local variable at [index] to the value on top of the stack.
            case PYRO_OPCODE_SET_LOCAL: {
                uint8_t index = READ_BYTE();
                frame->fp[index] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_0: {
                frame->fp[0] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_1: {
                frame->fp[1] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_2: {
                frame->fp[2] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_3: {
                frame->fp[3] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_4: {
                frame->fp[4] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_5: {
                frame->fp[5] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_6: {
                frame->fp[6] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_7: {
                frame->fp[7] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_8: {
                frame->fp[8] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_SET_LOCAL_9: {
                frame->fp[9] = vm->stack_top[-1];
                break;
            }

            // Pushes a new map object onto the stack.
            // Before: [ ... ][ key1 ][ value1 ][ key2 ][ value2 ]
            // After:  [ ... ][ map ]
            case PYRO_OPCODE_MAKE_MAP: {
                uint16_t entry_count = READ_BE_U16();

                PyroMap* map = PyroMap_new(vm);
                if (!map) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                // The entries are stored on the stack as [..][key][value][..] pairs.
                for (PyroValue* slot = vm->stack_top - entry_count * 2; slot < vm->stack_top; slot += 2) {
                    if (!PyroMap_set(map, slot[0], slot[1], vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= (entry_count * 2);
                pyro_push(vm, pyro_obj(map));
                break;
            }

            // Pushes a new set object onto the stack.
            // Before: [ ... ][ value1 ][ value2 ][ value3 ]
            // After:  [ ... ][ set ]
            case PYRO_OPCODE_MAKE_SET: {
                uint16_t entry_count = READ_BE_U16();

                PyroMap* map = PyroMap_new_as_set(vm);
                if (!map) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                for (PyroValue* slot = vm->stack_top - entry_count; slot < vm->stack_top; slot++) {
                    if (!PyroMap_set(map, *slot, pyro_null(), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= entry_count;
                pyro_push(vm, pyro_obj(map));
                break;
            }

            // Pushes a new vec object onto the stack.
            // Before: [ ... ][ value1 ][ value2 ][ value3 ]
            // After:  [ ... ][ vec ]
            case PYRO_OPCODE_MAKE_VEC: {
                uint16_t item_count = READ_BE_U16();

                PyroVec* vec = PyroVec_new_with_capacity(item_count, vm);
                if (!vec) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (item_count == 0) {
                    pyro_push(vm, pyro_obj(vec));
                    break;
                }

                memcpy(vec->values, vm->stack_top - item_count, sizeof(PyroValue) * item_count);
                vec->count = item_count;

                vm->stack_top -= item_count;
                pyro_push(vm, pyro_obj(vec));
                break;
            }

            // Implements the statement: [return value].
            // Before: [ ... ][ local0 = closure/receiver ][ local1 ][ local2 ][ return_value ]
            // After:  [ ... ][ return_value ]
            case PYRO_OPCODE_RETURN: {
                PyroValue return_value = vm->stack_top[-1];

                while (vm->with_stack_count > frame->with_stack_count_on_entry) {
                    PyroValue receiver = vm->with_stack[vm->with_stack_count - 1];
                    call_end_with_method(vm, receiver);
                    vm->with_stack_count--;
                }

                close_upvalues(vm, frame->fp);

                vm->stack_top = frame->fp + 1;
                vm->stack_top[-1] = return_value;

                vm->frame_count--;
                break;
            }

            // Implements the statement: [return value1, value2], i.e. [count >= 2].
            // Before: [ ... ][ local0 = closure/receiver ][ loc1 ][ loc2 ][ ret1 ][ ret2 ]
            // After:  [ ... ][ return_value = tuple ]
            case PYRO_OPCODE_RETURN_TUPLE: {
                uint8_t count = READ_BYTE();

                PyroTup* tup = PyroTup_new(count, vm);
                if (!tup) {
                    pyro_panic(vm, "out of memory");
                    break;
                }
                memcpy(tup->values, vm->stack_top - count, sizeof(PyroValue) * count);

                PyroValue return_value = pyro_obj(tup);
                vm->stack_top -= (count - 1);
                vm->stack_top[-1] = return_value;

                while (vm->with_stack_count > frame->with_stack_count_on_entry) {
                    PyroValue receiver = vm->with_stack[vm->with_stack_count - 1];
                    call_end_with_method(vm, receiver);
                    vm->with_stack_count--;
                }

                close_upvalues(vm, frame->fp);

                vm->stack_top = frame->fp + 1;
                vm->stack_top[-1] = return_value;

                vm->frame_count--;
                break;
            }

            // Defines a private method on a class.
            // Before: [ ... ][ class_object ][ method_closure ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_PRI_METHOD: {
                PyroValue method = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* name = READ_STRING();
                if (PyroMap_contains(class->pub_instance_methods, pyro_obj(name), vm)) {
                    pyro_panic(vm, "cannot override public method '%s' as private", name->bytes);
                    break;
                }

                if (!PyroMap_set(class->all_instance_methods, pyro_obj(name), method, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (name == vm->str_dollar_init) {
                    class->init_method = method;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a public method on a class.
            // Before: [ ... ][ class_object ][ method_closure ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_PUB_METHOD: {
                PyroValue method = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* name = READ_STRING();
                if (PyroMap_contains(class->all_instance_methods, pyro_obj(name), vm)) {
                    if (!PyroMap_contains(class->pub_instance_methods, pyro_obj(name), vm)) {
                        pyro_panic(vm, "cannot override private method '%s' as public", name->bytes);
                        break;
                    }
                }
                if (name->count > 0 && name->bytes[0] == '$') {
                    pyro_panic(vm, "$-prefixed method cannot be declared public");
                    break;
                }

                if (!PyroMap_set(class->all_instance_methods, pyro_obj(name), method, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (!PyroMap_set(class->pub_instance_methods, pyro_obj(name), method, vm)) {
                    PyroMap_remove(class->all_instance_methods, pyro_obj(name), vm);
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a static method on a class.
            // Before: [ ... ][ class_object ][ method_closure ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_STATIC_METHOD: {
                PyroValue method = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* name = READ_STRING();
                if (PyroMap_set(class->static_methods, pyro_obj(name), method, vm) == 0) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a private field on a class.
            // Before: [ ... ][ class_object ][ default_value ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_PRI_FIELD: {
                PyroValue default_value = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* field_name = READ_STRING();
                if (PyroMap_contains(class->all_field_indexes, pyro_obj(field_name), vm)) {
                    pyro_panic(vm, "the field '%s' already exists", field_name->bytes);
                    break;
                }

                size_t field_index = class->default_field_values->count;
                if (!PyroVec_append(class->default_field_values, default_value, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(class->all_field_indexes, pyro_obj(field_name), pyro_i64(field_index), vm) == 0) {
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a public field on a class.
            // Before: [ ... ][ class_object ][ default_value ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_PUB_FIELD: {
                PyroValue default_value = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* field_name = READ_STRING();
                if (PyroMap_contains(class->all_field_indexes, pyro_obj(field_name), vm)) {
                    pyro_panic(vm, "the field '%s' already exists", field_name->bytes);
                    break;
                }

                size_t field_index = class->default_field_values->count;
                if (!PyroVec_append(class->default_field_values, default_value, vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(class->all_field_indexes, pyro_obj(field_name), pyro_i64(field_index), vm) == 0) {
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(class->pub_field_indexes, pyro_obj(field_name), pyro_i64(field_index), vm) == 0) {
                    PyroMap_remove(class->all_field_indexes, pyro_obj(field_name), vm);
                    class->default_field_values->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a static field on a class.
            // Before: [ ... ][ class_object ][ default_value ]
            // After:  [ ... ][ class_object ]
            case PYRO_OPCODE_DEFINE_STATIC_FIELD: {
                PyroValue default_value = vm->stack_top[-1];
                PyroClass* class = PYRO_AS_CLASS(vm->stack_top[-2]);

                PyroStr* field_name = READ_STRING();
                if (PyroMap_contains(class->static_fields, pyro_obj(field_name), vm)) {
                    pyro_panic(vm, "the static field '%s' already exists", field_name->bytes);
                    break;
                }

                if (PyroMap_set(class->static_fields, pyro_obj(field_name), default_value, vm) == 0) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a private global variable.
            // Before: [ ... ][ value ]
            // After:  [ ... ]
            case PYRO_OPCODE_DEFINE_PRI_GLOBAL: {
                PyroMod* module = frame->closure->module;
                PyroValue name = READ_CONSTANT();

                if (strcmp(PYRO_AS_STR(name)->bytes, "_") == 0) {
                    pyro_pop(vm);
                    break;
                }

                if (PyroMap_contains(module->all_member_indexes, name, vm)) {
                    pyro_panic(vm, "the global variable '%s' already exists", PYRO_AS_STR(name)->bytes);
                    break;
                }

                size_t new_member_index = module->members->count;
                if (!PyroVec_append(module->members, vm->stack_top[-1], vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(module->all_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines a public global variable.
            // Before: [ ... ][ value ]
            // After:  [ ... ]
            case PYRO_OPCODE_DEFINE_PUB_GLOBAL: {
                PyroMod* module = frame->closure->module;
                PyroValue name = READ_CONSTANT();

                if (strcmp(PYRO_AS_STR(name)->bytes, "_") == 0) {
                    pyro_pop(vm);
                    break;
                }

                if (PyroMap_contains(module->all_member_indexes, name, vm)) {
                    pyro_panic(vm, "the global variable '%s' already exists", PYRO_AS_STR(name)->bytes);
                    break;
                }

                size_t new_member_index = module->members->count;
                if (!PyroVec_append(module->members, vm->stack_top[-1], vm)) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(module->all_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                if (PyroMap_set(module->pub_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                    PyroMap_remove(module->all_member_indexes, name, vm);
                    module->members->count--;
                    pyro_panic(vm, "out of memory");
                    break;
                }

                pyro_pop(vm);
                break;
            }

            // Defines multiple private global variables.
            // Before: [ ... ][ value1 ][ value2 ][ value3 ]
            // After:  [ ... ]
            case PYRO_OPCODE_DEFINE_PRI_GLOBALS: {
                PyroMod* module = frame->closure->module;
                uint8_t count = READ_BYTE();

                for (uint8_t i = 0; i < count; i++) {
                    PyroValue name = READ_CONSTANT();
                    if (strcmp(PYRO_AS_STR(name)->bytes, "_") == 0) {
                        continue;
                    }

                    if (PyroMap_contains(module->all_member_indexes, name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", PYRO_AS_STR(name)->bytes);
                        break;
                    }

                    PyroValue new_member_value = vm->stack_top[-(int)count + i];
                    size_t new_member_index = module->members->count;

                    if (!PyroVec_append(module->members, new_member_value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (PyroMap_set(module->all_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= count;
                break;
            }

            // Defines multiple public global variables.
            // Before: [ ... ][ value1 ][ value2 ][ value3 ]
            // After:  [ ... ]
            case PYRO_OPCODE_DEFINE_PUB_GLOBALS: {
                PyroMod* module = frame->closure->module;
                uint8_t count = READ_BYTE();

                for (uint8_t i = 0; i < count; i++) {
                    PyroValue name = READ_CONSTANT();
                    if (strcmp(PYRO_AS_STR(name)->bytes, "_") == 0) {
                        continue;
                    }

                    if (PyroMap_contains(module->all_member_indexes, name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", PYRO_AS_STR(name)->bytes);
                        break;
                    }

                    PyroValue new_member_value = vm->stack_top[-(int)count + i];
                    size_t new_member_index = module->members->count;

                    if (!PyroVec_append(module->members, new_member_value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (PyroMap_set(module->all_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (PyroMap_set(module->pub_member_indexes, name, pyro_i64(new_member_index), vm) == 0) {
                        PyroMap_remove(module->all_member_indexes, name, vm);
                        module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= count;
                break;
            }

            // Implements [echo value1, value2, value3].
            // Before: [ ... ][ value1 ][ value2 ][ value3 ]
            // After:  [ ... ]
            case PYRO_OPCODE_ECHO: {
                int arg_count = (int)READ_BYTE();

                for (int i = 0; i < arg_count; i++) {
                    PyroValue value = vm->stack_top[-arg_count + i];
                    PyroStr* string = pyro_stringify_value(vm, value);
                    if (vm->halt_flag) {
                        break;
                    }
                    if (pyro_stdout_write_s(vm, string) < 0) {
                        pyro_panic(vm, "echo: failed to write to the standard output stream");
                        break;
                    }
                    if (i < arg_count - 1) {
                        if (pyro_stdout_write_n(vm, " ", 1) < 0) {
                            pyro_panic(vm, "echo: failed to write to the standard output stream");
                            break;
                        }
                    }
                }

                if (pyro_stdout_write_n(vm, "\n", 1) < 0) {
                    pyro_panic(vm, "echo: failed to write to the standard output stream");
                    break;
                }

                vm->stack_top -= arg_count;
                break;
            }

            // Special opcode for printing the value of expression statements in the REPL.
            // Before: [ ... ][ value ]
            // After:  [ ... ]
            case PYRO_OPCODE_POP_ECHO_IN_REPL: {
                PyroValue value = vm->stack_top[-1];

                if (vm->in_repl && !PYRO_IS_NULL(value)) {
                    PyroStr* string = pyro_debugify_value(vm, value);
                    if (vm->halt_flag) {
                        break;
                    }
                    pyro_stdout_write_f(vm, "%s\n", string->bytes);
                }

                pyro_pop(vm);
                break;
            }

            // UNOPTIMIZED.
            // Opcodes below this point have not yet been optimized and documented.

            case PYRO_OPCODE_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack_top - 1);
                pyro_pop(vm);
                break;
            }

            case PYRO_OPCODE_MAKE_CLOSURE: {
                PyroFn* fn = PYRO_AS_PYRO_FN(READ_CONSTANT());
                PyroMod* module = frame->closure->module;

                PyroClosure* closure = PyroClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                for (size_t i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->fp + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                pyro_push(vm, pyro_obj(closure));
                break;
            }

            case PYRO_OPCODE_MAKE_CLOSURE_WITH_DEF_ARGS: {
                PyroFn* fn = PYRO_AS_PYRO_FN(READ_CONSTANT());
                PyroMod* module = frame->closure->module;

                PyroClosure* closure = PyroClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, "out of memory");
                    break;
                }

                uint8_t default_value_count = READ_BYTE();
                for (uint8_t i = 0; i < default_value_count; i++) {
                    PyroValue value = vm->stack_top[-default_value_count + i];
                    if (!PyroVec_append(closure->default_values, value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }
                vm->stack_top -= default_value_count;

                for (size_t i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(vm, frame->fp + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                pyro_push(vm, pyro_obj(closure));
                break;
            }

            case PYRO_OPCODE_GET_UPVALUE: {
                uint8_t index = READ_BYTE();
                PyroValue value = *frame->closure->upvalues[index]->location;
                pyro_push(vm, value);
                break;
            }

            // The import path is stored on the stack as an array of [arg_count] strings.
            case PYRO_OPCODE_IMPORT_MODULE: {
                uint8_t arg_count = READ_BYTE();
                PyroValue* args = vm->stack_top - arg_count;

                PyroMod* module = load_module(vm, args, arg_count);
                if (vm->halt_flag) {
                    break;
                }

                vm->stack_top -= arg_count;
                pyro_push(vm, pyro_obj(module));
                break;
            }

            case PYRO_OPCODE_IMPORT_ALL_MEMBERS: {
                uint8_t arg_count = READ_BYTE();
                PyroValue* args = vm->stack_top - arg_count;

                PyroMod* current_module = frame->closure->module;
                PyroMod* imported_module = load_module(vm, args, arg_count);
                if (vm->halt_flag) {
                    break;
                }

                for (size_t i = 0; i < imported_module->pub_member_indexes->entry_array_count; i++) {
                    PyroMapEntry* entry = &imported_module->pub_member_indexes->entry_array[i];
                    if (PYRO_IS_TOMBSTONE(entry->key)) {
                        continue;
                    }

                    PyroValue member_name = entry->key;
                    PyroValue member_index_in_imported_module = entry->value;
                    PyroValue value = imported_module->members->values[member_index_in_imported_module.as.i64];

                    if (PyroMap_contains(current_module->all_member_indexes, member_name, vm)) {
                        pyro_panic(vm, "the global variable '%s' already exists", PYRO_AS_STR(member_name)->bytes);
                        break;
                    }

                    size_t member_index_in_current_module = current_module->members->count;
                    if (!PyroVec_append(current_module->members, value, vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (PyroMap_set(current_module->all_member_indexes, member_name, pyro_i64(member_index_in_current_module), vm) == 0) {
                        current_module->members->count--;
                        pyro_panic(vm, "out of memory");
                        break;
                    }
                }

                vm->stack_top -= arg_count;
                break;
            }

            case PYRO_OPCODE_IMPORT_NAMED_MEMBERS: {
                uint8_t module_count = READ_BYTE();
                uint8_t member_count = READ_BYTE();
                PyroValue* args = vm->stack_top - module_count - member_count;

                PyroMod* module = load_module(vm, args, module_count);
                if (vm->halt_flag) {
                    break;
                }

                for (uint8_t i = 0; i < member_count; i++) {
                    PyroValue name = args[module_count + i];
                    PyroValue member_index;
                    if (!PyroMap_get(module->all_member_indexes, name, &member_index, vm)) {
                        pyro_panic(vm, "module has no member '%s'", PYRO_AS_STR(name)->bytes);
                        break;
                    }
                    args[i] = module->members->values[member_index.as.i64];
                }

                vm->stack_top -= module_count;
                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case PYRO_OPCODE_CALL_METHOD_WITH_UNPACK: {
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                PyroValue receiver = vm->stack_top[-(int)arg_count - 1];
                PyroValue last_arg = pyro_pop(vm);
                arg_count--;

                PyroValue* values;
                size_t value_count;

                if (PYRO_IS_TUP(last_arg)) {
                    values = PYRO_AS_TUP(last_arg)->values;
                    value_count = PYRO_AS_TUP(last_arg)->count;
                } else if (PYRO_IS_VEC(last_arg)) {
                    values = PYRO_AS_VEC(last_arg)->values;
                    value_count = PYRO_AS_VEC(last_arg)->count;
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

                PyroValue method = pyro_get_method(vm, receiver, method_name);
                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), total_args);
                    break;
                }
                if (PYRO_IS_CLOSURE(method)) {
                    call_closure(vm, PYRO_AS_CLOSURE(method), total_args);
                    break;
                }

                pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                break;
            }

            // The receiver value and [arg_count] arguments are sitting on top of the stack.
            case PYRO_OPCODE_CALL_PUB_METHOD_WITH_UNPACK: {
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                PyroValue receiver = vm->stack_top[-(int)arg_count - 1];
                PyroValue last_arg = pyro_pop(vm);
                arg_count--;

                PyroValue* values;
                size_t value_count;

                if (PYRO_IS_TUP(last_arg)) {
                    values = PYRO_AS_TUP(last_arg)->values;
                    value_count = PYRO_AS_TUP(last_arg)->count;
                } else if (PYRO_IS_VEC(last_arg)) {
                    values = PYRO_AS_VEC(last_arg)->values;
                    value_count = PYRO_AS_VEC(last_arg)->count;
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

                PyroValue method = pyro_get_pub_method(vm, receiver, method_name);
                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), total_args);
                    break;
                }
                if (PYRO_IS_CLOSURE(method)) {
                    call_closure(vm, PYRO_AS_CLOSURE(method), total_args);
                    break;
                }

                if (PYRO_IS_MOD(receiver)) {
                    pyro_panic(vm,
                        "invalid method name ':%s'; receiver is a module, did you mean to use '::'?",
                        method_name->bytes
                    );
                    break;
                }

                pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                break;
            }

            // The [receiver], [arg_count] args, and the [superclass] are sitting on top of the stack.
            case PYRO_OPCODE_CALL_SUPER_METHOD: {
                PyroClass* superclass = PYRO_AS_CLASS(pyro_pop(vm));
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();

                PyroValue method;
                if (!PyroMap_get(superclass->all_instance_methods, pyro_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), arg_count);
                } else {
                    call_closure(vm, PYRO_AS_CLOSURE(method), arg_count);
                }

                break;
            }

            case PYRO_OPCODE_CALL_SUPER_METHOD_WITH_UNPACK: {
                PyroClass* superclass = PYRO_AS_CLASS(pyro_pop(vm));
                PyroStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                PyroValue last_arg = pyro_pop(vm);
                arg_count--;

                PyroValue* values;
                size_t value_count;

                if (PYRO_IS_TUP(last_arg)) {
                    values = PYRO_AS_TUP(last_arg)->values;
                    value_count = PYRO_AS_TUP(last_arg)->count;
                } else if (PYRO_IS_VEC(last_arg)) {
                    values = PYRO_AS_VEC(last_arg)->values;
                    value_count = PYRO_AS_VEC(last_arg)->count;
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

                PyroValue method;
                if (!PyroMap_get(superclass->all_instance_methods, pyro_obj(method_name), &method, vm)) {
                    pyro_panic(vm, "invalid method name '%s'", method_name->bytes);
                    break;
                }

                if (PYRO_IS_NATIVE_FN(method)) {
                    call_native_fn(vm, PYRO_AS_NATIVE_FN(method), total_args);
                } else {
                    call_closure(vm, PYRO_AS_CLOSURE(method), total_args);
                }

                break;
            }

            case PYRO_OPCODE_SET_UPVALUE: {
                uint8_t index = READ_BYTE();
                *frame->closure->upvalues[index]->location = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_TRY: {
                size_t stashed_stack_size = vm->stack_top - vm->stack;
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
                    vm->stack_top = vm->stack + stashed_stack_size;
                    close_upvalues(vm, vm->stack_top);
                    vm->frame_count = stashed_frame_count;

                    PyroStr* error_message = PyroBuf_to_str(vm->panic_buffer, vm);
                    if (!error_message) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    PyroErr* err = PyroErr_new(vm);
                    if (!err) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    err->message = error_message;

                    if (!PyroMap_set(err->details, pyro_obj(vm->str_source), pyro_obj(vm->panic_source_id), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    if (!PyroMap_set(err->details, pyro_obj(vm->str_line), pyro_i64(vm->panic_line_number), vm)) {
                        pyro_panic(vm, "out of memory");
                        break;
                    }

                    vm->stack_top[-1] = pyro_obj(err);
                }

                assert(vm->stack_top == vm->stack + stashed_stack_size);
                assert(vm->frame_count == stashed_frame_count);
                break;
            }

            case PYRO_OPCODE_UNPACK: {
                PyroValue value = pyro_pop(vm);
                uint8_t count = READ_BYTE();

                if (PYRO_IS_TUP(value)) {
                    PyroTup* tup = PYRO_AS_TUP(value);
                    if (tup->count < count) {
                        pyro_panic(vm,
                            "tuple count is %zu, unpacking requires at least %zu",
                            tup->count, count
                        );
                        break;
                    }
                    for (size_t i = 0; i < count; i++) {
                        if (!pyro_push(vm, tup->values[i])) break;
                    }
                    break;
                }

                if (PYRO_IS_VEC(value)) {
                    PyroVec* vec = PYRO_AS_VEC(value);
                    if (vec->count < count) {
                        pyro_panic(vm,
                            "vector count is %zu, unpacking requires at least %zu",
                            vec->count, count
                        );
                        break;
                    }
                    for (size_t i = 0; i < count; i++) {
                        if (!pyro_push(vm, vec->values[i])) break;
                    }
                    break;
                }

                pyro_panic(vm, "value cannot be unpacked");
                break;
            }

            case PYRO_OPCODE_START_WITH: {
                if (vm->with_stack_count == vm->with_stack_capacity) {
                    size_t new_capacity = PYRO_GROW_CAPACITY(vm->with_stack_capacity);
                    PyroValue* new_array = PYRO_REALLOCATE_ARRAY(vm, PyroValue, vm->with_stack, vm->with_stack_capacity, new_capacity);
                    if (!new_array) {
                        pyro_panic(vm, "out of memory: failed to allocate memory for the 'with' stack");
                        break;
                    }
                    vm->with_stack_capacity = new_capacity;
                    vm->with_stack = new_array;
                }
                vm->with_stack[vm->with_stack_count++] = vm->stack_top[-1];
                break;
            }

            case PYRO_OPCODE_END_WITH: {
                PyroValue receiver = vm->with_stack[vm->with_stack_count - 1];
                call_end_with_method(vm, receiver);
                vm->with_stack_count--;
                break;
            }

            default:
                pyro_panic(vm, "invalid opcode");
                break;
        }

        #ifdef PYRO_DEBUG
            if (vm->trace_execution) {
                pyro_stdout_write(vm, "             ");
                if (vm->stack == vm->stack_top) {
                    pyro_stdout_write(vm, "[ empty ]");
                }
                for (PyroValue* slot = vm->stack; slot < vm->stack_top; slot++) {
                    pyro_stdout_write(vm, "[ ");
                    pyro_dump_value(vm, *slot);
                    pyro_stdout_write(vm, " ]");
                }
                pyro_stdout_write(vm, "\n");
            }
        #endif
    }

    while (vm->with_stack_count > with_stack_count_on_entry) {
        PyroValue receiver = vm->with_stack[vm->with_stack_count - 1];
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
    vm->with_stack_count = 0;
}


void pyro_try_compile_file(PyroVM* vm, const char* path, bool dump_bytecode) {
    PyroBuf* buf = pyro_read_file_into_buf(vm, path, "pyro_try_compile_file()");
    if (vm->halt_flag) {
        return;
    }
    pyro_compile(vm, (char*)buf->bytes, buf->count, path, dump_bytecode);
}


void pyro_exec_code(PyroVM* vm, const char* code, size_t code_length, const char* source_id, PyroMod* module) {
    if (!module) {
        module = vm->main_module;
    }

    PyroFn* fn = pyro_compile(vm, code, code_length, source_id, false);
    if (vm->halt_flag) {
        return;
    }

    PyroClosure* closure = PyroClosure_new(vm, fn, module);
    if (!closure) {
        pyro_panic(vm, "out of memory");
        return;
    }

    #ifdef PYRO_DEBUG
        size_t stack_size_before = vm->stack_top - vm->stack;
    #endif

    if (!pyro_push(vm, pyro_obj(closure))) return;
    call_value(vm, 0);
    run(vm);
    pyro_pop(vm);

    #ifdef PYRO_DEBUG
        if (!vm->halt_flag) {
            size_t stack_size_after = vm->stack_top - vm->stack;
            assert(stack_size_before == stack_size_after);
        }
    #endif
}


void pyro_exec_file(PyroVM* vm, const char* path, PyroMod* module) {
    if (!module) {
        module = vm->main_module;
    }

    PyroStr* path_string = PyroStr_COPY(path);
    if (!path_string) {
        pyro_panic(vm, "out of memory");
        return;
    }

    if (!pyro_define_pri_member(vm, module, "$filepath", pyro_obj(path_string))) {
        pyro_panic(vm, "out of memory");
        return;
    }

    PyroBuf* buf = pyro_read_file_into_buf(vm, path, "pyro_exec_file()");
    if (vm->halt_flag) {
        return;
    }

    if (!pyro_push(vm, pyro_obj(buf))) return;
    pyro_exec_code(vm, (char*)buf->bytes, buf->count, path, module);
    pyro_pop(vm);
}


void pyro_exec_path(PyroVM* vm, const char* path, PyroMod* module) {
    if (!module) {
        module = vm->main_module;
    }

    if (pyro_is_file(path)) {
        pyro_exec_file(vm, path, module);
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
            pyro_exec_file(vm, filepath, module);
            free(filepath);
            return;
        }

        pyro_panic(vm, "directory has no 'self.pyro' file: '%s'", path);
        free(filepath);
        return;
    }

    pyro_panic(vm, "invalid path '%s'", path);
}


void pyro_run_main_func(PyroVM* vm) {
    PyroStr* main_string = PyroStr_COPY("$main");
    if (!main_string) {
        pyro_panic(vm, "out of memory");
        return;
    }

    PyroValue main_index;
    if (!PyroMap_get(vm->main_module->all_member_indexes, pyro_obj(main_string), &main_index, vm)) {
        return;
    }

    PyroValue main_value = vm->main_module->members->values[main_index.as.i64];
    if (!PYRO_IS_CLOSURE(main_value)) {
        pyro_panic(vm,
            "invalid type for $main, '%s', expected a function",
            pyro_get_type_name(vm, main_value)->bytes
        );
        return;
    }

    if (PYRO_AS_CLOSURE(main_value)->fn->arity != 0) {
        pyro_panic(vm, "invalid $main() function, must take 0 arguments");
        return;
    }

    if (!pyro_push(vm, main_value)) return;
    call_value(vm, 0);
    run(vm);
    pyro_pop(vm);
}


PyroValue pyro_call_method(PyroVM* vm, PyroValue method, uint8_t arg_count) {
    if (PYRO_IS_NATIVE_FN(method)) {
        call_native_fn(vm, PYRO_AS_NATIVE_FN(method), arg_count);
        return pyro_pop(vm);
    }

    if (PYRO_IS_CLOSURE(method)) {
        call_closure(vm, PYRO_AS_CLOSURE(method), arg_count);
        run(vm);
        return pyro_pop(vm);
    }

    pyro_panic(vm, "value is not callable as a method");
    return pyro_null();
}


PyroValue pyro_call_function(PyroVM* vm, uint8_t arg_count) {
    PyroValue value = vm->stack_top[-(int)arg_count - 1];

    if (PYRO_IS_NATIVE_FN(value)) {
        call_native_fn(vm, PYRO_AS_NATIVE_FN(value), arg_count);
        return pyro_pop(vm);
    }

    call_value(vm, arg_count);
    run(vm);
    return pyro_pop(vm);
}
