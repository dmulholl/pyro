#include "../std_lib.h"

#include "../../vm/values.h"
#include "../../vm/vm.h"
#include "../../vm/utils.h"
#include "../../vm/heap.h"
#include "../../vm/utf8.h"
#include "../../vm/setup.h"
#include "../../vm/panics.h"


static Value fn_queue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = ObjQueue_new(vm);
    if (!queue) {
        pyro_panic(vm, "$queue(): out of memory");
        return MAKE_NULL();
    }
    return MAKE_OBJ(queue);
}


static Value fn_is_queue(PyroVM* vm, size_t arg_count, Value* args) {
    return MAKE_BOOL(IS_QUEUE(args[0]));
}


static Value queue_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return MAKE_I64(queue->count);
}


static Value queue_enqueue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    if (!ObjQueue_enqueue(queue, args[0], vm)) {
        pyro_panic(vm, "enqueue(): out of memory");
    }
    return MAKE_NULL();
}


static Value queue_dequeue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    Value value;
    if (!ObjQueue_dequeue(queue, &value, vm)) {
        pyro_panic(vm, "dequeue(): cannot dequeue from empty queue");
        return MAKE_NULL();
    }
    return value;
}


static Value queue_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return MAKE_BOOL(queue->count == 0);
}


static Value queue_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)queue, ITER_QUEUE, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return MAKE_NULL();
    }
    iter->next_queue_item = queue->head;

    return MAKE_OBJ(iter);
}


void pyro_load_std_core_queue(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$queue", fn_queue, 0);
    pyro_define_global_fn(vm, "$is_queue", fn_is_queue, 1);

    // Methods.
    pyro_define_method(vm, vm->class_queue, "count", queue_count, 0);
    pyro_define_method(vm, vm->class_queue, "enqueue", queue_enqueue, 1);
    pyro_define_method(vm, vm->class_queue, "dequeue", queue_dequeue, 0);
    pyro_define_method(vm, vm->class_queue, "is_empty", queue_is_empty, 0);
    pyro_define_method(vm, vm->class_queue, "$iter", queue_iter, 0);
}
