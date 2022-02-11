#include "exec.h"
#include "vm.h"
#include "opcodes.h"
#include "debug.h"
#include "compiler.h"
#include "heap.h"
#include "values.h"
#include "utils.h"
#include "operators.h"
#include "setup.h"
#include "stringify.h"
#include "panics.h"
#include "imports.h"
#include "os.h"
#include "io.h"
#include "gc.h"


static void call_closure(PyroVM* vm, ObjClosure* closure, uint8_t arg_count) {
    if (arg_count != closure->fn->arity) {
        pyro_panic(
            vm, ERR_ARGS_ERROR,
            "Expected %d argument%s for %s(), found %d.",
            closure->fn->arity,
            closure->fn->arity == 1 ? "" : "s",
            closure->fn->name->bytes,
            arg_count
        );
        return;
    }

    if (vm->frame_count == PYRO_MAX_CALL_FRAMES) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Max call depth exceeded.");
        return;
    }

    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->fn->code;
    frame->fp = vm->stack_top - arg_count - 1;
}


static void call_native_fn(PyroVM* vm, ObjNativeFn* fn, uint8_t arg_count) {
    if (fn->arity == arg_count || fn->arity == -1) {
        Value result = fn->fn_ptr(vm, arg_count, vm->stack_top - arg_count);
        vm->stack_top -= (arg_count + 1);
        pyro_push(vm, result);
        return;
    }
    pyro_panic(
        vm, ERR_ARGS_ERROR,
        "Expected %d argument%s for %s(), found %d.",
        fn->arity,
        fn->arity == 1 ? "" : "s",
        fn->name->bytes,
        arg_count
    );
}


static void call_value(PyroVM* vm, Value callee, uint8_t arg_count) {
    if (IS_OBJ(callee)) {
        switch(AS_OBJ(callee)->type) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stack_top[-arg_count - 1] = bound->receiver;

                if (bound->method->type == OBJ_NATIVE_FN) {
                    call_native_fn(vm, (ObjNativeFn*)bound->method, arg_count);
                } else {
                    call_closure(vm, (ObjClosure*)bound->method, arg_count);
                }

                return;
            }

            case OBJ_CLASS: {
                ObjClass* class = AS_CLASS(callee);
                ObjInstance* instance = ObjInstance_new(vm, class);
                if (!instance) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    return;
                }
                vm->stack_top[-arg_count - 1] = MAKE_OBJ(instance);

                Value initializer;
                if (ObjMap_get(class->methods, MAKE_OBJ(vm->str_init), &initializer, vm)) {
                    call_value(vm, initializer, arg_count);
                } else if (arg_count != 0) {
                    pyro_panic(
                        vm, ERR_ARGS_ERROR,
                        "Expected 0 arguments for initializer, found %d.", arg_count
                    );
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
                if (ObjMap_get(class->methods, MAKE_OBJ(vm->str_call), &call_method, vm)) {
                    call_value(vm, call_method, arg_count);
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Object is not callable.");
                }

                return;
            }

            default:
                break;
        }
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Object is not callable.");
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
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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


// The method's ObjClosure will be sitting on top of the stack, the ObjClass just below it.
static void define_method(PyroVM* vm, ObjStr* name) {
    Value method_value = pyro_peek(vm, 0);
    ObjClass* class = AS_CLASS(pyro_peek(vm, 1));

    if (ObjMap_set(class->methods, MAKE_OBJ(name), method_value, vm) == 0) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }

    pyro_pop(vm);
}


// The field's initial value will be sitting on top of the stack, the class object just below it.
static void define_field(PyroVM* vm, ObjStr* name) {
    Value init_value = pyro_peek(vm, 0);
    ObjClass* class = AS_CLASS(pyro_peek(vm, 1));
    size_t field_index = class->field_initializers->count;

    if (!ObjVec_append(class->field_initializers, init_value, vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    if (ObjMap_set(class->field_indexes, MAKE_OBJ(name), MAKE_I64(field_index), vm) == 0) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_pop(vm);
}


// Pops the receiver and replaces it with the bound method object.
static void bind_method(PyroVM* vm, ObjClass* class, ObjStr* method_name) {
    Value method;
    if (!ObjMap_get(class->methods, MAKE_OBJ(method_name), &method, vm)) {
        pyro_panic(vm, ERR_NAME_ERROR, "Invalid method name '%s'.", method_name->bytes);
        return;
    }

    ObjBoundMethod* bound = ObjBoundMethod_new(vm, pyro_peek(vm, 0), AS_OBJ(method));
    if (!bound) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_pop(vm);
    pyro_push(vm, MAKE_OBJ(bound));
}


static void invoke_method_from_class(PyroVM* vm, ObjClass* class, ObjStr* method_name, uint8_t arg_count) {
    Value method;
    if (!ObjMap_get(class->methods, MAKE_OBJ(method_name), &method, vm)) {
        pyro_panic(vm, ERR_NAME_ERROR, "Invalid method name '%s'.", method_name->bytes);
        return;
    }

    if (IS_NATIVE_FN(method)) {
        call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
    } else {
        call_closure(vm, AS_CLOSURE(method), arg_count);
    }
}


static void invoke_method(PyroVM* vm, ObjStr* method_name, uint8_t arg_count) {
    Value receiver = pyro_peek(vm, arg_count);
    ObjClass* class = pyro_get_class(receiver);

    if (class) {
        invoke_method_from_class(vm, class, method_name, arg_count);
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid method call '%s'.", method_name->bytes);
    }
}


static void run(PyroVM* vm) {
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
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
        if (vm->halt_flag) {
            return;
        }

        #ifdef PYRO_DEBUG_STRESS_GC
            pyro_collect_garbage(vm);
            if (vm->halt_flag) {
                return;
            }
        #else
            if (vm->bytes_allocated > vm->next_gc_threshold) {
                pyro_collect_garbage(vm);
                if (vm->halt_flag) {
                    return;
                }
            }
        #endif

        #ifdef PYRO_DEBUG_TRACE_EXECUTION
            pyro_write_stdout(vm, "             ");
            if (vm->stack == vm->stack_top) {
                pyro_write_stdout(vm, "[ empty ]");
            }
            for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
                pyro_write_stdout(vm, "[ ");
                pyro_dump_value(vm, *slot);
                pyro_write_stdout(vm, " ]");
            }
            pyro_write_stdout(vm, "\n");
            size_t ip = frame->ip - frame->closure->fn->code;
            pyro_disassemble_instruction(vm, frame->closure->fn, ip);
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {

            case OP_BINARY_PLUS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_plus(vm, a, b));
                break;
            }

            case OP_ASSERT: {
                Value test_expr = pyro_pop(vm);
                if (!pyro_is_truthy(test_expr)) {
                    pyro_panic(vm, ERR_ASSERTION_FAILED, "Assertion failed.");
                }
                break;
            }

            case OP_BINARY_AMP: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, MAKE_I64(a.as.i64 & b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '&' must both be integers.");
                }
                break;
            }

            case OP_BINARY_BAR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, MAKE_I64(a.as.i64 | b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '|' must both be integers.");
                }
                break;
            }

            case OP_UNARY_TILDE: {
                Value operand = pyro_pop(vm);
                if (IS_I64(operand)) {
                    pyro_push(vm, MAKE_I64(~operand.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Bitwise '~' requires an integer operand.");
                }
                break;
            }

            case OP_BINARY_CARET: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, MAKE_I64(a.as.i64 ^ b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '^' must both be integers.");
                }
                break;
            }

            case OP_CALL: {
                uint8_t arg_count = READ_BYTE();
                Value callee = pyro_peek(vm, arg_count);
                call_value(vm, callee, arg_count);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_MAKE_CLASS: {
                ObjClass* class = ObjClass_new(vm);
                if (class) {
                    class->name = READ_STRING();
                    pyro_push(vm, MAKE_OBJ(class));
                } else {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                }
                break;
            }

            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack_top - 1);
                pyro_pop(vm);
                break;
            }

            case OP_MAKE_CLOSURE: {
                ObjFn* fn = AS_FN(READ_CONSTANT());
                ObjModule* module = frame->closure->module;
                ObjClosure* closure = ObjClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                pyro_push(vm, MAKE_OBJ(closure));

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

            case OP_DEFINE_GLOBAL: {
                Value name = READ_CONSTANT();
                ObjMap* globals = frame->closure->module->globals;
                if (ObjMap_set(globals, name, pyro_peek(vm, 0), vm) == 0) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                }
                pyro_pop(vm);
                break;
            }

            case OP_DEFINE_GLOBALS: {
                uint8_t count = READ_BYTE();
                ObjMap* globals = frame->closure->module->globals;

                for (uint8_t i = 0; i < count; i++) {
                    Value name = READ_CONSTANT();
                    if (ObjMap_set(globals, name, pyro_peek(vm, count - 1 - i), vm) == 0) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
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
                        return;
                    }
                    int64_t n = pyro_write_stdout(vm, i > 1 ? "%s " : "%s\n", string->bytes);
                    if (n == -1) {
                        pyro_panic(vm, ERR_OS_ERROR, "Failed to write to the standard output stream.");
                        break;
                    } else if (n == -2) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        break;
                    }
                }

                vm->stack_top -= arg_count;
                break;
            }

            case OP_BINARY_EQUAL_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, MAKE_BOOL(pyro_op_compare_eq(vm, a, b)));
                break;
            }

            case OP_BINARY_SLASH: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, pyro_op_binary_slash(vm, a, b));
                break;
            }

            case OP_DEFINE_FIELD: {
                define_field(vm, READ_STRING());
                break;
            }

            case OP_GET_FIELD: {
                Value field_name = READ_CONSTANT();

                if (!IS_INSTANCE(pyro_peek(vm, 0))) {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid field access '%s'.", AS_STR(field_name)->bytes);
                    break;
                }

                ObjInstance* instance = AS_INSTANCE(pyro_peek(vm, 0));

                Value field_index;
                if (ObjMap_get(instance->obj.class->field_indexes, field_name, &field_index, vm)) {
                    pyro_pop(vm); // pop the instance
                    pyro_push(vm, instance->fields[field_index.as.i64]);
                    break;
                }

                pyro_panic(vm, ERR_NAME_ERROR, "Invalid field name '%s'.", AS_STR(field_name)->bytes);
                break;
            }

            case OP_GET_GLOBAL: {
                Value name = READ_CONSTANT();
                ObjMap* globals = frame->closure->module->globals;

                Value value;
                if (!ObjMap_get(globals, name, &value, vm)) {
                    if (!ObjMap_get(vm->globals, name, &value, vm)) {
                        pyro_panic(vm, ERR_NAME_ERROR, "Undefined variable '%s'.", AS_STR(name)->bytes);
                        break;
                    }
                }

                pyro_push(vm, value);
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

            case OP_GET_MEMBER: {
                Value member_name = READ_CONSTANT();

                if (!IS_MOD(pyro_peek(vm, 0))) {
                    pyro_panic(vm, ERR_TYPE_ERROR,
                        "Invalid member access '%s', receiver is not a module.",
                        AS_STR(member_name)->bytes
                    );
                    break;
                }

                ObjModule* module = AS_MOD(pyro_pop(vm));

                Value value;
                if (ObjMap_get(module->globals, member_name, &value, vm)) {
                    pyro_push(vm, value);
                    break;
                }

                pyro_panic(vm, ERR_NAME_ERROR, "Invalid member name '%s'.", AS_STR(member_name)->bytes);
                break;
            }

            case OP_GET_METHOD: {
                Value receiver = pyro_peek(vm, 0);
                ObjStr* method_name = READ_STRING();
                ObjClass* class = pyro_get_class(receiver);

                if (class) {
                    bind_method(vm, class, method_name);
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid method access '%s'.", method_name->bytes);
                }

                break;
            }

            case OP_GET_SUPER_METHOD: {
                ObjStr* method_name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pyro_pop(vm));
                bind_method(vm, superclass, method_name);
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
                pyro_push(vm, MAKE_BOOL(pyro_op_compare_gt(vm, a, b)));
                break;
            }

            case OP_BINARY_GREATER_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, MAKE_BOOL(pyro_op_compare_ge(vm, a, b)));
                break;
            }

            case OP_IMPORT_MODULE: {
                // The import path is stored on the stack as an array of [arg_count] strings.
                uint8_t arg_count = READ_BYTE();
                Value* args = vm->stack_top - arg_count;

                ObjMap* supermod_modules_map = vm->modules;
                Value module_value;

                // This loop imports ancestor modules along the path if they haven't already been
                // imported, i.e. if the path is foo::bar::baz this loop will first import foo then
                // foo::bar then foo::bar::baz.
                for (uint8_t i = 0; i < arg_count; i++) {
                    Value name = args[i];

                    if (ObjMap_get(supermod_modules_map, name, &module_value, vm)) {
                        supermod_modules_map = AS_MOD(module_value)->submodules;
                        continue;
                    }

                    ObjModule* module_object = ObjModule_new(vm);
                    if (!module_object) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    module_value = MAKE_OBJ(module_object);

                    pyro_push(vm, module_value);
                    if (ObjMap_set(supermod_modules_map, name, module_value, vm) == 0) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    pyro_pop(vm); // module_value

                    pyro_import_module(vm, i + 1, args, module_object);
                    if (vm->halt_flag) {
                        ObjMap_remove(supermod_modules_map, name, vm);
                        return;
                    }

                    supermod_modules_map = module_object->submodules;
                }

                vm->stack_top -= arg_count;
                pyro_push(vm, module_value);
                break;
            }

            case OP_IMPORT_MEMBERS: {
                uint8_t module_count = READ_BYTE();
                uint8_t member_count = READ_BYTE();
                Value* args = vm->stack_top - module_count - member_count;

                ObjMap* supermod_modules_map = vm->modules;
                Value module_value;

                for (uint8_t i = 0; i < module_count; i++) {
                    Value name = args[i];

                    if (ObjMap_get(supermod_modules_map, name, &module_value, vm)) {
                        supermod_modules_map = AS_MOD(module_value)->submodules;
                        continue;
                    }

                    ObjModule* module_object = ObjModule_new(vm);
                    if (!module_object) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    module_value = MAKE_OBJ(module_object);

                    pyro_push(vm, module_value);
                    if (ObjMap_set(supermod_modules_map, name, module_value, vm) == 0) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    pyro_pop(vm); // module_value

                    pyro_import_module(vm, i + 1, args, module_object);
                    if (vm->halt_flag) {
                        ObjMap_remove(supermod_modules_map, name, vm);
                        return;
                    }

                    supermod_modules_map = module_object->submodules;
                }

                ObjMap* module_globals = AS_MOD(module_value)->globals;

                for (uint8_t i = 0; i < member_count; i++) {
                    Value name = args[module_count + i];
                    Value member;
                    if (!ObjMap_get(module_globals, name, &member, vm)) {
                        pyro_panic(
                            vm, ERR_NAME_ERROR,
                            "Member '%s' not found in module.", AS_STR(name)->bytes
                        );
                        return;
                    }
                    args[i] = member;
                }

                vm->stack_top -= module_count;
                break;
            }

            case OP_INHERIT: {
                if (!IS_CLASS(pyro_peek(vm, 1))) {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Invalid superclass value (not a class).");
                    break;
                }

                ObjClass* superclass = AS_CLASS(pyro_peek(vm, 1));
                ObjClass* subclass = AS_CLASS(pyro_peek(vm, 0));

                if (superclass == subclass) {
                    pyro_panic(vm, ERR_TYPE_ERROR, "A class cannot inherit from itself.");
                    break;
                }

                // "Copy-down inheritance". We copy all the superclass's methods, field indexes,
                // and field initializers to the subclass. This means that there's no extra
                // runtime work involved in looking up inherited methods or fields.
                if (!ObjMap_copy_entries(superclass->methods, subclass->methods, vm)) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }
                if (!ObjMap_copy_entries(superclass->field_indexes, subclass->field_indexes, vm)) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }
                if (!ObjVec_copy_entries(superclass->field_initializers, subclass->field_initializers, vm)) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                subclass->superclass = superclass;
                pyro_pop(vm); // the subclass
                break;
            }

            case OP_INVOKE_METHOD: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                invoke_method(vm, method_name, arg_count);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_INVOKE_SUPER_METHOD: {
                ObjStr* method_name = READ_STRING();
                uint8_t arg_count = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pyro_pop(vm));
                invoke_method_from_class(vm, superclass, method_name, arg_count);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_GET_ITERATOR_OBJECT: {
                if (pyro_has_method(vm, pyro_peek(vm, 0), vm->str_iter)) {
                    invoke_method(vm, vm->str_iter, 0);
                    frame = &vm->frames[vm->frame_count - 1];
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Object is not iterable.");
                }
                break;
            }

            case OP_GET_ITERATOR_NEXT_VALUE: {
                if (IS_ITER(pyro_peek(vm, 0))) {
                    Value next_value = ObjIter_next(AS_ITER(pyro_peek(vm, 0)), vm);
                    pyro_push(vm, next_value);
                } else {
                    pyro_push(vm, pyro_peek(vm, 0));
                    invoke_method(vm, vm->str_next, 0);
                    frame = &vm->frames[vm->frame_count - 1];
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
                pyro_push(vm, MAKE_BOOL(pyro_op_compare_lt(vm, a, b)));
                break;
            }

            case OP_BINARY_LESS_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, MAKE_BOOL(pyro_op_compare_le(vm, a, b)));
                break;
            }

            case OP_LOAD_CONSTANT: {
                Value constant = READ_CONSTANT();
                pyro_push(vm, constant);
                break;
            }

            case OP_LOAD_FALSE:
                pyro_push(vm, MAKE_BOOL(false));
                break;

            case OP_LOAD_I64_0:
                pyro_push(vm, MAKE_I64(0));
                break;

            case OP_LOAD_I64_1:
                pyro_push(vm, MAKE_I64(1));
                break;

            case OP_LOAD_I64_2:
                pyro_push(vm, MAKE_I64(2));
                break;

            case OP_LOAD_I64_3:
                pyro_push(vm, MAKE_I64(3));
                break;

            case OP_LOAD_I64_4:
                pyro_push(vm, MAKE_I64(4));
                break;

            case OP_LOAD_I64_5:
                pyro_push(vm, MAKE_I64(5));
                break;

            case OP_LOAD_I64_6:
                pyro_push(vm, MAKE_I64(6));
                break;

            case OP_LOAD_I64_7:
                pyro_push(vm, MAKE_I64(7));
                break;

            case OP_LOAD_I64_8:
                pyro_push(vm, MAKE_I64(8));
                break;

            case OP_LOAD_I64_9:
                pyro_push(vm, MAKE_I64(9));
                break;

            case OP_LOAD_NULL:
                pyro_push(vm, MAKE_NULL());
                break;

            case OP_LOAD_TRUE:
                pyro_push(vm, MAKE_BOOL(true));
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
                        pyro_push(vm, MAKE_I64(a.as.i64 << b.as.i64));
                    } else {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Right operand to '<<' cannot be negative.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<<' must both be integers.");
                }
                break;
            }

            case OP_MAKE_MAP: {
                uint16_t entry_count = READ_BE_U16();

                ObjMap* map = ObjMap_new(vm);
                if (!map) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                // Push the map on the stack so it doesn't get gc'd while we're adding entries.
                pyro_push(vm, MAKE_OBJ(map));
                if (entry_count == 0) {
                    break;
                }

                // The entries are stored on the stack as [..][key][value][..] pairs.
                for (Value* slot = vm->stack_top - entry_count * 2 - 1; slot < vm->stack_top - 1; slot += 2) {
                    if (ObjMap_set(map, slot[0], slot[1], vm) == 0) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        break;
                    }
                }

                vm->stack_top -= (entry_count * 2 + 1);
                pyro_push(vm, MAKE_OBJ(map));
                break;
            }

            case OP_MAKE_VEC: {
                uint16_t item_count = READ_BE_U16();

                ObjVec* vec = ObjVec_new_with_cap(item_count, vm);
                if (!vec) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                if (item_count == 0) {
                    pyro_push(vm, MAKE_OBJ(vec));
                    break;
                }

                memcpy(vec->values, vm->stack_top - item_count, sizeof(Value) * item_count);
                vec->count = item_count;

                vm->stack_top -= item_count;
                pyro_push(vm, MAKE_OBJ(vec));
                break;
            }

            case OP_DEFINE_METHOD: {
                define_method(vm, READ_STRING());
                break;
            }

            case OP_BINARY_PERCENT: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_I64(a.as.i64 % b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(fmod(a.as.f64, b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(fmod((double)a.as.i64, b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(fmod(a.as.f64, (double)b.as.i64)));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '%%' must both be numbers.");
                }

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
                pyro_push(vm, MAKE_BOOL(!pyro_is_truthy(operand)));
                break;
            }

            case OP_BINARY_BANG_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, MAKE_BOOL(!pyro_op_compare_eq(vm, a, b)));
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
                        return;
                    }
                    pyro_write_stdout(vm, "%s\n", string->bytes);
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

                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, MAKE_F64(pow((double)a.as.i64, (double)b.as.i64)));
                } else if (IS_F64(a) && IS_F64(b)) {
                    pyro_push(vm, MAKE_F64(pow(a.as.f64, b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    pyro_push(vm, MAKE_F64(pow((double)a.as.i64, b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    pyro_push(vm, MAKE_F64(pow(a.as.f64, (double)b.as.i64)));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '^' must both be numbers.");
                }

                break;
            }

            case OP_RETURN: {
                Value result = pyro_pop(vm);

                close_upvalues(vm, frame->fp);
                vm->stack_top = frame->fp;
                pyro_push(vm, result);

                vm->frame_count--;
                if (vm->frame_count < frame_count_on_entry) {
                    return;
                }

                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_BINARY_GREATER_GREATER: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 >= 0) {
                        pyro_push(vm, MAKE_I64(a.as.i64 >> b.as.i64));
                    } else {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Right operand to '>>' cannot be negative.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>>' must both be integers.");
                }
                break;
            }

            case OP_SET_FIELD: {
                if (!IS_INSTANCE(pyro_peek(vm, 1))) {
                    pyro_panic(
                        vm, ERR_TYPE_ERROR,
                        "Invalid field access '.', receiver does not have fields."
                    );
                    break;
                }

                ObjInstance* instance = AS_INSTANCE(pyro_peek(vm, 1));
                Value field_name = READ_CONSTANT();

                Value field_index;
                if (ObjMap_get(instance->obj.class->field_indexes, field_name, &field_index, vm)) {
                    Value new_value = pyro_pop(vm);
                    pyro_pop(vm); // pop the instance
                    instance->fields[field_index.as.i64] = new_value;
                    pyro_push(vm, new_value);
                    break;
                }

                pyro_panic(vm, ERR_NAME_ERROR, "Invalid field name '%s'.", AS_STR(field_name)->bytes);
                break;
            }

            case OP_SET_GLOBAL: {
                Value name = READ_CONSTANT();
                ObjMap* globals = frame->closure->module->globals;
                if (!ObjMap_update_entry(globals, name, pyro_peek(vm, 0), vm)) {
                    if (!ObjMap_update_entry(vm->globals, name, pyro_peek(vm, 0), vm)) {
                        pyro_panic(vm, ERR_NAME_ERROR, "Undefined variable '%s'.", AS_STR(name)->bytes);
                    }
                }
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

                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_I64(a.as.i64 / b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(trunc(a.as.f64 / b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(trunc((double)a.as.i64 / b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, MAKE_F64(trunc(a.as.f64 / (double)b.as.i64)));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '//' must both be numbers.");
                }

                break;
            }

            case OP_TRY: {
                Value* stashed_stack_top = vm->stack_top;
                size_t stashed_frame_count = vm->frame_count;

                vm->try_depth++;
                Value callee = pyro_peek(vm, 0);
                call_value(vm, callee, 0);
                run(vm);
                vm->try_depth--;

                if (vm->exit_flag || vm->hard_panic) {
                    return;
                }

                if (vm->panic_flag) {
                    int64_t error_code = vm->status_code;

                    // Reset the VM.
                    vm->panic_flag = false;
                    vm->halt_flag = false;
                    vm->status_code = 0;
                    vm->memory_allocation_failed = false;
                    vm->stack_top = stashed_stack_top - 1;
                    close_upvalues(vm, stashed_stack_top);
                    vm->frame_count = stashed_frame_count;
                    frame = &vm->frames[vm->frame_count - 1];

                    ObjStr* err_str = ObjBuf_to_str(vm->panic_buffer, vm);
                    if (!err_str) {
                        vm->hard_panic = true;
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }

                    pyro_push(vm, MAKE_OBJ(err_str));
                    ObjTup* err_tup = ObjTup_new_err(2, vm);
                    if (!err_tup) {
                        vm->hard_panic = true;
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    pyro_pop(vm);

                    err_tup->values[0] = MAKE_I64(error_code);
                    err_tup->values[1] = MAKE_OBJ(err_str);
                    pyro_push(vm, MAKE_OBJ(err_tup));

                    if (!ObjBuf_grow(vm->panic_buffer, 256, vm)) {
                        vm->hard_panic = true;
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
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
                            vm, ERR_VALUE_ERROR,
                            "Tuple has %d value(s), requires %d for unpacking.", tup->count, count
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
                            vm, ERR_VALUE_ERROR,
                            "Vector has %d value(s), requires %d for unpacking.", vec->count, count
                        );
                    } else {
                        for (size_t i = 0; i < count; i++) {
                            pyro_push(vm, vec->values[i]);
                        }
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Value is not unpackable.");
                }

                break;
            }

            default:
                assert(false);
                vm->hard_panic = true;
                pyro_panic(vm, ERR_ERROR, "Invalid opcode.");
                break;
        }
    }

    #undef READ_BE_U16
    #undef READ_STRING
    #undef READ_CONSTANT
    #undef READ_BYTE
}


static void reset_stack(PyroVM* vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}


void pyro_exec_code_as_main(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    vm->halt_flag = false;
    vm->exit_flag = false;
    vm->panic_flag = false;
    vm->hard_panic = false;
    vm->status_code = 0;

    ObjFn* fn = pyro_compile(vm, src_code, src_len, src_id);
    if (vm->halt_flag) {
        return;
    }

    pyro_push(vm, MAKE_OBJ(fn));
    ObjClosure* closure = ObjClosure_new(vm, fn, vm->main_module);
    pyro_pop(vm);
    if (!closure) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_push(vm, MAKE_OBJ(closure));
    call_value(vm, MAKE_OBJ(closure), 0);
    run(vm);
    pyro_pop(vm);

    if (vm->halt_flag) {
        reset_stack(vm);
    }
    assert(vm->stack_top == vm->stack);
}


void pyro_exec_file_as_main(PyroVM* vm, const char* filepath) {
    char* realpath = pyro_realpath(filepath);
    if (!realpath) {
        pyro_panic(vm, ERR_OS_ERROR, "Unable to resolve path '%s'.", filepath);
        return;
    }

    ObjStr* realpath_string = STR(realpath);
    if (!realpath_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(realpath);
        return;
    }
    pyro_define_member(vm, vm->main_module, "$filepath", MAKE_OBJ(realpath_string));
    free(realpath);

    FileData fd;
    if (!pyro_read_file(vm, filepath, &fd) || fd.size == 0) {
        return;
    }

    pyro_exec_code_as_main(vm, fd.data, fd.size, filepath);
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
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return;
        }
        memcpy(filepath, path, path_length);
        memcpy(&filepath[path_length], "/self.pyro", strlen("/self.pyro"));
        filepath[path_length + strlen("/self.pyro")] = '\0';

        if (pyro_is_file(filepath)) {
            pyro_exec_file_as_main(vm, filepath);
        } else {
            pyro_panic(vm, ERR_ERROR, "Module requires 'self.pyro' file to be executable.");
        }

        free(filepath);
        return;
    }

    pyro_panic(vm, ERR_ERROR, "Invalid path '%s'.", path);
}


void pyro_try_compile_file(PyroVM* vm, const char* path) {
    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }
    pyro_compile(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
}


void pyro_exec_file_as_module(PyroVM* vm, const char* filepath, ObjModule* module) {
    char* realpath = pyro_realpath(filepath);
    if (!realpath) {
        pyro_panic(vm, ERR_OS_ERROR, "Unable to resolve path '%s'.", filepath);
        return;
    }

    ObjStr* realpath_string = STR(realpath);
    if (!realpath_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        free(realpath);
        return;
    }
    pyro_define_member(vm, module, "$filepath", MAKE_OBJ(realpath_string));
    free(realpath);

    FileData fd;
    if (!pyro_read_file(vm, filepath, &fd) || fd.size == 0) {
        return;
    }

    ObjFn* fn = pyro_compile(vm, fd.data, fd.size, filepath);
    FREE_ARRAY(vm, char, fd.data, fd.size);
    if (vm->halt_flag) {
        return;
    }

    pyro_push(vm, MAKE_OBJ(fn));
    ObjClosure* closure = ObjClosure_new(vm, fn, module);
    pyro_pop(vm);
    if (!closure) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_push(vm, MAKE_OBJ(closure));
    call_value(vm, MAKE_OBJ(closure), 0);
    run(vm);
    pyro_pop(vm);
}


void pyro_run_main_func(PyroVM* vm) {
    ObjStr* main_string = STR("$main");
    if (!main_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    Value main_value;
    if (ObjMap_get(vm->main_module->globals, MAKE_OBJ(main_string), &main_value, vm)) {
        if (IS_CLOSURE(main_value)) {
            if (AS_CLOSURE(main_value)->fn->arity == 0) {
                pyro_push(vm, main_value);
                call_value(vm, main_value, 0);
                run(vm);
                pyro_pop(vm);
            } else {
                pyro_panic(vm, ERR_ARGS_ERROR, "Invalid $main(), must take 0 arguments.");
            }
        } else {
            pyro_panic(vm, ERR_TYPE_ERROR, "Invalid $main, must be a function.");
        }
    }
}


void pyro_run_test_funcs(PyroVM* vm, int* passed, int* failed) {
    int tests_passed = 0;
    int tests_failed = 0;

    for (size_t i = 0; i < vm->main_module->globals->entry_array_count; i++) {
        MapEntry* entry = &vm->main_module->globals->entry_array[i];
        if (IS_STR(entry->key) && IS_CLOSURE(entry->value)) {
            ObjStr* name = AS_STR(entry->key);
            if (name->length > 6 && memcmp(name->bytes, "$test_", 6) == 0) {
                if (AS_CLOSURE(entry->value)->fn->arity > 0) {
                    pyro_write_stdout(vm, " · Invalid test function (%s), too many args.\n", name->bytes);
                    tests_failed += 1;
                    continue;
                }

                vm->halt_flag = false;
                vm->exit_flag = false;
                vm->panic_flag = false;
                vm->hard_panic = false;
                vm->status_code = 0;

                pyro_push(vm, entry->value);
                call_value(vm, entry->value, 0);
                run(vm);
                pyro_pop(vm);

                if (vm->halt_flag) {
                    reset_stack(vm);
                }
                assert(vm->stack_top == vm->stack);

                if (vm->exit_flag) {
                    pyro_write_stdout(vm, " · \x1B[1;31mEXIT\x1B[0m (%d) %s\n", vm->status_code, name->bytes);
                    tests_failed += 1;
                    break;
                }

                if (vm->hard_panic) {
                    pyro_write_stdout(vm, " · \x1B[1;31mHARD PANIC\x1B[0m %s\n", name->bytes);
                    tests_failed += 1;
                    break;
                }

                if (vm->panic_flag) {
                    pyro_write_stdout(vm, " · \x1B[1;31mFAIL\x1B[0m %s\n", name->bytes);
                    tests_failed += 1;
                    continue;
                }

                tests_passed += 1;
            }
        }
    }

    *passed = tests_passed;
    *failed = tests_failed;
}


void pyro_run_time_funcs(PyroVM* vm, size_t num_iterations) {
    size_t max_name_length = 0;

    for (size_t i = 0; i < vm->main_module->globals->entry_array_count; i++) {
        MapEntry* entry = &vm->main_module->globals->entry_array[i];
        if (IS_STR(entry->key) && IS_CLOSURE(entry->value)) {
            ObjStr* name = AS_STR(entry->key);
            if (name->length > 6 && memcmp(name->bytes, "$time_", 6) == 0) {
                if (AS_CLOSURE(entry->value)->fn->arity > 0) {
                    pyro_write_stdout(vm, " · Invalid time function (%s), too many args.\n", name->bytes);
                    return;
                }
                if (name->length > max_name_length) {
                    max_name_length = name->length;
                }
            }
        }
    }

    for (size_t i = 0; i < vm->main_module->globals->entry_array_count; i++) {
        MapEntry* entry = &vm->main_module->globals->entry_array[i];
        if (IS_STR(entry->key) && IS_CLOSURE(entry->value)) {
            ObjStr* name = AS_STR(entry->key);
            if (name->length > 6 && memcmp(name->bytes, "$time_", 6) == 0) {
                double start_time = clock();

                for (size_t j = 0; j < num_iterations; j++) {
                    pyro_push(vm, entry->value);
                    call_value(vm, entry->value, 0);
                    run(vm);
                    pyro_pop(vm);

                    if (vm->halt_flag) {
                        reset_stack(vm);
                        return;
                    }
                }

                double total_runtime = (double)(clock() - start_time) / CLOCKS_PER_SEC;
                double average_in_secs = total_runtime / num_iterations;
                double average_in_millisecs = average_in_secs * 1000;

                pyro_write_stdout(vm, " · %s() ···", name->bytes);
                for (size_t j = 0; j < max_name_length - name->length; j++) {
                    pyro_write_stdout(vm, "·");
                }

                pyro_write_stdout(vm, " %.6f s", average_in_secs);

                if (average_in_secs < 1.0) {
                    pyro_write_stdout(vm, " ··· ");
                    pyro_write_stdout(vm, "%.3f ms", average_in_millisecs);
                }

                pyro_write_stdout(vm, "\n");
            }
        }
    }
}


Value pyro_call_method(PyroVM* vm, Value method, uint8_t arg_count) {
    if (IS_NATIVE_FN(method)) {
        call_native_fn(vm, AS_NATIVE_FN(method), arg_count);
        return pyro_pop(vm);
    } else {
        call_closure(vm, AS_CLOSURE(method), arg_count);
        run(vm);
        return pyro_pop(vm);
    }
}


Value pyro_call_function(PyroVM* vm, uint8_t arg_count) {
    Value function = pyro_peek(vm, arg_count);
    if (IS_NATIVE_FN(function)) {
        call_native_fn(vm, AS_NATIVE_FN(function), arg_count);
        return pyro_pop(vm);
    } else {
        call_value(vm, function, arg_count);
        run(vm);
        return pyro_pop(vm);
    }
}
