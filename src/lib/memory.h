#ifndef pyro_memory_h
#define pyro_memory_h

#include "common.h"
#include "values.h"


#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)


// Allocate a block of memory for a new array of [type] with space for [capacity] elements.
#define ALLOCATE_ARRAY(vm, type, capacity) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (capacity))


// Resize an existing heap-allocated array.
// Equivalent to ALLOCATE_ARRAY() if [pointer] is NULL and [old_capacity] is 0.
#define REALLOCATE_ARRAY(vm, type, pointer, old_capacity, new_capacity) \
    (type*)reallocate(vm, pointer, sizeof(type) * (old_capacity), sizeof(type) * (new_capacity))


#define FREE_ARRAY(vm, type, pointer, capacity) \
    reallocate(vm, pointer, sizeof(type) * (capacity), 0)


// Allocate a block of memory for a single object.
#define ALLOCATE(vm, type) \
    (type*)reallocate(vm, NULL, 0, sizeof(type))


// Free the block of memory occupied by a single object.
#define FREE(vm, type, pointer) \
    reallocate(vm, pointer, sizeof(type), 0)


// All dynamic memory management is funneled through this function: allocating, reallocating,
// and freeing.
void* reallocate(PyroVM* vm, void* pointer, size_t old_size, size_t new_size);

void free_object(PyroVM* vm, Obj* object);
void collect_garbage(PyroVM* vm);
void mark_value(PyroVM* vm, Value value);
void mark_object(PyroVM* vm, Obj* object);

#endif
