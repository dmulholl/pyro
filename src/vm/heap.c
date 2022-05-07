#include "heap.h"
#include "vm.h"
#include "stringify.h"


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


void pyro_free_object(PyroVM* vm, Obj* object) {
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
            int num_fields = instance->obj.class->field_values->count;
            pyro_realloc(vm, instance, sizeof(ObjInstance) + num_fields * sizeof(Value), 0);
            break;
        }

        case OBJ_ITER: {
            FREE_OBJECT(vm, ObjIter, object);
            break;
        }

        case OBJ_MAP_AS_WEAKREF:
        case OBJ_MAP_AS_SET:
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            FREE_ARRAY(vm, MapEntry, map->entry_array, map->entry_array_capacity);
            FREE_ARRAY(vm, int64_t, map->index_array, map->index_array_capacity);
            FREE_OBJECT(vm, ObjMap, object);
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

        case OBJ_QUEUE: {
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

        case OBJ_RESOURCE_POINTER: {
            ObjResourcePointer* resource = (ObjResourcePointer*)object;
            if (resource->callback) {
                resource->callback(vm, resource->pointer);
            }
            FREE_OBJECT(vm, ObjResourcePointer, object);
            break;
        }

        case OBJ_STR: {
            ObjStr* string = (ObjStr*)object;
            ObjMap_remove(vm->strings, MAKE_OBJ(string), vm);
            FREE_ARRAY(vm, char, string->bytes, string->length + 1);
            FREE_OBJECT(vm, ObjStr, object);
            break;
        }

        case OBJ_TUP_AS_ERR:
        case OBJ_TUP: {
            ObjTup* tup = (ObjTup*)object;
            pyro_realloc(vm, tup, sizeof(ObjTup) + tup->count * sizeof(Value), 0);
            break;
        }

        case OBJ_UPVALUE: {
            FREE_OBJECT(vm, ObjUpvalue, object);
            break;
        }

        case OBJ_VEC_AS_STACK:
        case OBJ_VEC: {
            ObjVec* vec = (ObjVec*)object;
            FREE_ARRAY(vm, Value, vec->values, vec->capacity);
            FREE_OBJECT(vm, ObjVec, object);
            break;
        }
    }

    #undef FREE_OBJECT
}
