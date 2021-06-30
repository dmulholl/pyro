#ifndef pyro_heap_h
#define pyro_heap_h

#include "common.h"
#include "values.h"


// Increase the capacity of an array. Capacity will always be a power of 2.
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)


// Heap-allocate memory for a new [type] array with space for [capacity] elements.
#define ALLOCATE_ARRAY(vm, type, capacity) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (capacity))


// Resize an existing heap-allocated array. Equivalent to ALLOCATE_ARRAY() if [pointer = NULL]
// and [old_capacity = 0].
#define REALLOCATE_ARRAY(vm, type, pointer, old_capacity, new_capacity) \
    (type*)reallocate(vm, pointer, sizeof(type) * (old_capacity), sizeof(type) * (new_capacity))


// Free an array allocated using ALLOCATE_ARRAY() or REALLOCATE_ARRAY().
#define FREE_ARRAY(vm, type, pointer, capacity) \
    reallocate(vm, pointer, sizeof(type) * (capacity), 0)


// This function is a wrapper around the C standard library's `realloc` and `free`; it adds
// bookkeeping and triggers for the garbage collector. All the VM's dynamic memory management is
// funneled through this single function: allocating, reallocating, and freeing.
// [old_size] and [new_size] are both in bytes. To allocate a new block of memory
// use [pointer = NULL] and [old_size = 0]. To free a block of memory set [new_size = 0].
void* reallocate(PyroVM* vm, void* pointer, size_t old_size, size_t new_size);

void free_object(PyroVM* vm, Obj* object);
void collect_garbage(PyroVM* vm);
void mark_value(PyroVM* vm, Value value);
void mark_object(PyroVM* vm, Obj* object);

#endif
