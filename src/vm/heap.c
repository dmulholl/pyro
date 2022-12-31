#include "../inc/heap.h"
#include "../inc/vm.h"
#include "../inc/stringify.h"


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


void pyro_free_object(PyroVM* vm, PyroObj* object) {
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
            FREE_OBJECT(vm, PyroObjBoundMethod, object);
            break;
        }

        case PYRO_OBJECT_BUF: {
            PyroObjBuf* buf = (PyroObjBuf*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity);
            FREE_OBJECT(vm, PyroObjBuf, object);
            break;
        }

        case PYRO_OBJECT_CLASS: {
            FREE_OBJECT(vm, PyroObjClass, object);
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            PyroObjClosure* closure = (PyroObjClosure*)object;
            PYRO_FREE_ARRAY(vm, PyroObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE_OBJECT(vm, PyroObjClosure, object);
            break;
        }

        case PYRO_OBJECT_FILE: {
            PyroObjFile* file = (PyroObjFile*)object;
            if (file->stream) {
                if (file->stream == stdin) {
                   // Nothing to do.
                } else if (file->stream == stdout || file->stream == stderr) {
                    fflush(file->stream);
                } else {
                    fclose(file->stream);
                }
            }
            FREE_OBJECT(vm, PyroObjFile, object);
            break;
        }

        case PYRO_OBJECT_PYRO_FN: {
            PyroObjPyroFn* fn = (PyroObjPyroFn*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity);
            PYRO_FREE_ARRAY(vm, PyroValue, fn->constants, fn->constants_capacity);
            PYRO_FREE_ARRAY(vm, uint16_t, fn->bpl, fn->bpl_capacity);
            FREE_OBJECT(vm, PyroObjPyroFn, object);
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            PyroObjInstance* instance = (PyroObjInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            pyro_realloc(vm, instance, sizeof(PyroObjInstance) + num_fields * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_ITER: {
            FREE_OBJECT(vm, PyroObjIter, object);
            break;
        }

        case PYRO_OBJECT_MAP_AS_WEAKREF:
        case PYRO_OBJECT_MAP_AS_SET:
        case PYRO_OBJECT_MAP: {
            PyroObjMap* map = (PyroObjMap*)object;
            PYRO_FREE_ARRAY(vm, PyroMapEntry, map->entry_array, map->entry_array_capacity);
            PYRO_FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
            FREE_OBJECT(vm, PyroObjMap, object);
            break;
        }

        case PYRO_OBJECT_MODULE: {
            FREE_OBJECT(vm, PyroObjModule, object);
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            FREE_OBJECT(vm, PyroObjNativeFn, object);
            break;
        }

        case PYRO_OBJECT_QUEUE: {
            PyroObjQueue* queue = (PyroObjQueue*)object;
            QueueItem* next_item = queue->head;
            while (next_item) {
                QueueItem* current_item = next_item;
                next_item = current_item->next;
                pyro_realloc(vm, current_item, sizeof(QueueItem), 0);
            }
            FREE_OBJECT(vm, PyroObjQueue, object);
            break;
        }

        case PYRO_OBJECT_RESOURCE_POINTER: {
            PyroObjResourcePointer* resource = (PyroObjResourcePointer*)object;
            if (resource->callback) {
                resource->callback(vm, resource->pointer);
            }
            FREE_OBJECT(vm, PyroObjResourcePointer, object);
            break;
        }

        case PYRO_OBJECT_STR: {
            PyroObjStr* string = (PyroObjStr*)object;
            PyroObjMap_remove(vm->strings, pyro_obj(string), vm);
            PYRO_FREE_ARRAY(vm, char, string->bytes, string->length + 1);
            FREE_OBJECT(vm, PyroObjStr, object);
            break;
        }

        case PYRO_OBJECT_TUP: {
            PyroObjTup* tup = (PyroObjTup*)object;
            pyro_realloc(vm, tup, sizeof(PyroObjTup) + tup->count * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_UPVALUE: {
            FREE_OBJECT(vm, PyroObjUpvalue, object);
            break;
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            PyroObjVec* vec = (PyroObjVec*)object;
            PYRO_FREE_ARRAY(vm, PyroValue, vec->values, vec->capacity);
            FREE_OBJECT(vm, PyroObjVec, object);
            break;
        }

        case PYRO_OBJECT_ERR: {
            FREE_OBJECT(vm, PyroObjErr, object);
            break;
        }
    }

    #undef FREE_OBJECT
}
