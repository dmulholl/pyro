#include "../../../inc/pyro.h"


static PyroValue fn_queue(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PyroQueue_new(vm);
    if (!queue) {
        pyro_panic(vm, "$queue(): out of memory");
        return pyro_null();
    }
    return pyro_obj(queue);
}


static PyroValue fn_is_queue(PyroVM* vm, size_t arg_count, PyroValue* args) {
    return pyro_bool(PYRO_IS_QUEUE(args[0]));
}


static PyroValue queue_count(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);
    return pyro_i64(queue->count);
}


static PyroValue queue_enqueue(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);
    if (!PyroQueue_enqueue(queue, args[0], vm)) {
        pyro_panic(vm, "enqueue(): out of memory");
    }
    return pyro_null();
}


static PyroValue queue_dequeue(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);
    PyroValue value;
    if (!PyroQueue_dequeue(queue, &value, vm)) {
        pyro_panic(vm, "dequeue(): cannot dequeue from empty queue");
        return pyro_null();
    }
    return value;
}


static PyroValue queue_is_empty(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);
    return pyro_bool(queue->count == 0);
}


static PyroValue queue_iter(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);

    PyroIter* iter = PyroIter_new((PyroObject*)queue, PYRO_ITER_QUEUE, vm);
    if (!iter) {
        pyro_panic(vm, "iter(): out of memory");
        return pyro_null();
    }
    iter->next_queue_item = queue->head;

    return pyro_obj(iter);
}


static PyroValue queue_clear(PyroVM* vm, size_t arg_count, PyroValue* args) {
    PyroQueue* queue = PYRO_AS_QUEUE(args[-1]);
    PyroQueue_clear(queue, vm);
    return pyro_null();
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
