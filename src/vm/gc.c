#include "../inc/heap.h"
#include "../inc/vm.h"
#include "../inc/stringify.h"
#include "../inc/panics.h"
#include "../inc/values.h"
#include "../inc/objects.h"


// Marks an object as reachable. This sets the object's [is_marked] flag and pushes it onto the
// grey stack. This function will set the panic flag BUT NOT call pyro_panic() if an attempt to
// allocate memory for the grey stack fails.
static void mark_object(PyroVM* vm, Obj* object) {
    if (object == NULL || object->is_marked || vm->panic_flag) {
        return;
    }

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(
            vm,
            "   %p mark %s\n",
            (void*)object,
            pyro_stringify_object_type(object->type)
        );
    #endif

    if (vm->grey_stack_count == vm->grey_stack_capacity) {
        size_t new_capacity = GROW_CAPACITY(vm->grey_stack_capacity);
        Obj** new_array = REALLOCATE_ARRAY(vm, Obj*, vm->grey_stack, vm->grey_stack_capacity, new_capacity);
        if (!new_array) {
            vm->panic_flag = true;
            return;
        }
        vm->grey_stack_capacity = new_capacity;
        vm->grey_stack = new_array;
    }

    object->is_marked = true;
    vm->grey_stack[vm->grey_stack_count++] = object;
}


// Marks a value as reachable.
static void mark_value(PyroVM* vm, Value value) {
    if (IS_OBJ(value)) {
        mark_object(vm, AS_OBJ(value));
    }
}


// Marks every root object as reachable. (A root object is an object the VM can access directly
// without going through another object.)
static void mark_roots(PyroVM* vm) {
    // Local variables and temporary values on the stack.
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(vm, *slot);
    }

    // Classes for builtin types.
    mark_object(vm, (Obj*)vm->class_map);
    mark_object(vm, (Obj*)vm->class_str);
    mark_object(vm, (Obj*)vm->class_tup);
    mark_object(vm, (Obj*)vm->class_vec);
    mark_object(vm, (Obj*)vm->class_buf);
    mark_object(vm, (Obj*)vm->class_file);
    mark_object(vm, (Obj*)vm->class_iter);
    mark_object(vm, (Obj*)vm->class_stack);
    mark_object(vm, (Obj*)vm->class_set);
    mark_object(vm, (Obj*)vm->class_queue);
    mark_object(vm, (Obj*)vm->class_err);
    mark_object(vm, (Obj*)vm->class_module);
    mark_object(vm, (Obj*)vm->class_char);

    // The VM's pool of canned objects.
    mark_object(vm, (Obj*)vm->error);
    mark_object(vm, (Obj*)vm->empty_string);
    mark_object(vm, (Obj*)vm->str_dollar_init);
    mark_object(vm, (Obj*)vm->str_dollar_str);
    mark_object(vm, (Obj*)vm->str_true);
    mark_object(vm, (Obj*)vm->str_false);
    mark_object(vm, (Obj*)vm->str_null);
    mark_object(vm, (Obj*)vm->str_dollar_fmt);
    mark_object(vm, (Obj*)vm->str_dollar_iter);
    mark_object(vm, (Obj*)vm->str_dollar_next);
    mark_object(vm, (Obj*)vm->str_dollar_get_index);
    mark_object(vm, (Obj*)vm->str_dollar_set_index);
    mark_object(vm, (Obj*)vm->str_dollar_debug);
    mark_object(vm, (Obj*)vm->str_op_binary_equals_equals);
    mark_object(vm, (Obj*)vm->str_op_binary_less);
    mark_object(vm, (Obj*)vm->str_op_binary_less_equals);
    mark_object(vm, (Obj*)vm->str_op_binary_greater);
    mark_object(vm, (Obj*)vm->str_op_binary_greater_equals);
    mark_object(vm, (Obj*)vm->str_op_binary_plus);
    mark_object(vm, (Obj*)vm->str_op_binary_minus);
    mark_object(vm, (Obj*)vm->str_op_binary_bar);
    mark_object(vm, (Obj*)vm->str_op_binary_amp);
    mark_object(vm, (Obj*)vm->str_op_binary_star);
    mark_object(vm, (Obj*)vm->str_op_binary_slash);
    mark_object(vm, (Obj*)vm->str_op_binary_caret);
    mark_object(vm, (Obj*)vm->str_op_binary_percent);
    mark_object(vm, (Obj*)vm->str_op_binary_star_star);
    mark_object(vm, (Obj*)vm->str_op_binary_slash_slash);
    mark_object(vm, (Obj*)vm->str_op_unary_plus);
    mark_object(vm, (Obj*)vm->str_op_unary_minus);
    mark_object(vm, (Obj*)vm->str_dollar_hash);
    mark_object(vm, (Obj*)vm->str_dollar_call);
    mark_object(vm, (Obj*)vm->str_dollar_contains);
    mark_object(vm, (Obj*)vm->str_bool);
    mark_object(vm, (Obj*)vm->str_i64);
    mark_object(vm, (Obj*)vm->str_f64);
    mark_object(vm, (Obj*)vm->str_char);
    mark_object(vm, (Obj*)vm->str_method);
    mark_object(vm, (Obj*)vm->str_buf);
    mark_object(vm, (Obj*)vm->str_class);
    mark_object(vm, (Obj*)vm->str_fn);
    mark_object(vm, (Obj*)vm->str_instance);
    mark_object(vm, (Obj*)vm->str_file);
    mark_object(vm, (Obj*)vm->str_iter);
    mark_object(vm, (Obj*)vm->str_map);
    mark_object(vm, (Obj*)vm->str_set);
    mark_object(vm, (Obj*)vm->str_vec);
    mark_object(vm, (Obj*)vm->str_stack);
    mark_object(vm, (Obj*)vm->str_queue);
    mark_object(vm, (Obj*)vm->str_str);
    mark_object(vm, (Obj*)vm->str_module);
    mark_object(vm, (Obj*)vm->str_tup);
    mark_object(vm, (Obj*)vm->str_err);
    mark_object(vm, (Obj*)vm->str_dollar_end_with);
    mark_object(vm, (Obj*)vm->str_source);
    mark_object(vm, (Obj*)vm->str_line);

    // Other object fields.
    mark_object(vm, (Obj*)vm->superglobals);
    mark_object(vm, (Obj*)vm->modules);
    mark_object(vm, (Obj*)vm->strings);
    mark_object(vm, (Obj*)vm->main_module);
    mark_object(vm, (Obj*)vm->import_roots);
    mark_object(vm, (Obj*)vm->stdout_stream);
    mark_object(vm, (Obj*)vm->stderr_stream);
    mark_object(vm, (Obj*)vm->stdin_file);
    mark_object(vm, (Obj*)vm->panic_buffer);
    mark_object(vm, (Obj*)vm->panic_source_id);

    // Each CallFrame in the call stack has a pointer to an ObjClosure.
    for (size_t i = 0; i < vm->frame_count; i++) {
        mark_object(vm, (Obj*)vm->frames[i].closure);
    }

    // The VM's linked-list of open upvalues.
    for (ObjUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object(vm, (Obj*)upvalue);
    }

    // Values on the 'with' stack with pending $end_with() method calls.
    for (size_t i = 0; i < vm->with_stack_count; i++) {
        mark_value(vm, vm->with_stack[i]);
    }
}


static void blacken_object(PyroVM* vm, Obj* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(
            vm,
            "   %p blacken %s\n",
            (void*)object,
            pyro_stringify_object_type(object->type)
        );
    #endif

    mark_object(vm, (Obj*)object->class);

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            mark_value(vm, bound->receiver);
            mark_object(vm, (Obj*)bound->method);
            break;
        }

        case OBJ_BUF:
            break;

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            mark_object(vm, (Obj*)class->name);
            mark_object(vm, (Obj*)class->superclass);
            mark_object(vm, (Obj*)class->all_instance_methods);
            mark_object(vm, (Obj*)class->pub_instance_methods);
            mark_object(vm, (Obj*)class->all_field_indexes);
            mark_object(vm, (Obj*)class->pub_field_indexes);
            mark_object(vm, (Obj*)class->default_field_values);
            mark_object(vm, (Obj*)class->static_methods);
            mark_object(vm, (Obj*)class->static_fields);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            mark_object(vm, (Obj*)closure->fn);
            mark_object(vm, (Obj*)closure->module);
            mark_object(vm, (Obj*)closure->default_values);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                mark_object(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            mark_object(vm, (Obj*)file->path);
            break;
        }

        case OBJ_PYRO_FN: {
            ObjPyroFn* fn = (ObjPyroFn*)object;
            mark_object(vm, (Obj*)fn->name);
            mark_object(vm, (Obj*)fn->source_id);
            for (size_t i = 0; i < fn->constants_count; i++) {
                mark_value(vm, fn->constants[i]);
            }
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            for (int i = 0; i < num_fields; i++) {
                mark_value(vm, instance->fields[i]);
            }
            break;
        }

        case OBJ_ITER: {
            ObjIter* iter = (ObjIter*)object;
            mark_object(vm, (Obj*)iter->source);
            mark_object(vm, (Obj*)iter->callback);
            break;
        }

        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                MapEntry* entry = &map->entry_array[i];
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
                mark_value(vm, entry->value);
            }
            break;
        }

        case OBJ_MAP_AS_SET: {
            ObjMap* map = (ObjMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                MapEntry* entry = &map->entry_array[i];
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
            }
            break;
        }

        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            mark_object(vm, (Obj*)module->submodules);
            mark_object(vm, (Obj*)module->members);
            mark_object(vm, (Obj*)module->all_member_indexes);
            mark_object(vm, (Obj*)module->pub_member_indexes);
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            mark_object(vm, (Obj*)native->name);
            break;
        }

        case OBJ_QUEUE: {
            ObjQueue* queue = (ObjQueue*)object;
            QueueItem* next_item = queue->head;
            while (next_item) {
                mark_value(vm, next_item->value);
                next_item = next_item->next;
            }
            break;
        }

        case OBJ_STR:
            break;

        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            for (size_t i = 0; i < tup->count; i++) {
                mark_value(vm, tup->values[i]);
            }
            break;
        }

        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)object;
            mark_value(vm, upvalue->closed);
            break;
        }

        case OBJ_VEC_AS_STACK:
        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            for (size_t i = 0; i < vec->count; i++) {
                mark_value(vm, vec->values[i]);
            }
            break;
        }

        case OBJ_MAP_AS_WEAKREF:
            break;

        case OBJ_ERR: {
            ObjErr* err = (ObjErr*)object;
            mark_object(vm, (Obj*)err->message);
            mark_object(vm, (Obj*)err->details);
            break;
        }

        case OBJ_RESOURCE_POINTER:
            break;
    }
}


static void trace_references(PyroVM* vm) {
    while (vm->grey_stack_count > 0) {
        Obj* object = vm->grey_stack[--vm->grey_stack_count];
        blacken_object(vm, object);
        if (vm->panic_flag) {
            return;
        }
    }
}


static void sweep(PyroVM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;

    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* not_marked = object;
            object = object->next;
            if (previous == NULL) {
                vm->objects = object;
            } else {
                previous->next = object;
            }
            pyro_free_object(vm, not_marked);
        }
    }
}


static void undo_mark_objects(PyroVM* vm) {
    Obj* obj = vm->objects;
    while (obj != NULL) {
        obj->is_marked = false;
        obj = obj->next;
    }
}


void pyro_collect_garbage(PyroVM* vm) {
    assert(vm->grey_stack_count == 0);

    if (vm->gc_disallows > 0 || vm->panic_flag) {
        return;
    }

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(vm, "-- gc begin\n");
        size_t initial_bytes_allocated = vm->bytes_allocated;
    #endif

    // If we make it to here, we're not in a panic state.
    // - Attempt to mark every root object as reachable -- i.e. set the object's [is_marked] flag
    //   and push it onto the grey stack.
    // - This call can only fail (and set the panic flag) if an attempt to allocate memory for the
    //   grey stack fails.
    mark_roots(vm);
    if (vm->panic_flag) {
        vm->grey_stack_count = 0;
        undo_mark_objects(vm);
        pyro_panic(vm, "out of memory: failed to allocate memory for the garbage collector");
        return;
    }

    // If we make it to here, we're not in a panic state.
    // - Attempt to mark every object reachable from the root objects as reachable -- i.e. set the
    //   object's [is_marked] flag.
    // - This call can only fail (and set the panic flag) if an attempt to allocate memory for the
    //   grey stack fails.
    trace_references(vm);
    if (vm->panic_flag) {
        vm->grey_stack_count = 0;
        undo_mark_objects(vm);
        pyro_panic(vm, "out of memory: failed to allocate memory for the garbage collector");
        return;
    }

    // If we make it to here, we've marked every reachable object as [is_marked] without panicking.
    assert(vm->grey_stack_count == 0);

    // Free every non-reachable object, i.e. every objeect with [is_marked == false].
    sweep(vm);

    // Update the GC threshold.
    vm->next_gc_threshold = vm->bytes_allocated * PYRO_GC_HEAP_GROW_FACTOR;

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(vm, "-- gc end\n");
        pyro_write_stdout(vm, "-- gc collected %zu bytes (from %zu to %zu) next gc at %zu\n",
            initial_bytes_allocated - vm->bytes_allocated,
            initial_bytes_allocated,
            vm->bytes_allocated,
            vm->next_gc_threshold
        );
    #endif
}
