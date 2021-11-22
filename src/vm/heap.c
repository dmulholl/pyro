#include "heap.h"
#include "vm.h"
#include "compiler.h"
#include "errors.h"


// Mark every root object as reachable. (A root object is an object the VM can access directly
// without going through another object.)
static void mark_roots(PyroVM* vm) {
    // Local variables and temporary values on the stack.
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
        pyro_mark_value(vm, *slot);
    }

    // Classes for builtin types.
    pyro_mark_object(vm, (Obj*)vm->map_class);
    pyro_mark_object(vm, (Obj*)vm->str_class);
    pyro_mark_object(vm, (Obj*)vm->tup_class);
    pyro_mark_object(vm, (Obj*)vm->vec_class);
    pyro_mark_object(vm, (Obj*)vm->buf_class);
    pyro_mark_object(vm, (Obj*)vm->tup_iter_class);
    pyro_mark_object(vm, (Obj*)vm->vec_iter_class);
    pyro_mark_object(vm, (Obj*)vm->map_iter_class);
    pyro_mark_object(vm, (Obj*)vm->str_iter_class);
    pyro_mark_object(vm, (Obj*)vm->range_class);
    pyro_mark_object(vm, (Obj*)vm->file_class);

    // The VM's pool of canned objects.
    pyro_mark_object(vm, (Obj*)vm->empty_error);
    pyro_mark_object(vm, (Obj*)vm->empty_string);
    pyro_mark_object(vm, (Obj*)vm->str_init);
    pyro_mark_object(vm, (Obj*)vm->str_str);
    pyro_mark_object(vm, (Obj*)vm->str_true);
    pyro_mark_object(vm, (Obj*)vm->str_false);
    pyro_mark_object(vm, (Obj*)vm->str_null);
    pyro_mark_object(vm, (Obj*)vm->str_fmt);
    pyro_mark_object(vm, (Obj*)vm->str_iter);
    pyro_mark_object(vm, (Obj*)vm->str_next);
    pyro_mark_object(vm, (Obj*)vm->str_get_index);
    pyro_mark_object(vm, (Obj*)vm->str_set_index);
    pyro_mark_object(vm, (Obj*)vm->str_debug);

    // Other object fields.
    pyro_mark_object(vm, (Obj*)vm->globals);
    pyro_mark_object(vm, (Obj*)vm->modules);
    pyro_mark_object(vm, (Obj*)vm->strings);
    pyro_mark_object(vm, (Obj*)vm->main_module);
    pyro_mark_object(vm, (Obj*)vm->import_roots);

    // Each CallFrame in the call stack has a pointer to an ObjClosure.
    for (size_t i = 0; i < vm->frame_count; i++) {
        pyro_mark_object(vm, (Obj*)vm->frames[i].closure);
    }

    // The VM's linked-list of open upvalues.
    for (ObjUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        pyro_mark_object(vm, (Obj*)upvalue);
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
            pyro_mark_value(vm, bound->receiver);
            pyro_mark_object(vm, (Obj*)bound->method);
            break;
        }

        case OBJ_BUF:
            break;

        case OBJ_CLASS: {
            ObjClass* class = (ObjClass*)object;
            pyro_mark_object(vm, (Obj*)class->name);
            pyro_mark_object(vm, (Obj*)class->methods);
            pyro_mark_object(vm, (Obj*)class->field_initializers);
            pyro_mark_object(vm, (Obj*)class->field_indexes);
            pyro_mark_object(vm, (Obj*)class->superclass);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            pyro_mark_object(vm, (Obj*)closure->fn);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                pyro_mark_object(vm, (Obj*)closure->upvalues[i]);
            }
            break;
        }

        case OBJ_FILE:
            break;

        case OBJ_FN: {
            ObjFn* fn = (ObjFn*)object;
            pyro_mark_object(vm, (Obj*)fn->name);
            pyro_mark_object(vm, (Obj*)fn->source);
            pyro_mark_object(vm, (Obj*)fn->module);
            for (size_t i = 0; i < fn->constants_count; i++) {
                pyro_mark_value(vm, fn->constants[i]);
            }
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            pyro_mark_object(vm, (Obj*)instance->obj.class);
            int num_fields = instance->obj.class->field_initializers->count;
            for (int i = 0; i < num_fields; i++) {
                pyro_mark_value(vm, instance->fields[i]);
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
                pyro_mark_value(vm, entry->key);
                pyro_mark_value(vm, entry->value);
            }
            break;
        }

        case OBJ_MAP_ITER: {
            ObjMapIter* iter = (ObjMapIter*)object;
            pyro_mark_object(vm, (Obj*)iter->map);
            break;
        }

        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            pyro_mark_object(vm, (Obj*)module->globals);
            pyro_mark_object(vm, (Obj*)module->submodules);
            break;
        }

        case OBJ_NATIVE_FN: {
            ObjNativeFn* native = (ObjNativeFn*)object;
            pyro_mark_object(vm, (Obj*)native->name);
            break;
        }

        case OBJ_RANGE:
            break;

        case OBJ_STR:
            break;

        case OBJ_STR_ITER: {
            ObjStrIter* iter = (ObjStrIter*)object;
            pyro_mark_object(vm, (Obj*)iter->string);
            break;
        }

        case OBJ_ERR:
        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            for (size_t i = 0; i < tup->count; i++) {
                pyro_mark_value(vm, tup->values[i]);
            }
            break;
        }

        case OBJ_TUP_ITER: {
            ObjTupIter* iter = (ObjTupIter*)object;
            pyro_mark_object(vm, (Obj*)iter->tup);
            break;
        }

        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*)object;
            pyro_mark_value(vm, upvalue->closed);
            break;
        }

        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            for (size_t i = 0; i < vec->count; i++) {
                pyro_mark_value(vm, vec->values[i]);
            }
            break;
        }

        case OBJ_VEC_ITER: {
            ObjVecIter* iter = (ObjVecIter*)object;
            pyro_mark_object(vm, (Obj*)iter->vec);
            break;
        }

        case OBJ_WEAKREF_MAP:
            break;
    }
}


static void trace_references(PyroVM* vm) {
    while (vm->grey_count > 0) {
        Obj* object = vm->grey_stack[--vm->grey_count];
        blacken_object(vm, object);
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


// ---------------- //
// Public Interface //
// ---------------- //


void* pyro_realloc(PyroVM* vm, void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        vm->bytes_allocated -= old_size;
        return NULL;
    }

    #ifdef PYRO_DEBUG_STRESS_GC
        pyro_collect_garbage(vm);
    #else
        if (vm->bytes_allocated > vm->next_gc_threshold) {
            pyro_collect_garbage(vm);
        }
    #endif

    size_t new_total_allocation = vm->bytes_allocated - old_size + new_size;
    if (new_total_allocation > vm->max_bytes) {
        vm->memory_allocation_failed = true;
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result) {
        vm->bytes_allocated = new_total_allocation;
        return result;
    }

    vm->memory_allocation_failed = true;
    return NULL;
}


void pyro_free_object(PyroVM* vm, Obj* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "   %p free object %s\n", (void*)object, pyro_stringify_obj_type(object->type));
    #endif

    #define FREE_OBJECT(vm, type, pointer) \
        pyro_realloc(vm, pointer, sizeof(type), 0)

    switch(object->type) {
        case OBJ_BOUND_METHOD: {
            FREE_OBJECT(vm, ObjBoundMethod, object);
            break;
        }

        case OBJ_BUF: {
            ObjBuf* buf = (ObjBuf*)object;
            FREE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity);
            FREE_OBJECT(vm, ObjBuf, object);
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

        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (file->stream) {
                if (file->stream != stdout && file->stream != stdin && file->stream != stderr) {
                    fclose(file->stream);
                    file->stream = NULL;
                }
            }
            FREE_OBJECT(vm, ObjFile, object);
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
            pyro_realloc(vm, instance, sizeof(ObjInstance) + num_fields * sizeof(Value), 0);
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

        case OBJ_RANGE: {
            FREE_OBJECT(vm, ObjRange, object);
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

        case OBJ_ERR:
        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            pyro_realloc(vm, tup, sizeof(ObjTup) + tup->count * sizeof(Value), 0);
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


void pyro_mark_object(PyroVM* vm, Obj* object) {
    if (object == NULL || object->is_marked || vm->panic_flag) {
        return;
    }

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "   %p mark %s\n", (void*)object, pyro_stringify_obj_type(object->type));
    #endif

    if (vm->grey_count == vm->grey_capacity) {
        size_t new_capacity = GROW_CAPACITY(vm->grey_capacity);
        void* new_array = realloc(vm->grey_stack, sizeof(Obj*) * new_capacity);

        if (!new_array) {
            vm->hard_panic = true;
            vm->memory_allocation_failed = true;
            pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory, grey stack allocation failed.");
            return;
        }

        vm->bytes_allocated -= vm->grey_capacity;
        vm->bytes_allocated += new_capacity;

        vm->grey_capacity = new_capacity;
        vm->grey_stack = new_array;
    }

    object->is_marked = true;
    vm->grey_stack[vm->grey_count++] = object;
}


void pyro_mark_value(PyroVM* vm, Value value) {
    if (IS_OBJ(value)) {
        pyro_mark_object(vm, AS_OBJ(value));
    }
}


void pyro_collect_garbage(PyroVM* vm) {
    assert(!vm->panic_flag);
    if (vm->panic_flag) {
        return;
    }

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "-- gc begin\n");
        size_t initial_bytes_allocated = vm->bytes_allocated;
    #endif

    mark_roots(vm);
    if (vm->panic_flag) {
        return;
    }

    trace_references(vm);
    if (vm->panic_flag) {
        return;
    }

    sweep(vm);
    vm->next_gc_threshold = vm->bytes_allocated * PYRO_GC_HEAP_GROW_FACTOR;

    #ifdef PYRO_DEBUG_LOG_GC
        pyro_out(vm, "-- gc end\n");
        pyro_out(vm, "-- gc collected %zu bytes (from %zu to %zu) next gc at %zu\n",
            initial_bytes_allocated - vm->bytes_allocated,
            initial_bytes_allocated,
            vm->bytes_allocated,
            vm->next_gc_threshold
        );
    #endif
}
