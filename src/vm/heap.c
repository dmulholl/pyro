#include "../inc/pyro.h"


void* pyro_realloc(PyroVM* vm, void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        vm->bytes_allocated -= old_size;
        return NULL;
    }

    size_t new_total_allocation = (vm->bytes_allocated - old_size) + new_size;
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


void pyro_free_object(PyroVM* vm, PyroObject* object) {
    #ifdef PYRO_DEBUG_LOG_GC
        pyro_write_stdout(
            vm,
            "   %p free object %s\n",
            (void*)object,
            pyro_stringify_object_type(object->type)
        );
    #endif

    #define FREE_OBJECT(vm, type, pointer) \
        pyro_realloc(vm, pointer, sizeof(type), 0)

    switch(object->type) {
        case PYRO_OBJECT_BOUND_METHOD: {
            FREE_OBJECT(vm, PyroBoundMethod, object);
            break;
        }

        case PYRO_OBJECT_BUF: {
            PyroBuf* buf = (PyroBuf*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity);
            FREE_OBJECT(vm, PyroBuf, object);
            break;
        }

        case PYRO_OBJECT_CLASS: {
            FREE_OBJECT(vm, PyroClass, object);
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroClosure* closure = (PyroClosure*)object;
            PYRO_FREE_ARRAY(vm, PyroUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE_OBJECT(vm, PyroClosure, object);
            break;
        }

        case PYRO_OBJECT_FILE: {
            PyroFile* file = (PyroFile*)object;
            if (file->stream) {
                if (file->stream == stdin) {
                   // Nothing to do.
                } else if (file->stream == stdout || file->stream == stderr) {
                    fflush(file->stream);
                } else {
                    fclose(file->stream);
                }
            }
            FREE_OBJECT(vm, PyroFile, object);
            break;
        }

        case PYRO_OBJECT_FN: {
            PyroFn* fn = (PyroFn*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity);
            PYRO_FREE_ARRAY(vm, PyroValue, fn->constants, fn->constants_capacity);
            PYRO_FREE_ARRAY(vm, uint16_t, fn->bpl, fn->bpl_capacity);
            FREE_OBJECT(vm, PyroFn, object);
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroInstance* instance = (PyroInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            pyro_realloc(vm, instance, sizeof(PyroInstance) + num_fields * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_ITER: {
            FREE_OBJECT(vm, PyroIter, object);
            break;
        }

        case PYRO_OBJECT_MAP_AS_WEAKREF:
        case PYRO_OBJECT_MAP_AS_SET:
        case PYRO_OBJECT_MAP: {
            PyroMap* map = (PyroMap*)object;
            PYRO_FREE_ARRAY(vm, PyroMapEntry, map->entry_array, map->entry_array_capacity);
            PYRO_FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
            FREE_OBJECT(vm, PyroMap, object);
            break;
        }

        case PYRO_OBJECT_MODULE: {
            FREE_OBJECT(vm, PyroMod, object);
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            FREE_OBJECT(vm, PyroNativeFn, object);
            break;
        }

        case PYRO_OBJECT_QUEUE: {
            PyroQueue* queue = (PyroQueue*)object;
            PyroQueueItem* next_item = queue->head;
            while (next_item) {
                PyroQueueItem* current_item = next_item;
                next_item = current_item->next;
                pyro_realloc(vm, current_item, sizeof(PyroQueueItem), 0);
            }
            FREE_OBJECT(vm, PyroQueue, object);
            break;
        }

        case PYRO_OBJECT_RESOURCE_POINTER: {
            PyroResourcePointer* resource = (PyroResourcePointer*)object;
            if (resource->callback) {
                resource->callback(vm, resource->pointer);
            }
            FREE_OBJECT(vm, PyroResourcePointer, object);
            break;
        }

        case PYRO_OBJECT_STR: {
            PyroStr* string = (PyroStr*)object;
            PyroMap_remove(vm->strings, pyro_obj(string), vm);
            PYRO_FREE_ARRAY(vm, char, string->bytes, string->length + 1);
            FREE_OBJECT(vm, PyroStr, object);
            break;
        }

        case PYRO_OBJECT_TUP: {
            PyroTup* tup = (PyroTup*)object;
            pyro_realloc(vm, tup, sizeof(PyroTup) + tup->count * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_UPVALUE: {
            FREE_OBJECT(vm, PyroUpvalue, object);
            break;
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            PyroVec* vec = (PyroVec*)object;
            PYRO_FREE_ARRAY(vm, PyroValue, vec->values, vec->capacity);
            FREE_OBJECT(vm, PyroVec, object);
            break;
        }

        case PYRO_OBJECT_ERR: {
            FREE_OBJECT(vm, PyroErr, object);
            break;
        }
    }

    #undef FREE_OBJECT
}
