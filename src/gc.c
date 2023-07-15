#include "../inc/pyro.h"


// Marks an object as reachable. This sets the object's [is_marked] flag and pushes it onto the
// grey stack. This function will set the panic flag BUT NOT call pyro_panic() if an attempt to
// allocate memory for the grey stack fails.
static void mark_object(PyroVM* vm, PyroObject* object) {
    if (object == NULL || object->is_marked || vm->panic_flag) {
        return;
    }

    if (vm->grey_stack_count == vm->grey_stack_capacity) {
        size_t new_capacity = PYRO_GROW_CAPACITY(vm->grey_stack_capacity);
        PyroObject** new_array = PYRO_REALLOCATE_ARRAY(vm, PyroObject*, vm->grey_stack, vm->grey_stack_capacity, new_capacity);
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
static void mark_value(PyroVM* vm, PyroValue value) {
    if (PYRO_IS_OBJ(value)) {
        mark_object(vm, PYRO_AS_OBJ(value));
    }
}


// Marks every root object as reachable. (A root object is an object the VM can access directly
// without going through another object.)
static void mark_roots(PyroVM* vm) {
    // Local variables and temporary values on the stack.
    for (PyroValue* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(vm, *slot);
    }

    // Classes for builtin types.
    mark_object(vm, (PyroObject*)vm->class_map);
    mark_object(vm, (PyroObject*)vm->class_str);
    mark_object(vm, (PyroObject*)vm->class_tup);
    mark_object(vm, (PyroObject*)vm->class_vec);
    mark_object(vm, (PyroObject*)vm->class_buf);
    mark_object(vm, (PyroObject*)vm->class_file);
    mark_object(vm, (PyroObject*)vm->class_iter);
    mark_object(vm, (PyroObject*)vm->class_stack);
    mark_object(vm, (PyroObject*)vm->class_set);
    mark_object(vm, (PyroObject*)vm->class_queue);
    mark_object(vm, (PyroObject*)vm->class_err);
    mark_object(vm, (PyroObject*)vm->class_module);
    mark_object(vm, (PyroObject*)vm->class_char);

    // The VM's pool of canned objects.
    mark_object(vm, (PyroObject*)vm->error);
    mark_object(vm, (PyroObject*)vm->empty_string);
    mark_object(vm, (PyroObject*)vm->str_dollar_init);
    mark_object(vm, (PyroObject*)vm->str_dollar_str);
    mark_object(vm, (PyroObject*)vm->str_true);
    mark_object(vm, (PyroObject*)vm->str_false);
    mark_object(vm, (PyroObject*)vm->str_null);
    mark_object(vm, (PyroObject*)vm->str_dollar_fmt);
    mark_object(vm, (PyroObject*)vm->str_dollar_iter);
    mark_object(vm, (PyroObject*)vm->str_dollar_next);
    mark_object(vm, (PyroObject*)vm->str_dollar_get);
    mark_object(vm, (PyroObject*)vm->str_dollar_set);
    mark_object(vm, (PyroObject*)vm->str_dollar_debug);
    mark_object(vm, (PyroObject*)vm->str_op_binary_equals_equals);
    mark_object(vm, (PyroObject*)vm->str_op_binary_less);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_less);
    mark_object(vm, (PyroObject*)vm->str_op_binary_less_equals);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_less_equals);
    mark_object(vm, (PyroObject*)vm->str_op_binary_greater);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_greater);
    mark_object(vm, (PyroObject*)vm->str_op_binary_greater_equals);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_greater_equals);
    mark_object(vm, (PyroObject*)vm->str_op_binary_plus);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_plus);
    mark_object(vm, (PyroObject*)vm->str_op_binary_minus);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_minus);
    mark_object(vm, (PyroObject*)vm->str_op_binary_bar);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_bar);
    mark_object(vm, (PyroObject*)vm->str_op_binary_amp);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_amp);
    mark_object(vm, (PyroObject*)vm->str_op_binary_star);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_star);
    mark_object(vm, (PyroObject*)vm->str_op_binary_slash);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_slash);
    mark_object(vm, (PyroObject*)vm->str_op_binary_caret);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_caret);
    mark_object(vm, (PyroObject*)vm->str_op_binary_percent);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_percent);
    mark_object(vm, (PyroObject*)vm->str_op_binary_star_star);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_star_star);
    mark_object(vm, (PyroObject*)vm->str_op_binary_slash_slash);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_slash_slash);
    mark_object(vm, (PyroObject*)vm->str_op_unary_plus);
    mark_object(vm, (PyroObject*)vm->str_op_unary_minus);
    mark_object(vm, (PyroObject*)vm->str_dollar_hash);
    mark_object(vm, (PyroObject*)vm->str_dollar_call);
    mark_object(vm, (PyroObject*)vm->str_dollar_contains);
    mark_object(vm, (PyroObject*)vm->str_bool);
    mark_object(vm, (PyroObject*)vm->str_i64);
    mark_object(vm, (PyroObject*)vm->str_f64);
    mark_object(vm, (PyroObject*)vm->str_char);
    mark_object(vm, (PyroObject*)vm->str_method);
    mark_object(vm, (PyroObject*)vm->str_buf);
    mark_object(vm, (PyroObject*)vm->str_class);
    mark_object(vm, (PyroObject*)vm->str_func);
    mark_object(vm, (PyroObject*)vm->str_instance);
    mark_object(vm, (PyroObject*)vm->str_file);
    mark_object(vm, (PyroObject*)vm->str_iter);
    mark_object(vm, (PyroObject*)vm->str_map);
    mark_object(vm, (PyroObject*)vm->str_set);
    mark_object(vm, (PyroObject*)vm->str_vec);
    mark_object(vm, (PyroObject*)vm->str_stack);
    mark_object(vm, (PyroObject*)vm->str_queue);
    mark_object(vm, (PyroObject*)vm->str_str);
    mark_object(vm, (PyroObject*)vm->str_module);
    mark_object(vm, (PyroObject*)vm->str_tup);
    mark_object(vm, (PyroObject*)vm->str_err);
    mark_object(vm, (PyroObject*)vm->str_dollar_end_with);
    mark_object(vm, (PyroObject*)vm->str_source);
    mark_object(vm, (PyroObject*)vm->str_line);
    mark_object(vm, (PyroObject*)vm->str_op_binary_less_less);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_less_less);
    mark_object(vm, (PyroObject*)vm->str_op_binary_greater_greater);
    mark_object(vm, (PyroObject*)vm->str_rop_binary_greater_greater);
    mark_object(vm, (PyroObject*)vm->str_op_unary_tilde);

    // Other object fields.
    mark_object(vm, (PyroObject*)vm->superglobals);
    mark_object(vm, (PyroObject*)vm->modules);
    mark_object(vm, (PyroObject*)vm->module_cache);
    mark_object(vm, (PyroObject*)vm->string_pool);
    mark_object(vm, (PyroObject*)vm->main_module);
    mark_object(vm, (PyroObject*)vm->import_roots);
    mark_object(vm, (PyroObject*)vm->stdout_file);
    mark_object(vm, (PyroObject*)vm->stderr_file);
    mark_object(vm, (PyroObject*)vm->stdin_file);
    mark_object(vm, (PyroObject*)vm->panic_buffer);
    mark_object(vm, (PyroObject*)vm->panic_source_id);

    // Each PyroCallFrame in the call stack has a pointer to a PyroClosure.
    for (size_t i = 0; i < vm->frame_count; i++) {
        mark_object(vm, (PyroObject*)vm->frames[i].closure);
    }

    // The VM's linked-list of open upvalues.
    for (PyroUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object(vm, (PyroObject*)upvalue);
    }

    // Values on the 'with' stack with pending $end_with() method calls.
    for (size_t i = 0; i < vm->with_stack_count; i++) {
        mark_value(vm, vm->with_stack[i]);
    }
}


static void blacken_object(PyroVM* vm, PyroObject* object) {
    mark_object(vm, (PyroObject*)object->class);

    switch (object->type) {
        case PYRO_OBJECT_BOUND_METHOD: {
            PyroBoundMethod* bound = (PyroBoundMethod*)object;
            mark_value(vm, bound->receiver);
            mark_object(vm, (PyroObject*)bound->method);
            break;
        }

        case PYRO_OBJECT_BUF:
            break;

        case PYRO_OBJECT_CLASS: {
            // We don't need to mark the cached method names or values as they're in the maps.
            PyroClass* class = (PyroClass*)object;
            mark_object(vm, (PyroObject*)class->name);
            mark_object(vm, (PyroObject*)class->superclass);
            mark_object(vm, (PyroObject*)class->all_instance_methods);
            mark_object(vm, (PyroObject*)class->pub_instance_methods);
            mark_object(vm, (PyroObject*)class->all_field_indexes);
            mark_object(vm, (PyroObject*)class->pub_field_indexes);
            mark_object(vm, (PyroObject*)class->default_field_values);
            mark_object(vm, (PyroObject*)class->static_methods);
            mark_object(vm, (PyroObject*)class->static_fields);
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroClosure* closure = (PyroClosure*)object;
            mark_object(vm, (PyroObject*)closure->fn);
            mark_object(vm, (PyroObject*)closure->module);
            mark_object(vm, (PyroObject*)closure->default_values);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                mark_object(vm, (PyroObject*)closure->upvalues[i]);
            }
            break;
        }

        case PYRO_OBJECT_FILE: {
            PyroFile* file = (PyroFile*)object;
            mark_object(vm, (PyroObject*)file->path);
            break;
        }

        case PYRO_OBJECT_FN: {
            PyroFn* fn = (PyroFn*)object;
            mark_object(vm, (PyroObject*)fn->name);
            mark_object(vm, (PyroObject*)fn->source_id);
            for (size_t i = 0; i < fn->constants_count; i++) {
                mark_value(vm, fn->constants[i]);
            }
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroInstance* instance = (PyroInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            for (int i = 0; i < num_fields; i++) {
                mark_value(vm, instance->fields[i]);
            }
            break;
        }

        case PYRO_OBJECT_ITER: {
            PyroIter* iter = (PyroIter*)object;
            mark_object(vm, (PyroObject*)iter->source);
            mark_object(vm, (PyroObject*)iter->callback);
            break;
        }

        case PYRO_OBJECT_MAP: {
            PyroMap* map = (PyroMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                PyroMapEntry* entry = &map->entry_array[i];
                if (PYRO_IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
                mark_value(vm, entry->value);
            }
            break;
        }

        case PYRO_OBJECT_MAP_AS_SET: {
            PyroMap* map = (PyroMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                PyroMapEntry* entry = &map->entry_array[i];
                if (PYRO_IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
            }
            break;
        }

        case PYRO_OBJECT_MODULE: {
            PyroMod* module = (PyroMod*)object;
            mark_object(vm, (PyroObject*)module->submodules);
            mark_object(vm, (PyroObject*)module->members);
            mark_object(vm, (PyroObject*)module->all_member_indexes);
            mark_object(vm, (PyroObject*)module->pub_member_indexes);
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            PyroNativeFn* native = (PyroNativeFn*)object;
            mark_object(vm, (PyroObject*)native->name);
            break;
        }

        case PYRO_OBJECT_QUEUE: {
            PyroQueue* queue = (PyroQueue*)object;
            PyroQueueItem* next_item = queue->head;
            while (next_item) {
                mark_value(vm, next_item->value);
                next_item = next_item->next;
            }
            break;
        }

        case PYRO_OBJECT_STR:
            break;

        case PYRO_OBJECT_TUP: {
            PyroTup* tup = (PyroTup*)object;
            for (size_t i = 0; i < tup->count; i++) {
                mark_value(vm, tup->values[i]);
            }
            break;
        }

        case PYRO_OBJECT_UPVALUE: {
            PyroUpvalue* upvalue = (PyroUpvalue*)object;
            mark_value(vm, upvalue->closed);
            break;
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            PyroVec* vec = (PyroVec*)object;
            for (size_t i = 0; i < vec->count; i++) {
                mark_value(vm, vec->values[i]);
            }
            break;
        }

        case PYRO_OBJECT_MAP_AS_WEAKREF:
            break;

        case PYRO_OBJECT_ERR: {
            PyroErr* err = (PyroErr*)object;
            mark_object(vm, (PyroObject*)err->message);
            mark_object(vm, (PyroObject*)err->details);
            break;
        }

        case PYRO_OBJECT_RESOURCE_POINTER:
            break;
    }
}


static void trace_references(PyroVM* vm) {
    while (vm->grey_stack_count > 0) {
        PyroObject* object = vm->grey_stack[--vm->grey_stack_count];
        blacken_object(vm, object);
        if (vm->panic_flag) {
            return;
        }
    }
}


static void sweep(PyroVM* vm) {
    PyroObject* previous = NULL;
    PyroObject* object = vm->objects;

    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            PyroObject* not_marked = object;
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
    PyroObject* obj = vm->objects;
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
}
