#include "../../inc/std_lib.h"
#include "../../inc/values.h"
#include "../../inc/vm.h"
#include "../../inc/utils.h"
#include "../../inc/heap.h"
#include "../../inc/utf8.h"
#include "../../inc/setup.h"
#include "../../inc/panics.h"


static Value fn_queue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = ObjQueue_new(vm);
    if (!queue) {
        pyro_panic(vm, "$queue(): out of memory");
        return pyro_make_null();
    }
    return pyro_make_obj(queue);
}


static Value fn_is_queue(PyroVM* vm, size_t arg_count, Value* args) {
    return pyro_make_bool(IS_QUEUE(args[0]));
}


static Value queue_count(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return pyro_make_i64(queue->count);
}


static Value queue_enqueue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    if (!ObjQueue_enqueue(queue, args[0], vm)) {
        pyro_panic(vm, "enqueue(): out of memory");
    }
    return pyro_make_null();
}


static Value queue_dequeue(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    Value value;
    if (!ObjQueue_dequeue(queue, &value, vm)) {
        pyro_panic(vm, "dequeue(): cannot dequeue from empty queue");
        return pyro_make_null();
    }
    return value;
}


static Value queue_is_empty(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    return pyro_make_bool(queue->count == 0);
}


static Value queue_iter(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);

    ObjIter* iter = ObjIter_new((Obj*)queue, ITER_QUEUE, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_make_null();
    }
    iter->next_queue_item = queue->head;

    return pyro_make_obj(iter);
}


static Value queue_clear(PyroVM* vm, size_t arg_count, Value* args) {
    ObjQueue* queue = AS_QUEUE(args[-1]);
    ObjQueue_clear(queue, vm);
    return pyro_make_null();
}


void pyro_load_std_core_queue(PyroVM* vm) {
    // Functions.
    pyro_define_global_fn(vm, "$queue", fn_queue, 0);
    pyro_define_global_fn(vm, "$is_queue", fn_is_queue, 1);

    // Methods -- private.
    pyro_define_pri_method(vm, vm->class_queue, "$iter", queue_iter, 0);

    // Methods -- public.
    pyro_define_pub_method(vm, vm->class_queue, "count", queue_count, 0);
    pyro_define_pub_method(vm, vm->class_queue, "enqueue", queue_enqueue, 1);
    pyro_define_pub_method(vm, vm->class_queue, "dequeue", queue_dequeue, 0);
    pyro_define_pub_method(vm, vm->class_queue, "is_empty", queue_is_empty, 0);
    pyro_define_pub_method(vm, vm->class_queue, "clear", queue_clear, 0);
}
