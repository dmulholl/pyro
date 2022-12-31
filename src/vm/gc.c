#include "../inc/heap.h"
#include "../inc/vm.h"
#include "../inc/stringify.h"
#include "../inc/panics.h"
#include "../inc/values.h"
#include "../inc/objects.h"


// Marks an object as reachable. This sets the object's [is_marked] flag and pushes it onto the
// grey stack. This function will set the panic flag BUT NOT call pyro_panic() if an attempt to
// allocate memory for the grey stack fails.
static void mark_object(PyroVM* vm, PyroObj* object) {
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
        size_t new_capacity = PYRO_GROW_CAPACITY(vm->grey_stack_capacity);
        PyroObj** new_array = PYRO_REALLOCATE_ARRAY(vm, PyroObj*, vm->grey_stack, vm->grey_stack_capacity, new_capacity);
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
    if (IS_OBJ(value)) {
        mark_object(vm, AS_OBJ(value));
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
    mark_object(vm, (PyroObj*)vm->class_map);
    mark_object(vm, (PyroObj*)vm->class_str);
    mark_object(vm, (PyroObj*)vm->class_tup);
    mark_object(vm, (PyroObj*)vm->class_vec);
    mark_object(vm, (PyroObj*)vm->class_buf);
    mark_object(vm, (PyroObj*)vm->class_file);
    mark_object(vm, (PyroObj*)vm->class_iter);
    mark_object(vm, (PyroObj*)vm->class_stack);
    mark_object(vm, (PyroObj*)vm->class_set);
    mark_object(vm, (PyroObj*)vm->class_queue);
    mark_object(vm, (PyroObj*)vm->class_err);
    mark_object(vm, (PyroObj*)vm->class_module);
    mark_object(vm, (PyroObj*)vm->class_char);

    // The VM's pool of canned objects.
    mark_object(vm, (PyroObj*)vm->error);
    mark_object(vm, (PyroObj*)vm->empty_string);
    mark_object(vm, (PyroObj*)vm->str_dollar_init);
    mark_object(vm, (PyroObj*)vm->str_dollar_str);
    mark_object(vm, (PyroObj*)vm->str_true);
    mark_object(vm, (PyroObj*)vm->str_false);
    mark_object(vm, (PyroObj*)vm->str_null);
    mark_object(vm, (PyroObj*)vm->str_dollar_fmt);
    mark_object(vm, (PyroObj*)vm->str_dollar_iter);
    mark_object(vm, (PyroObj*)vm->str_dollar_next);
    mark_object(vm, (PyroObj*)vm->str_dollar_get_index);
    mark_object(vm, (PyroObj*)vm->str_dollar_set_index);
    mark_object(vm, (PyroObj*)vm->str_dollar_debug);
    mark_object(vm, (PyroObj*)vm->str_op_binary_equals_equals);
    mark_object(vm, (PyroObj*)vm->str_op_binary_less);
    mark_object(vm, (PyroObj*)vm->str_op_binary_less_equals);
    mark_object(vm, (PyroObj*)vm->str_op_binary_greater);
    mark_object(vm, (PyroObj*)vm->str_op_binary_greater_equals);
    mark_object(vm, (PyroObj*)vm->str_op_binary_plus);
    mark_object(vm, (PyroObj*)vm->str_op_binary_minus);
    mark_object(vm, (PyroObj*)vm->str_op_binary_bar);
    mark_object(vm, (PyroObj*)vm->str_op_binary_amp);
    mark_object(vm, (PyroObj*)vm->str_op_binary_star);
    mark_object(vm, (PyroObj*)vm->str_op_binary_slash);
    mark_object(vm, (PyroObj*)vm->str_op_binary_caret);
    mark_object(vm, (PyroObj*)vm->str_op_binary_percent);
    mark_object(vm, (PyroObj*)vm->str_op_binary_star_star);
    mark_object(vm, (PyroObj*)vm->str_op_binary_slash_slash);
    mark_object(vm, (PyroObj*)vm->str_op_unary_plus);
    mark_object(vm, (PyroObj*)vm->str_op_unary_minus);
    mark_object(vm, (PyroObj*)vm->str_dollar_hash);
    mark_object(vm, (PyroObj*)vm->str_dollar_call);
    mark_object(vm, (PyroObj*)vm->str_dollar_contains);
    mark_object(vm, (PyroObj*)vm->str_bool);
    mark_object(vm, (PyroObj*)vm->str_i64);
    mark_object(vm, (PyroObj*)vm->str_f64);
    mark_object(vm, (PyroObj*)vm->str_char);
    mark_object(vm, (PyroObj*)vm->str_method);
    mark_object(vm, (PyroObj*)vm->str_buf);
    mark_object(vm, (PyroObj*)vm->str_class);
    mark_object(vm, (PyroObj*)vm->str_fn);
    mark_object(vm, (PyroObj*)vm->str_instance);
    mark_object(vm, (PyroObj*)vm->str_file);
    mark_object(vm, (PyroObj*)vm->str_iter);
    mark_object(vm, (PyroObj*)vm->str_map);
    mark_object(vm, (PyroObj*)vm->str_set);
    mark_object(vm, (PyroObj*)vm->str_vec);
    mark_object(vm, (PyroObj*)vm->str_stack);
    mark_object(vm, (PyroObj*)vm->str_queue);
    mark_object(vm, (PyroObj*)vm->str_str);
    mark_object(vm, (PyroObj*)vm->str_module);
    mark_object(vm, (PyroObj*)vm->str_tup);
    mark_object(vm, (PyroObj*)vm->str_err);
    mark_object(vm, (PyroObj*)vm->str_dollar_end_with);
    mark_object(vm, (PyroObj*)vm->str_source);
    mark_object(vm, (PyroObj*)vm->str_line);

    // Other object fields.
    mark_object(vm, (PyroObj*)vm->superglobals);
    mark_object(vm, (PyroObj*)vm->modules);
    mark_object(vm, (PyroObj*)vm->strings);
    mark_object(vm, (PyroObj*)vm->main_module);
    mark_object(vm, (PyroObj*)vm->import_roots);
    mark_object(vm, (PyroObj*)vm->stdout_file);
    mark_object(vm, (PyroObj*)vm->stderr_file);
    mark_object(vm, (PyroObj*)vm->stdin_file);
    mark_object(vm, (PyroObj*)vm->panic_buffer);
    mark_object(vm, (PyroObj*)vm->panic_source_id);

    // Each CallFrame in the call stack has a pointer to an PyroObjClosure.
    for (size_t i = 0; i < vm->frame_count; i++) {
        mark_object(vm, (PyroObj*)vm->frames[i].closure);
    }

    // The VM's linked-list of open upvalues.
    for (PyroObjUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object(vm, (PyroObj*)upvalue);
    }

    // Values on the 'with' stack with pending $end_with() method calls.
    for (size_t i = 0; i < vm->with_stack_count; i++) {
        mark_value(vm, vm->with_stack[i]);
    }
}


static void blacken_object(PyroVM* vm, PyroObj* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(
            vm,
            "   %p blacken %s\n",
            (void*)object,
            pyro_stringify_object_type(object->type)
        );
    #endif

    mark_object(vm, (PyroObj*)object->class);

    switch (object->type) {
        case PYRO_OBJECT_BOUND_METHOD: {
            PyroObjBoundMethod* bound = (PyroObjBoundMethod*)object;
            mark_value(vm, bound->receiver);
            mark_object(vm, (PyroObj*)bound->method);
            break;
        }

        case PYRO_OBJECT_BUF:
            break;

        case PYRO_OBJECT_CLASS: {
            // We don't need to mark the cached method names or values as they're in the maps.
            PyroObjClass* class = (PyroObjClass*)object;
            mark_object(vm, (PyroObj*)class->name);
            mark_object(vm, (PyroObj*)class->superclass);
            mark_object(vm, (PyroObj*)class->all_instance_methods);
            mark_object(vm, (PyroObj*)class->pub_instance_methods);
            mark_object(vm, (PyroObj*)class->all_field_indexes);
            mark_object(vm, (PyroObj*)class->pub_field_indexes);
            mark_object(vm, (PyroObj*)class->default_field_values);
            mark_object(vm, (PyroObj*)class->static_methods);
            mark_object(vm, (PyroObj*)class->static_fields);
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroObjClosure* closure = (PyroObjClosure*)object;
            mark_object(vm, (PyroObj*)closure->fn);
            mark_object(vm, (PyroObj*)closure->module);
            mark_object(vm, (PyroObj*)closure->default_values);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                mark_object(vm, (PyroObj*)closure->upvalues[i]);
            }
            break;
        }

        case PYRO_OBJECT_FILE: {
            PyroObjFile* file = (PyroObjFile*)object;
            mark_object(vm, (PyroObj*)file->path);
            break;
        }

        case PYRO_OBJECT_PYRO_FN: {
            PyroObjPyroFn* fn = (PyroObjPyroFn*)object;
            mark_object(vm, (PyroObj*)fn->name);
            mark_object(vm, (PyroObj*)fn->source_id);
            for (size_t i = 0; i < fn->constants_count; i++) {
                mark_value(vm, fn->constants[i]);
            }
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroObjInstance* instance = (PyroObjInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            for (int i = 0; i < num_fields; i++) {
                mark_value(vm, instance->fields[i]);
            }
            break;
        }

        case PYRO_OBJECT_ITER: {
            PyroObjIter* iter = (PyroObjIter*)object;
            mark_object(vm, (PyroObj*)iter->source);
            mark_object(vm, (PyroObj*)iter->callback);
            break;
        }

        case PYRO_OBJECT_MAP: {
            PyroObjMap* map = (PyroObjMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                PyroMapEntry* entry = &map->entry_array[i];
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
                mark_value(vm, entry->value);
            }
            break;
        }

        case PYRO_OBJECT_MAP_AS_SET: {
            PyroObjMap* map = (PyroObjMap*)object;
            for (size_t i = 0; i < map->entry_array_count; i++) {
                PyroMapEntry* entry = &map->entry_array[i];
                if (IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
            }
            break;
        }

        case PYRO_OBJECT_MODULE: {
            PyroObjModule* module = (PyroObjModule*)object;
            mark_object(vm, (PyroObj*)module->submodules);
            mark_object(vm, (PyroObj*)module->members);
            mark_object(vm, (PyroObj*)module->all_member_indexes);
            mark_object(vm, (PyroObj*)module->pub_member_indexes);
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            PyroObjNativeFn* native = (PyroObjNativeFn*)object;
            mark_object(vm, (PyroObj*)native->name);
            break;
        }

        case PYRO_OBJECT_QUEUE: {
            PyroObjQueue* queue = (PyroObjQueue*)object;
            QueueItem* next_item = queue->head;
            while (next_item) {
                mark_value(vm, next_item->value);
                next_item = next_item->next;
            }
            break;
        }

        case PYRO_OBJECT_STR:
            break;

        case PYRO_OBJECT_TUP: {
            PyroObjTup* tup = (PyroObjTup*)object;
            for (size_t i = 0; i < tup->count; i++) {
                mark_value(vm, tup->values[i]);
            }
            break;
        }

        case PYRO_OBJECT_UPVALUE: {
            PyroObjUpvalue* upvalue = (PyroObjUpvalue*)object;
            mark_value(vm, upvalue->closed);
            break;
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            PyroObjVec* vec = (PyroObjVec*)object;
            for (size_t i = 0; i < vec->count; i++) {
                mark_value(vm, vec->values[i]);
            }
            break;
        }

        case PYRO_OBJECT_MAP_AS_WEAKREF:
            break;

        case PYRO_OBJECT_ERR: {
            PyroObjErr* err = (PyroObjErr*)object;
            mark_object(vm, (PyroObj*)err->message);
            mark_object(vm, (PyroObj*)err->details);
            break;
        }

        case PYRO_OBJECT_RESOURCE_POINTER:
            break;
    }
}


static void trace_references(PyroVM* vm) {
    while (vm->grey_stack_count > 0) {
        PyroObj* object = vm->grey_stack[--vm->grey_stack_count];
        blacken_object(vm, object);
        if (vm->panic_flag) {
            return;
        }
    }
}


static void sweep(PyroVM* vm) {
    PyroObj* previous = NULL;
    PyroObj* object = vm->objects;

    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            PyroObj* not_marked = object;
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
    PyroObj* obj = vm->objects;
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
