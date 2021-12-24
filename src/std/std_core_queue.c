#include "std_core.h"

#include "../vm/values.h"
#include "../vm/vm.h"
#include "../vm/utils.h"
#include "../vm/heap.h"
#include "../vm/utf8.h"
#include "../vm/setup.h"
#include "../vm/panics.h"


static Value fn_queue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = ObjQueue_new(vm);
    if (!queue) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    return OBJ_VAL(queue);
}


static Value fn_is_queue(PyroVM* vm, size_t arg_count, Value* args) {
    return BOOL_VAL(IS_QUEUE(args[0]));
}


static Value queue_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return I64_VAL(queue->count);
}


static Value queue_enqueue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    if (!ObjQueue_enqueue(queue, args[0], vm)) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
    }
    return NULL_VAL();
}


static Value queue_dequeue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    Value value;
    if (!ObjQueue_dequeue(queue, &value, vm)) {
        pyro_panic(vm, ERR_VALUE_ERROR, "Cannot dequeue from empty queue.");
        return NULL_VAL();
    }
    return value;
}


static Value queue_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return BOOL_VAL(queue->count == 0);
}


static Value queue_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)queue, ITER_QUEUE, vm);
    if (!iter) {
        pyro_panic(vm, ERR_OUT_OF_MEMORY, "Out of memory.");
        return NULL_VAL();
    }
    iter->next_queue_item = queue->head;

    return OBJ_VAL(iter);
}


void pyro_load_std_core_queue(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$queue", fn_queue, 0);
    pyro_define_global_fn(vm, "$is_queue", fn_is_queue, 1);

    // Methods.
    pyro_define_method(vm, vm->queue_class, "count", queue_count, 0);
    pyro_define_method(vm, vm->queue_class, "enqueue", queue_enqueue, 1);
    pyro_define_method(vm, vm->queue_class, "dequeue", queue_dequeue, 0);
    pyro_define_method(vm, vm->queue_class, "is_empty", queue_is_empty, 0);
    pyro_define_method(vm, vm->queue_class, "$iter", queue_iter, 0);
}
