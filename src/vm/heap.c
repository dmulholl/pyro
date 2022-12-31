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
            FREE_OBJECT(vm, ObjBoundMethod, object);
            break;
        }

        case PYRO_OBJECT_BUF: {
            ObjBuf* buf = (ObjBuf*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, buf->bytes, buf->capacity);
            FREE_OBJECT(vm, ObjBuf, object);
            break;
        }

        case PYRO_OBJECT_CLASS: {
            FREE_OBJECT(vm, ObjClass, object);
            break;
        }

        case PYRO_OBJECT_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            PYRO_FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE_OBJECT(vm, ObjClosure, object);
            break;
        }

        case PYRO_OBJECT_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (file->stream) {
                if (file->stream == stdin) {
                   // Nothing to do.
                } else if (file->stream == stdout || file->stream == stderr) {
                    fflush(file->stream);
                } else {
                    fclose(file->stream);
                }
            }
            FREE_OBJECT(vm, ObjFile, object);
            break;
        }

        case PYRO_OBJECT_PYRO_FN: {
            ObjPyroFn* fn = (ObjPyroFn*)object;
            PYRO_FREE_ARRAY(vm, uint8_t, fn->code, fn->code_capacity);
            PYRO_FREE_ARRAY(vm, PyroValue, fn->constants, fn->constants_capacity);
            PYRO_FREE_ARRAY(vm, uint16_t, fn->bpl, fn->bpl_capacity);
            FREE_OBJECT(vm, ObjPyroFn, object);
            break;
        }

        case PYRO_OBJECT_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            int num_fields = instance->obj.class->default_field_values->count;
            pyro_realloc(vm, instance, sizeof(ObjInstance) + num_fields * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_ITER: {
            FREE_OBJECT(vm, ObjIter, object);
            break;
        }

        case PYRO_OBJECT_MAP_AS_WEAKREF:
        case PYRO_OBJECT_MAP_AS_SET:
        case PYRO_OBJECT_MAP: {
            ObjMap* map = (ObjMap*)object;
            PYRO_FREE_ARRAY(vm, PyroMapEntry, map->entry_array, map->entry_array_capacity);
            PYRO_FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
            FREE_OBJECT(vm, ObjMap, object);
            break;
        }

        case PYRO_OBJECT_MODULE: {
            FREE_OBJECT(vm, ObjModule, object);
            break;
        }

        case PYRO_OBJECT_NATIVE_FN: {
            FREE_OBJECT(vm, ObjNativeFn, object);
            break;
        }

        case PYRO_OBJECT_QUEUE: {
            ObjQueue* queue = (ObjQueue*)object;
            QueueItem* next_item = queue->head;
            while (next_item) {
                QueueItem* current_item = next_item;
                next_item = current_item->next;
                pyro_realloc(vm, current_item, sizeof(QueueItem), 0);
            }
            FREE_OBJECT(vm, ObjQueue, object);
            break;
        }

        case PYRO_OBJECT_RESOURCE_POINTER: {
            ObjResourcePointer* resource = (ObjResourcePointer*)object;
            if (resource->callback) {
                resource->callback(vm, resource->pointer);
            }
            FREE_OBJECT(vm, ObjResourcePointer, object);
            break;
        }

        case PYRO_OBJECT_STR: {
            ObjStr* string = (ObjStr*)object;
            ObjMap_remove(vm->strings, pyro_obj(string), vm);
            PYRO_FREE_ARRAY(vm, char, string->bytes, string->length + 1);
            FREE_OBJECT(vm, ObjStr, object);
            break;
        }

        case PYRO_OBJECT_TUP: {
            ObjTup* tup = (ObjTup*)object;
            pyro_realloc(vm, tup, sizeof(ObjTup) + tup->count * sizeof(PyroValue), 0);
            break;
        }

        case PYRO_OBJECT_UPVALUE: {
            FREE_OBJECT(vm, ObjUpvalue, object);
            break;
        }

        case PYRO_OBJECT_VEC_AS_STACK:
        case PYRO_OBJECT_VEC: {
            ObjVec* vec = (ObjVec*)object;
            PYRO_FREE_ARRAY(vm, PyroValue, vec->values, vec->capacity);
            FREE_OBJECT(vm, ObjVec, object);
            break;
        }

        case PYRO_OBJECT_ERR: {
            FREE_OBJECT(vm, ObjErr, object);
            break;
        }
    }

    #undef FREE_OBJECT
}
