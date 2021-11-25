#include "vm.h"
#include "opcodes.h"
#include "debug.h"
#include "compiler.h"
#include "heap.h"
#include "values.h"
#include "utils.h"
#include "os.h"
#include "errors.h"

#include "../std/std_core.h"
#include "../std/std_math.h"
#include "../std/std_pyro.h"
#include "../std/std_prng.h"
#include "../std/std_errors.h"


static Value pyro_import_module(PyroVM* vm, uint8_t arg_count, Value* args) {
    ObjModule* module = ObjModule_new(vm);
    if (!module) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }

    for (size_t i = 0; i < vm->import_roots->count; i++) {
        ObjStr* base = AS_STR(vm->import_roots->values[i]);

        bool has_trailing_slash = false;
        if (base->length > 0 && base->bytes[base->length - 1] == '/') {
            has_trailing_slash = true;
        }

        size_t path_length = has_trailing_slash ? base->length : base->length + 1;
        for (uint8_t j = 0; j < arg_count; j++) {
            path_length += AS_STR(args[j])->length + 1;
        }
        path_length += 4 + 5; // add space for a [.pyro] or [/self.pyro] suffix

        pyro_push(vm, OBJ_VAL(module));
        char* path = ALLOCATE_ARRAY(vm, char, path_length + 1);
        pyro_pop(vm);
        if (!path) {
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
            return NULL_VAL();
        }

        // We start with path = BASE/
        memcpy(path, base->bytes, base->length);
        size_t path_count = base->length;
        if (!has_trailing_slash) {
            path[path_count++] = '/';
        }

        // Given 'import foo::bar::baz', assemble path = BASE/foo/bar/baz/
        for (uint8_t j = 0; j < arg_count; j++) {
            ObjStr* name = AS_STR(args[j]);
            memcpy(path + path_count, name->bytes, name->length);
            path_count += name->length;
            path[path_count++] = '/';
        }

        // 1. Try file: BASE/foo/bar/baz.pyro
        memcpy(path + path_count - 1, ".pyro", 5);
        path_count += 4;
        path[path_count] = '\0';

        if (pyro_file_exists(path)) {
            pyro_push(vm, OBJ_VAL(module));
            pyro_exec_file_as_module(vm, path, module);
            pyro_pop(vm);
            FREE_ARRAY(vm, char, path, path_length + 1);
            return OBJ_VAL(module);
        }

        // 2. Try file: BASE/foo/bar/baz/self.pyro
        memcpy(path + path_count - 5, "/self.pyro", 10);
        path_count += 5;
        path[path_count] = '\0';

        if (pyro_file_exists(path)) {
            pyro_push(vm, OBJ_VAL(module));
            pyro_exec_file_as_module(vm, path, module);
            pyro_pop(vm);
            FREE_ARRAY(vm, char, path, path_length + 1);
            return OBJ_VAL(module);
        }

        // 3. Try dir: BASE_DIR/foo/bar/baz
        path_count -= 10;
        path[path_count] = '\0';

        if (pyro_dir_exists(path)) {
            FREE_ARRAY(vm, char, path, path_length + 1);
            return OBJ_VAL(module);
        }

        FREE_ARRAY(vm, char, path, path_length + 1);
    }

    return NULL_VAL();
}


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
                vm->stack_top[-arg_count - 1] = OBJ_VAL(instance);

                Value initializer;
                if (ObjMap_get(class->methods, OBJ_VAL(vm->str_init), &initializer)) {
                    call_closure(vm, AS_CLOSURE(initializer), arg_count);
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

            default:
                break;
        }
    }
    pyro_panic(vm, ERR_TYPE_ERROR, "Value is not callable.");
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
    /* ObjClosure* method_closure = AS_CLOSURE(method_value); */
    // if name == vm->str_init add as property on class object...
    ObjClass* class = AS_CLASS(pyro_peek(vm, 1));
    if (ObjMap_set(class->methods, OBJ_VAL(name), method_value, vm) == 0) {
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

    if (ObjMap_set(class->field_indexes, OBJ_VAL(name), I64_VAL(field_index), vm) == 0) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_pop(vm);
}


// Pops the receiver and replaces it with the bound method object.
static void bind_method(PyroVM* vm, ObjClass* class, ObjStr* method_name) {
    Value method;
    if (!ObjMap_get(class->methods, OBJ_VAL(method_name), &method)) {
        pyro_panic(vm, ERR_NAME_ERROR, "Invalid method name '%s'.", method_name->bytes);
        return;
    }

    ObjBoundMethod* bound = ObjBoundMethod_new(vm, pyro_peek(vm, 0), AS_OBJ(method));
    if (!bound) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_pop(vm);
    pyro_push(vm, OBJ_VAL(bound));
}


static void invoke_from_class(PyroVM* vm, ObjClass* class, ObjStr* method_name, uint8_t arg_count) {
    Value method;
    if (!ObjMap_get(class->methods, OBJ_VAL(method_name), &method)) {
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
        invoke_from_class(vm, class, method_name, arg_count);
    } else {
        pyro_panic(vm, ERR_TYPE_ERROR, "Invalid method call '%s'.", method_name->bytes);
    }
}


static void run(PyroVM* vm) {
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    size_t frame_count_on_entry = vm->frame_count;
    assert(frame_count_on_entry >= 1);

    #define READ_BYTE()         (*frame->ip++)
    #define READ_U16()          (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_CONSTANT()     (frame->closure->fn->constants[READ_U16()])
    #define READ_STRING()       AS_STR(READ_CONSTANT())

    for (;;) {
        if (vm->halt_flag) {
            return;
        }

        #ifdef PYRO_DEBUG_TRACE_EXECUTION
            pyro_out(vm, "             ");
            if (vm->stack == vm->stack_top) {
                pyro_out(vm, "[ empty ]");
            }
            for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
                pyro_out(vm, "[ ");
                pyro_dump_value(vm, *slot);
                pyro_out(vm, " ]");
            }
            pyro_out(vm, "\n");
            size_t ip = frame->ip - frame->closure->fn->code;
            pyro_disassemble_instruction(vm, frame->closure->fn, ip);
        #endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {

            case OP_ADD: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (a.type == b.type) {
                    switch (a.type) {
                        case VAL_I64:
                            pyro_push(vm, I64_VAL(a.as.i64 + b.as.i64));
                            break;
                        case VAL_F64:
                            pyro_push(vm, F64_VAL(a.as.f64 + b.as.f64));
                            break;
                        default:
                            if (IS_STR(a) && IS_STR(b)) {
                                vm->stack_top += 2;
                                ObjStr* s2 = AS_STR(b);
                                ObjStr* s1 = AS_STR(a);
                                ObjStr* result = ObjStr_concat(s1, s2, vm);
                                if (!result) {
                                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                                    break;
                                }
                                vm->stack_top -= 2;
                                pyro_push(vm, OBJ_VAL(result));
                            } else {
                                pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '+' must both be numbers or strings.");
                            }
                    }
                } else if (IS_I64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL((double)a.as.i64 + b.as.f64));
                } else if (IS_F64(a) && IS_I64(b)) {
                    pyro_push(vm, F64_VAL(a.as.f64 + (double)b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '+' must both be numbers or strings.");
                }

                break;
            }

            case OP_ASSERT: {
                Value test_expr = pyro_pop(vm);
                if (!pyro_is_truthy(test_expr)) {
                    pyro_panic(vm, ERR_ASSERTION_FAILED, "Assertion failed.");
                }
                break;
            }

            case OP_BITWISE_AND: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, I64_VAL(a.as.i64 & b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to 'and' must both be integers.");
                }
                break;
            }

            case OP_BITWISE_OR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, I64_VAL(a.as.i64 | b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to 'or' must both be integers.");
                }
                break;
            }

            case OP_BITWISE_NOT: {
                Value operand = pyro_pop(vm);
                if (IS_I64(operand)) {
                    pyro_push(vm, I64_VAL(~operand.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Bitwise 'not' requires an integer operand.");
                }
                break;
            }

            case OP_BITWISE_XOR: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, I64_VAL(a.as.i64 ^ b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to 'xor' must both be integers.");
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

            case OP_CLASS: {
                ObjClass* class = ObjClass_new(vm);
                if (class) {
                    class->name = READ_STRING();
                    pyro_push(vm, OBJ_VAL(class));
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

            case OP_CLOSURE: {
                ObjFn* fn = AS_FN(READ_CONSTANT());
                ObjModule* module = frame->closure->module;
                ObjClosure* closure = ObjClosure_new(vm, fn, module);
                if (!closure) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                pyro_push(vm, OBJ_VAL(closure));

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
            case OP_DUP2: {
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
                    if (!string) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        return;
                    }
                    pyro_out(vm, "%s", string->bytes);
                    if (i > 1) {
                        fprintf(vm->out_file, " ");
                    }
                }

                pyro_out(vm, "\n");
                vm->stack_top -= arg_count;
                break;
            }

            case OP_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, BOOL_VAL(pyro_check_equal(a, b)));
                break;
            }

            case OP_FLOAT_DIV: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL((double)a.as.i64 / (double)b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(a.as.f64 / b.as.f64));
                } else if (IS_I64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL((double)a.as.i64 / b.as.f64));
                } else if (IS_F64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(a.as.f64 / (double)b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '/' must both be numbers.");
                }

                break;
            }

            case OP_FIELD: {
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
                if (ObjMap_get(instance->obj.class->field_indexes, field_name, &field_index)) {
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
                if (!ObjMap_get(globals, name, &value)) {
                    if (!ObjMap_get(vm->globals, name, &value)) {
                        pyro_panic(vm, ERR_NAME_ERROR, "Undefined variable '%s'.", AS_STR(name)->bytes);
                        break;
                    }
                }

                pyro_push(vm, value);
                break;
            }

            case OP_GET_INDEX: {
                invoke_method(vm, vm->str_get_index, 1);
                frame = &vm->frames[vm->frame_count - 1];
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
                if (ObjMap_get(module->globals, member_name, &value)) {
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

            case OP_GREATER: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (a.type == b.type) {
                    switch (a.type) {
                        case VAL_I64:
                            pyro_push(vm, BOOL_VAL(a.as.i64 > b.as.i64));
                            break;
                        case VAL_F64:
                            pyro_push(vm, BOOL_VAL(a.as.f64 > b.as.f64));
                            break;
                        case VAL_CHAR:
                            pyro_push(vm, BOOL_VAL(a.as.u32 > b.as.u32));
                            break;
                        case VAL_OBJ:
                            if (IS_STR(a) && IS_STR(b)) {
                                int result = pyro_compare_strings(AS_STR(a), AS_STR(b));
                                pyro_push(vm, BOOL_VAL(result > 0));
                            } else {
                                pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>' must both be numbers or strings.");
                            }
                            break;
                        default:
                            pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>' must both be numbers.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>' must both be the same type.");
                }

                break;
            }

            case OP_GREATER_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (a.type == b.type) {
                    switch (a.type) {
                        case VAL_I64:
                            pyro_push(vm, BOOL_VAL(a.as.i64 >= b.as.i64));
                            break;
                        case VAL_F64:
                            pyro_push(vm, BOOL_VAL(a.as.f64 >= b.as.f64));
                            break;
                        case VAL_CHAR:
                            pyro_push(vm, BOOL_VAL(a.as.u32 >= b.as.u32));
                            break;
                        case VAL_OBJ:
                            if (IS_STR(a) && IS_STR(b)) {
                                int result = pyro_compare_strings(AS_STR(a), AS_STR(b));
                                pyro_push(vm, BOOL_VAL(result >= 0));
                            } else {
                                pyro_panic(
                                    vm, ERR_TYPE_ERROR,
                                    "Operands to '>=' must both be numbers or strings."
                                );
                            }
                            break;
                        default:
                            pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>=' must both be numbers.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '>=' must both be the same type.");
                }

                break;
            }

            case OP_IMPORT: {
                uint8_t arg_count = READ_BYTE();
                Value* args = vm->stack_top - arg_count;

                ObjMap* supermod_map = vm->modules;
                Value module;

                for (uint8_t i = 0; i < arg_count; i++) {
                    Value name = args[i];

                    if (ObjMap_get(supermod_map, name, &module)) {
                        supermod_map = AS_MOD(module)->submodules;
                        continue;
                    }

                    module = pyro_import_module(vm, i + 1, args);
                    if (vm->halt_flag) {
                        break;
                    }
                    if (IS_NULL(module)) {
                        pyro_panic(
                            vm, ERR_MODULE_NOT_FOUND,
                            "Unable to locate module '%s'.", AS_STR(name)->bytes
                        );
                        break;
                    }

                    pyro_push(vm, module);
                    if (ObjMap_set(supermod_map, name, module, vm) == 0) {
                        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                        break;
                    }
                    pyro_pop(vm);

                    supermod_map = AS_MOD(module)->submodules;
                }

                vm->stack_top -= arg_count;
                pyro_push(vm, module);
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

                // "Copy-down inheritance". We copy all the superclass's methods into the subclass's
                // method table. This means that there's no extra runtime work involved in looking
                // up an inherited method.
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
                invoke_from_class(vm, superclass, method_name, arg_count);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_ITER: {
                invoke_method(vm, vm->str_iter, 0);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_ITER_NEXT: {
                pyro_push(vm, pyro_peek(vm, 0));
                invoke_method(vm, vm->str_next, 0);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_JUMP: {
                uint16_t offset = READ_U16();
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_ERR: {
                uint16_t offset = READ_U16();
                if (IS_ERR(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_U16();
                if (!pyro_is_truthy(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_NOT_ERR: {
                uint16_t offset = READ_U16();
                if (!IS_ERR(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_NOT_NULL: {
                uint16_t offset = READ_U16();
                if (!IS_NULL(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_TRUE: {
                uint16_t offset = READ_U16();
                if (pyro_is_truthy(pyro_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_LESS: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (a.type == b.type) {
                    switch (a.type) {
                        case VAL_I64:
                            pyro_push(vm, BOOL_VAL(a.as.i64 < b.as.i64));
                            break;
                        case VAL_F64:
                            pyro_push(vm, BOOL_VAL(a.as.f64 < b.as.f64));
                            break;
                        case VAL_CHAR:
                            pyro_push(vm, BOOL_VAL(a.as.u32 < b.as.u32));
                            break;
                        case VAL_OBJ:
                            if (IS_STR(a) && IS_STR(b)) {
                                int result = pyro_compare_strings(AS_STR(a), AS_STR(b));
                                pyro_push(vm, BOOL_VAL(result < 0));
                            } else {
                                pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<' must both be numbers or strings.");
                            }
                            break;
                        default:
                            pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<' must both be numbers or strings.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<' must both be the same type.");
                }

                break;
            }

            case OP_LESS_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (a.type == b.type) {
                    switch (a.type) {
                        case VAL_I64:
                            pyro_push(vm, BOOL_VAL(a.as.i64 <= b.as.i64));
                            break;
                        case VAL_F64:
                            pyro_push(vm, BOOL_VAL(a.as.f64 <= b.as.f64));
                            break;
                        case VAL_CHAR:
                            pyro_push(vm, BOOL_VAL(a.as.u32 <= b.as.u32));
                            break;
                        case VAL_OBJ:
                            if (IS_STR(a) && IS_STR(b)) {
                                int result = pyro_compare_strings(AS_STR(a), AS_STR(b));
                                pyro_push(vm, BOOL_VAL(result <= 0));
                            } else {
                                pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<' must both be numbers or strings.");
                            }
                            break;
                        default:
                            pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<=' must both be numbers or strings.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<=' must both be the same type.");
                }

                break;
            }

            case OP_LOAD_CONSTANT: {
                Value constant = READ_CONSTANT();
                pyro_push(vm, constant);
                break;
            }

            case OP_LOAD_FALSE:
                pyro_push(vm, BOOL_VAL(false));
                break;

            case OP_LOAD_NULL:
                pyro_push(vm, NULL_VAL());
                break;

            case OP_LOAD_TRUE:
                pyro_push(vm, BOOL_VAL(true));
                break;

            case OP_LOOP: {
                uint16_t offset = READ_U16();
                frame->ip -= offset;
                break;
            }

            case OP_LSHIFT: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 >= 0) {
                        pyro_push(vm, I64_VAL(a.as.i64 << b.as.i64));
                    } else {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Right operand to '<<' cannot be negative.");
                    }
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '<<' must both be integers.");
                }
                break;
            }

            case OP_MAKE_MAP: {
                uint16_t entry_count = READ_U16();

                ObjMap* map = ObjMap_new(vm);
                if (!map) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                // Push the map on the stack so it doesn't get gc'd while we're adding entries.
                pyro_push(vm, OBJ_VAL(map));
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
                pyro_push(vm, OBJ_VAL(map));
                break;
            }

            case OP_MAKE_VEC: {
                uint16_t item_count = READ_U16();

                ObjVec* vec = ObjVec_new_with_cap(item_count, vm);
                if (!vec) {
                    pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
                    break;
                }

                if (item_count == 0) {
                    pyro_push(vm, OBJ_VAL(vec));
                    break;
                }

                memcpy(vec->values, vm->stack_top - item_count, sizeof(Value) * item_count);
                vec->count = item_count;

                vm->stack_top -= item_count;
                pyro_push(vm, OBJ_VAL(vec));
                break;
            }

            case OP_METHOD: {
                define_method(vm, READ_STRING());
                break;
            }

            case OP_MODULO: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, I64_VAL(a.as.i64 % b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(fmod(a.as.f64, b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(fmod((double)a.as.i64, b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Modulo by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(fmod(a.as.f64, (double)b.as.i64)));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '%%' must both be numbers.");
                }

                break;
            }

            case OP_MULTIPLY: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, I64_VAL(a.as.i64 * b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL(a.as.f64 * b.as.f64));
                } else if (IS_I64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL((double)a.as.i64 * b.as.f64));
                } else if (IS_F64(a) && IS_I64(b)) {
                    pyro_push(vm, F64_VAL(a.as.f64 * (double)b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '*' must both be numbers.");
                }

                break;
            }

            case OP_NEGATE: {
                Value operand = pyro_pop(vm);
                switch (operand.type) {
                    case VAL_I64:
                        pyro_push(vm, I64_VAL(-operand.as.i64));
                        break;
                    case VAL_F64:
                        pyro_push(vm, F64_VAL(-operand.as.f64));
                        break;
                    default:
                        pyro_panic(vm, ERR_TYPE_ERROR, "Operand to '-' must be a number.");
                }
                break;
            }

            case OP_LOGICAL_NOT: {
                Value operand = pyro_pop(vm);
                pyro_push(vm, BOOL_VAL(!pyro_is_truthy(operand)));
                break;
            }

            case OP_NOT_EQUAL: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                pyro_push(vm, BOOL_VAL(!pyro_check_equal(a, b)));
                break;
            }

            case OP_POP:
                pyro_pop(vm);
                break;

            case OP_POP_JUMP_IF_FALSE: {
                uint16_t offset = READ_U16();
                if (!pyro_is_truthy(pyro_pop(vm))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_POWER: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, F64_VAL(pow((double)a.as.i64, (double)b.as.i64)));
                } else if (IS_F64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL(pow(a.as.f64, b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL(pow((double)a.as.i64, b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    pyro_push(vm, F64_VAL(pow(a.as.f64, (double)b.as.i64)));
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

            case OP_RSHIFT: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);
                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 >= 0) {
                        pyro_push(vm, I64_VAL(a.as.i64 >> b.as.i64));
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
                if (ObjMap_get(instance->obj.class->field_indexes, field_name, &field_index)) {
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
                invoke_method(vm, vm->str_set_index, 2);
                frame = &vm->frames[vm->frame_count - 1];
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

            case OP_SUBTRACT: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    pyro_push(vm, I64_VAL(a.as.i64 - b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL(a.as.f64 - b.as.f64));
                } else if (IS_I64(a) && IS_F64(b)) {
                    pyro_push(vm, F64_VAL((double)a.as.i64 - b.as.f64));
                } else if (IS_F64(a) && IS_I64(b)) {
                    pyro_push(vm, F64_VAL(a.as.f64 - (double)b.as.i64));
                } else {
                    pyro_panic(vm, ERR_TYPE_ERROR, "Operands to '-' must both be numbers.");
                }

                break;
            }

            case OP_TRUNC_DIV: {
                Value b = pyro_pop(vm);
                Value a = pyro_pop(vm);

                if (IS_I64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, I64_VAL(a.as.i64 / b.as.i64));
                } else if (IS_F64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(trunc(a.as.f64 / b.as.f64)));
                } else if (IS_I64(a) && IS_F64(b)) {
                    if (b.as.f64 == 0.0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(trunc((double)a.as.i64 / b.as.f64)));
                } else if (IS_F64(a) && IS_I64(b)) {
                    if (b.as.i64 == 0) {
                        pyro_panic(vm, ERR_VALUE_ERROR, "Division by zero.");
                        break;
                    }
                    pyro_push(vm, F64_VAL(trunc(a.as.f64 / (double)b.as.i64)));
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

                    // Create an error tuple as the return value of the try expression.
                    ObjStr* err_msg = ObjStr_copy_raw(vm->panic_buffer, strlen(vm->panic_buffer), vm);
                    if (err_msg) {
                        pyro_push(vm, OBJ_VAL(err_msg));
                        ObjTup* err = ObjTup_new_err(2, vm);
                        pyro_pop(vm);
                        if (err) {
                            err->values[0] = I64_VAL(error_code);
                            err->values[1] = OBJ_VAL(err_msg);
                            pyro_push(vm, OBJ_VAL(err));
                        }
                    }

                    // Hard panic if we failed to allocate memory for the error string or tuple.
                    if (vm->memory_allocation_failed) {
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

    #undef READ_U16
    #undef READ_STRING
    #undef READ_CONSTANT
    #undef READ_BYTE
}


static void reset_stack(PyroVM* vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}


/* ---------------- */
/* Public Interface */
/* ---------------- */


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
    vm->tup_iter_class = NULL;
    vm->vec_iter_class = NULL;
    vm->map_iter_class = NULL;
    vm->str_iter_class = NULL;
    vm->main_module = NULL;
    vm->import_roots = NULL;
    vm->range_class = NULL;
    vm->file_class = NULL;
    vm->mt64 = NULL;
    vm->str_debug = NULL;
    vm->max_bytes = SIZE_MAX;
    vm->memory_allocation_failed = false;
    vm->gc_disallows = 0;

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
    vm->tup_iter_class = ObjClass_new(vm);
    vm->vec_iter_class = ObjClass_new(vm);
    vm->map_iter_class = ObjClass_new(vm);
    vm->str_iter_class = ObjClass_new(vm);
    vm->range_class = ObjClass_new(vm);
    vm->file_class = ObjClass_new(vm);

    if (vm->memory_allocation_failed) {
        pyro_free_vm(vm);
        return NULL;
    }

    // We need to initialize the interned strings pool before we create any strings.
    vm->strings = ObjMap_new_weakref(vm);
    if (!vm->strings) {
        pyro_free_vm(vm);
        return NULL;
    }

    // Canned objects, mostly static strings.
    vm->empty_error = ObjTup_new_err(0, vm);
    vm->empty_string = ObjStr_empty(vm);
    vm->str_init = STR_OBJ("$init");
    vm->str_str = STR_OBJ("$str");
    vm->str_true = STR_OBJ("true");
    vm->str_false = STR_OBJ("false");
    vm->str_null = STR_OBJ("null");
    vm->str_fmt = STR_OBJ("$fmt");
    vm->str_iter = STR_OBJ("$iter");
    vm->str_next = STR_OBJ("$next");
    vm->str_get_index = STR_OBJ("$get_index");
    vm->str_set_index = STR_OBJ("$set_index");
    vm->str_debug = STR_OBJ("$debug");

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
    pyro_load_std_core_range(vm);

    // Load individual standard library modules.
    pyro_load_std_math(vm);
    pyro_load_std_prng(vm);
    pyro_load_std_pyro(vm);
    pyro_load_std_errors(vm);

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


void pyro_out(PyroVM* vm, const char* format, ...) {
    if (vm->out_file) {
        va_list args;
        va_start(args, format);
        vfprintf(vm->out_file, format, args);
        va_end(args);
    }
}


void pyro_err(PyroVM* vm, const char* format, ...) {
    if (vm->err_file) {
        va_list args;
        va_start(args, format);
        vfprintf(vm->err_file, format, args);
        va_end(args);
    }
}


void pyro_print_stack_trace(PyroVM* vm, FILE* file) {
    if (file) {
        fprintf(file, "Traceback (most recent function first):\n\n");

        for (size_t i = vm->frame_count; i > 0; i--) {
            CallFrame* frame = &vm->frames[i - 1];
            ObjFn* fn = frame->closure->fn;

            size_t line_number = 1;
            if (frame->ip > fn->code) {
                size_t ip = frame->ip - fn->code - 1;
                line_number = ObjFn_get_line_number(fn, ip);
            }

            fprintf(file, "%s:%zu\n", fn->source->bytes, line_number);
            fprintf(file, "  [%zu] --> in %s\n", i, fn->name->bytes);
        }
    }
}


void pyro_panic(PyroVM* vm, int64_t error_code, const char* format, ...) {
    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->status_code = error_code;

    if (format == NULL) {
        return;
    }

    // If we're inside a try expression and the panic is catchable, we don't print anything to the
    // error stream. We simply store the error message in the panic buffer so it can be returned
    // by the try expression.
    if (vm->try_depth > 0 && !vm->hard_panic) {
        va_list args;
        va_start(args, format);
        vsnprintf(vm->panic_buffer, PYRO_PANIC_BUFFER_SIZE, format, args);
        va_end(args);
        return;
    }

    // Print the source filename and line number if available.
    if (vm->frame_count > 0 && vm->err_file) {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        ObjFn* fn = frame->closure->fn;
        size_t line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = ObjFn_get_line_number(fn, ip);
        }
        fprintf(vm->err_file, "%s:%zu\n  ", fn->source->bytes, line_number);
    }

    // Print the error message.
    if (vm->err_file) {
        fprintf(vm->err_file, "[%lld] Error: ", error_code);
        va_list args;
        va_start(args, format);
        vfprintf(vm->err_file, format, args);
        va_end(args);
        fprintf(vm->err_file, "\n");
    }

    // Print a stack trace if the panic occurred inside a Pyro function.
    if (vm->frame_count > 1 && vm->err_file) {
        fprintf(vm->err_file, "\n");
        pyro_print_stack_trace(vm, vm->err_file);
    }
}


void pyro_exec_code_as_main(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    vm->halt_flag = false;
    vm->exit_flag = false;
    vm->panic_flag = false;
    vm->hard_panic = false;
    vm->status_code = 0;

    ObjFn* fn = pyro_compile(vm, src_code, src_len, src_id);
    if (!fn) {
        if (vm->status_code == ERR_SYNTAX_ERROR) {
            pyro_panic(vm, ERR_SYNTAX_ERROR, NULL);
        } else if (vm->status_code == ERR_OUT_OF_MEMORY) {
            pyro_panic(
                vm, ERR_OUT_OF_MEMORY,
                "Out of memory, unable to compile '%s'.", src_id
            );
        } else {
            assert(false);
        }
        return;
    }

    pyro_push(vm, OBJ_VAL(fn));
    ObjClosure* closure = ObjClosure_new(vm, fn, vm->main_module);
    pyro_pop(vm);
    if (!closure) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_push(vm, OBJ_VAL(closure));
    call_value(vm, OBJ_VAL(closure), 0);
    run(vm);
    pyro_pop(vm);

    if (vm->halt_flag) {
        reset_stack(vm);
    }
    assert(vm->stack_top == vm->stack);
}


void pyro_exec_file_as_main(PyroVM* vm, const char* path) {
    ObjStr* path_string = STR_OBJ(path);
    if (!path_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }
    pyro_define_member(vm, vm->main_module, "$filepath", OBJ_VAL(path_string));

    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }

    pyro_exec_code_as_main(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
}


void pyro_test_compile_file(PyroVM* vm, const char* path) {
    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }

    ObjFn* fn = pyro_compile(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
    if (!fn) {
        if (vm->status_code == ERR_SYNTAX_ERROR) {
            pyro_panic(vm, ERR_SYNTAX_ERROR, NULL);
        } else if (vm->status_code == ERR_OUT_OF_MEMORY) {
            pyro_panic(
                vm, ERR_OUT_OF_MEMORY,
                "Out of memory, unable to compile file '%s'.", path
            );
        } else {
            assert(false);
        }
    }
    return;
}


void pyro_exec_file_as_module(PyroVM* vm, const char* path, ObjModule* module) {
    ObjStr* path_string = STR_OBJ(path);
    if (!path_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }
    pyro_define_member(vm, module, "$filepath", OBJ_VAL(path_string));

    FileData fd;
    if (!pyro_read_file(vm, path, &fd) || fd.size == 0) {
        return;
    }

    ObjFn* fn = pyro_compile(vm, fd.data, fd.size, path);
    FREE_ARRAY(vm, char, fd.data, fd.size);
    if (!fn) {
        if (vm->status_code == ERR_SYNTAX_ERROR) {
            pyro_panic(vm, ERR_SYNTAX_ERROR, "Unable to compile module '%s'.", path);
        } else if (vm->status_code == ERR_OUT_OF_MEMORY) {
            pyro_panic(
                vm, ERR_OUT_OF_MEMORY,
                "Out of memory, unable to compile module '%s'.", path
            );
        } else {
            assert(false);
        }
        return;
    }

    pyro_push(vm, OBJ_VAL(fn));
    ObjClosure* closure = ObjClosure_new(vm, fn, module);
    pyro_pop(vm);
    if (!closure) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    pyro_push(vm, OBJ_VAL(closure));
    call_value(vm, OBJ_VAL(closure), 0);
    run(vm);
    pyro_pop(vm);
}


void pyro_run_main_func(PyroVM* vm) {
    ObjStr* main_string = STR_OBJ("$main");
    if (!main_string) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return;
    }

    Value main_value;
    if (ObjMap_get(vm->main_module->globals, OBJ_VAL(main_string), &main_value)) {
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

    for (size_t i = 0; i < vm->main_module->globals->capacity; i++) {
        MapEntry entry = vm->main_module->globals->entries[i];
        if (IS_STR(entry.key) && IS_CLOSURE(entry.value)) {
            ObjStr* name = AS_STR(entry.key);
            if (name->length > 6 && memcmp(name->bytes, "$test_", 6) == 0) {
                if (AS_CLOSURE(entry.value)->fn->arity > 0) {
                    pyro_out(vm, " · Invalid test function (%s), too many args.\n", name->bytes);
                    tests_failed += 1;
                    continue;
                }

                vm->halt_flag = false;
                vm->exit_flag = false;
                vm->panic_flag = false;
                vm->hard_panic = false;
                vm->status_code = 0;

                pyro_push(vm, entry.value);
                call_value(vm, entry.value, 0);
                run(vm);
                pyro_pop(vm);

                if (vm->halt_flag) {
                    reset_stack(vm);
                }
                assert(vm->stack_top == vm->stack);

                if (vm->exit_flag) {
                    pyro_out(vm, " · \x1B[1;31mEXIT\x1B[0m (%d) %s\n", vm->status_code, name->bytes);
                    tests_failed += 1;
                    break;
                }

                if (vm->hard_panic) {
                    pyro_out(vm, " · \x1B[1;31mHARD PANIC\x1B[0m %s\n", name->bytes);
                    tests_failed += 1;
                    break;
                }

                if (vm->panic_flag) {
                    pyro_out(vm, " · \x1B[1;31mFAIL\x1B[0m %s\n", name->bytes);
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
    for (size_t i = 0; i < vm->main_module->globals->capacity; i++) {
        MapEntry entry = vm->main_module->globals->entries[i];
        if (IS_STR(entry.key) && IS_CLOSURE(entry.value)) {
            ObjStr* name = AS_STR(entry.key);
            if (name->length > 6 && memcmp(name->bytes, "$time_", 6) == 0) {
                if (AS_CLOSURE(entry.value)->fn->arity > 0) {
                    pyro_out(vm, " · Invalid time function (%s), too many args.\n", name->bytes);
                    continue;
                }

                double start_time = clock();

                for (size_t j = 0; j < num_iterations; j++) {
                    pyro_push(vm, entry.value);
                    call_value(vm, entry.value, 0);
                    run(vm);
                    pyro_pop(vm);

                    if (vm->halt_flag) {
                        reset_stack(vm);
                        return;
                    }
                }

                double runtime = (double)(clock() - start_time) / CLOCKS_PER_SEC;
                pyro_out(vm, " · %s() ··· %f secs\n", name->bytes, runtime / num_iterations);
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


Value pyro_call_fn(PyroVM* vm, Value func, uint8_t arg_count) {
    if (IS_NATIVE_FN(func)) {
        call_native_fn(vm, AS_NATIVE_FN(func), arg_count);
        return pyro_pop(vm);
    } else {
        call_value(vm, func, arg_count);
        run(vm);
        return pyro_pop(vm);
    }
}


ObjModule* pyro_define_module_1(PyroVM* vm, const char* name) {
    ObjStr* name_object = STR_OBJ(name);
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
    ObjStr* parent_object = STR_OBJ(parent);
    if (!parent_object) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(vm->modules, OBJ_VAL(parent_object), &parent_module)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_object = STR_OBJ(name);
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
    ObjStr* grandparent_object = STR_OBJ(parent);
    if (!grandparent_object) {
        return NULL;
    }

    Value grandparent_module;
    if (!ObjMap_get(vm->modules, OBJ_VAL(grandparent_object), &grandparent_module)) {
        assert(false);
        return NULL;
    }

    ObjStr* parent_object = STR_OBJ(parent);
    if (!parent_object) {
        return NULL;
    }

    Value parent_module;
    if (!ObjMap_get(AS_MOD(grandparent_module)->submodules, OBJ_VAL(parent_object), &parent_module)) {
        assert(false);
        return NULL;
    }

    ObjStr* name_object = STR_OBJ(name);
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


void pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value value) {
    pyro_push(vm, value);

    ObjStr* name_object = STR_OBJ(name);
    if (!name_object) {
        pyro_pop(vm); // value
        return;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjMap_set(module->globals, name_value, value, vm);
    pyro_pop(vm); // name_value
    pyro_pop(vm); // value
}


void pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return;
    }
    pyro_define_member(vm, module, name, OBJ_VAL(func_object));
}


void pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity) {
    ObjStr* name_object = STR_OBJ(name);
    if (!name_object) {
        return;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        pyro_pop(vm); // name_value
        return;
    }
    Value func_value = OBJ_VAL(func_object);
    pyro_push(vm, func_value);

    ObjMap_set(class->methods, name_value, func_value, vm);
    pyro_pop(vm); // func_value
    pyro_pop(vm); // name_value
}


void pyro_define_global(PyroVM* vm, const char* name, Value value) {
    pyro_push(vm, value);

    ObjStr* name_object = STR_OBJ(name);
    if (!name_object) {
        pyro_pop(vm); // value
        return;
    }
    Value name_value = OBJ_VAL(name_object);
    pyro_push(vm, name_value);

    ObjMap_set(vm->globals, name_value, value, vm);

    pyro_pop(vm); // name_value
    pyro_pop(vm); // value
}


void pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity) {
    ObjNativeFn* func_object = ObjNativeFn_new(vm, fn_ptr, name, arity);
    if (!func_object) {
        return;
    }
    pyro_define_global(vm, name, OBJ_VAL(func_object));
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
        ObjStr* string = STR_OBJ(args[i]);
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
    ObjStr* string = STR_OBJ(path);
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
