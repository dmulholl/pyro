#include "heap.h"
#include "vm.h"
#include "compiler.h"


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
            free_object(vm, not_marked);
        }
    }
}


// Mark every root object as reachable. (A root object is an object the VM can access directly
// without going through another object.)
static void mark_roots(PyroVM* vm) {
    // Mark local variables and temporary values on the stack.
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        mark_value(vm, *slot);
    }

    // Classes for builtin types.
    mark_object(vm, (Obj*)vm->map_class);
    mark_object(vm, (Obj*)vm->str_class);
    mark_object(vm, (Obj*)vm->tup_class);
    mark_object(vm, (Obj*)vm->vec_class);
    mark_object(vm, (Obj*)vm->tup_iter_class);
    mark_object(vm, (Obj*)vm->vec_iter_class);
    mark_object(vm, (Obj*)vm->map_iter_class);
    mark_object(vm, (Obj*)vm->str_iter_class);

    // The pool of interned strings. This map is a collection of *weak* references.
    mark_object(vm, (Obj*)vm->strings);

    // The VM's pool of canned objects.
    mark_object(vm, (Obj*)vm->empty_tuple);
    mark_object(vm, (Obj*)vm->empty_string);
    mark_object(vm, (Obj*)vm->str_init);
    mark_object(vm, (Obj*)vm->str_str);
    mark_object(vm, (Obj*)vm->str_true);
    mark_object(vm, (Obj*)vm->str_false);
    mark_object(vm, (Obj*)vm->str_null);
    mark_object(vm, (Obj*)vm->str_fmt);
    mark_object(vm, (Obj*)vm->str_iter);
    mark_object(vm, (Obj*)vm->str_next);
    mark_object(vm, (Obj*)vm->str_get_index);
    mark_object(vm, (Obj*)vm->str_set_index);

    // The map of global variables.
    mark_object(vm, (Obj*)vm->globals);

    // The tree of imported modules.
    mark_object(vm, (Obj*)vm->modules);

    mark_object(vm, (Obj*)vm->main_module);
    mark_object(vm, (Obj*)vm->import_dirs);

    // Each CallFrame in the call stack has a pointer to an ObjClosure.
    for (int i = 0; i < vm->frame_count; i++) {
        mark_object(vm, (Obj*)vm->frames[i].closure);
    }

    // The VM's linked-list of open upvalues.
    for (ObjUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object(vm, (Obj*)upvalue);
    }

    // If we're in the middle of compiling, mark any objects directly accessible by the compiler.
    pyro_mark_compiler_roots(vm);
}


static void blacken_object(PyroVM* vm, Obj* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "   %p blacken %s\n", (void*)object, pyro_stringify_obj_type(object->type));
    #endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*)object;
            mark_value(vm, bound->receiver);
            mark_object(vm, (Obj*)bound->method);
            break;
        }

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            mark_object(vm, (Obj*)class->name);
            mark_object(vm, (Obj*)class->methods);
            mark_object(vm, (Obj*)class->field_initializers);
            mark_object(vm, (Obj*)class->field_indexes);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            mark_object(vm, (Obj*)closure->fn);
            for (int i = 0; i < closure->upvalue_count; i++) {
                mark_object(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FN: {
            ObjFn* fn = (ObjFn*)object;
            mark_object(vm, (Obj*)fn->name);
            mark_object(vm, (Obj*)fn->source);
            mark_object(vm, (Obj*)fn->module);
            for (size_t i = 0; i < fn->constants_count; i++) {
                mark_value(vm, fn->constants[i]);
            }
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            mark_object(vm, (Obj*)instance->obj.class);
            int num_fields = instance->obj.class->field_initializers->count;
            for (int i = 0; i < num_fields; i++) {
                mark_value(vm, instance->fields[i]);
            }
            break;
        }

        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            for (size_t i = 0; i < map->capacity; i++) {
                MapEntry* entry = &map->entries[i];
                if (IS_EMPTY(entry->key) || IS_TOMBSTONE(entry->key)) {
                    continue;
                }
                mark_value(vm, entry->key);
                mark_value(vm, entry->value);
            }
            break;
        }

        case OBJ_MAP_ITER: {
            ObjMapIter* iter = (ObjMapIter*)object;
            mark_object(vm, (Obj*)iter->map);
            break;
        }

        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            mark_object(vm, (Obj*)module->globals);
            mark_object(vm, (Obj*)module->submodules);
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            mark_object(vm, (Obj*)native->name);
            break;
        }

        case OBJ_STR:
            break;

        case OBJ_STR_ITER: {
            ObjStrIter* iter = (ObjStrIter*)object;
            mark_object(vm, (Obj*)iter->string);
            break;
        }

        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            for (size_t i = 0; i < tup->count; i++) {
                mark_value(vm, tup->values[i]);
            }
            break;
        }

        case OBJ_TUP_ITER: {
            ObjTupIter* iter = (ObjTupIter*)object;
            mark_object(vm, (Obj*)iter->tup);
            break;
        }

        case OBJ_UPVALUE:
            mark_value(vm, ((ObjUpvalue*)object)->closed);
            break;

        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            for (size_t i = 0; i < vec->count; i++) {
                mark_value(vm, vec->values[i]);
            }
            break;
        }

        case OBJ_VEC_ITER: {
            ObjVecIter* iter = (ObjVecIter*)object;
            mark_object(vm, (Obj*)iter->vec);
            break;
        }

        case OBJ_WEAKREF_MAP:
            // This map type holds weak references to its keys and values.
            break;
    }
}


static void trace_references(PyroVM* vm) {
    while (vm->grey_count > 0) {
        Obj* object = vm->grey_stack[--vm->grey_count];
        blacken_object(vm, object);
    }
}


void* reallocate(PyroVM* vm, void* pointer, size_t old_size, size_t new_size) {
    vm->bytes_allocated -= old_size;
    vm->bytes_allocated += new_size;

    if (new_size > old_size) {
        #ifdef PYRO_DEBUG_STRESS_GC
            collect_garbage(vm);
        #endif

        if (vm->bytes_allocated > vm->next_gc_threshold) {
            collect_garbage(vm);
        }
    }

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    return realloc(pointer, new_size);
}


void free_object(PyroVM* vm, Obj* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "   %p free object %s\n", (void*)object, pyro_stringify_obj_type(object->type));
    #endif

    #define FREE_OBJECT(vm, type, pointer) \
        reallocate(vm, pointer, sizeof(type), 0)

    switch(object->type) {
        case OBJ_BOUND_METHOD: {
            FREE_OBJECT(vm, ObjBoundMethod, object);
            break;
        }

        case OBJ_CLASS: {
            FREE_OBJECT(vm, ObjClass, object);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE_OBJECT(vm, ObjClosure, object);
            break;
        }

        case OBJ_FN: {
            ObjFn* fn = (ObjFn*)object;
            FREE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity);
            FREE_ARRAY(vm, Value, fn->constants, fn->constants_capacity);
            FREE_ARRAY(vm, uint16_t, fn->bpl, fn->bpl_capacity);
            FREE_OBJECT(vm, ObjFn, object);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            int num_fields = instance->obj.class->field_initializers->count;
            reallocate(vm, instance, sizeof(ObjInstance) + num_fields * sizeof(Value), 0);
            break;
        }

        case OBJ_MAP:
        case OBJ_WEAKREF_MAP: {
            ObjMap* map = (ObjMap*)object;
            FREE_ARRAY(vm, MapEntry, map->entries, map->capacity);
            FREE_OBJECT(vm, ObjMap, object);
            break;
        }

        case OBJ_MAP_ITER: {
            FREE_OBJECT(vm, ObjMapIter, object);
            break;
        }

        case OBJ_MODULE: {
            FREE_OBJECT(vm, ObjModule, object);
            break;
        }

        case OBJ_NATIVE_FN: {
            FREE_OBJECT(vm, ObjNativeFn, object);
            break;
        }

        case OBJ_STR: {
            ObjStr* string = (ObjStr*)object;
            ObjMap_remove(vm->strings, OBJ_VAL(string));
            FREE_ARRAY(vm, char, string->bytes, string->length + 1);
            FREE_OBJECT(vm, ObjStr, object);
            break;
        }

        case OBJ_STR_ITER: {
            FREE_OBJECT(vm, ObjStrIter, object);
            break;
        }

        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            reallocate(vm, tup, sizeof(ObjTup) + tup->count * sizeof(Value), 0);
            break;
        }

        case OBJ_TUP_ITER: {
            FREE_OBJECT(vm, ObjTupIter, object);
            break;
        }

        case OBJ_UPVALUE: {
            FREE_OBJECT(vm, ObjUpvalue, object);
            break;
        }

        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            FREE_ARRAY(vm, Value, vec->values, vec->capacity);
            FREE_OBJECT(vm, ObjVec, object);
            break;
        }

        case OBJ_VEC_ITER: {
            FREE_OBJECT(vm, ObjVecIter, object);
            break;
        }
    }

    #undef FREE_OBJECT
}


// Mark an object as reachable.
void mark_object(PyroVM* vm, Obj* object) {
    if (object == NULL || object->is_marked) {
        return;
    }

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "   %p mark %s\n", (void*)object, pyro_stringify_obj_type(object->type));
    #endif

    object->is_marked = true;

    // Add the object to the worklist of grey objects.
    if (vm->grey_count == vm->grey_capacity) {
        size_t new_capacity = GROW_CAPACITY(vm->grey_capacity);
        void* new_array = realloc(vm->grey_stack, sizeof(Obj*) * new_capacity);

        if (new_array == NULL) {
            pyro_err(vm, "Error: Out of memory.\n");
            vm->exit_flag = true;
            vm->exit_code = 127;
            return;
        }

        vm->grey_capacity = new_capacity;
        vm->grey_stack = new_array;
    }

    vm->grey_stack[vm->grey_count++] = object;
}


// Mark a value as reachable.
void mark_value(PyroVM* vm, Value value) {
    if (IS_OBJ(value) || IS_ERR(value)) {
        mark_object(vm, AS_OBJ(value));
    }
}


void collect_garbage(PyroVM* vm) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "-- gc begin\n");
        size_t before = vm->bytes_allocated;
    #endif

        mark_roots(vm);
        trace_references(vm);
        sweep(vm);

        vm->next_gc_threshold = vm->bytes_allocated * PYRO_GC_HEAP_GROW_FACTOR;

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "-- gc end\n");
        pyro_out(vm, "-- gc collected %zu bytes (from %zu to %zu) next gc at %zu\n",
            before - vm->bytes_allocated,
            before,
            vm->bytes_allocated,
            vm->next_gc_threshold
        );
    #endif
}
