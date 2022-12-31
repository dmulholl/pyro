#ifndef pyro_heap_h
#define pyro_heap_h

#include "pyro.h"
#include "values.h"
#include "objects.h"

// Increase the capacity of an array.
#define PYRO_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

// Heap-allocate memory for a new array of [type] with space for [capacity] elements.
#define ALLOCATE_ARRAY(vm, type, capacity) \
    (type*)pyro_realloc(vm, NULL, 0, sizeof(type) * (capacity))

// Resize an existing heap-allocated array. Equivalent to ALLOCATE_ARRAY() if [pointer = NULL]
// and [old_capacity = 0].
#define REALLOCATE_ARRAY(vm, type, pointer, old_capacity, new_capacity) \
    (type*)pyro_realloc(vm, pointer, sizeof(type) * (old_capacity), sizeof(type) * (new_capacity))

// Free an array allocated using ALLOCATE_ARRAY() or REALLOCATE_ARRAY().
#define FREE_ARRAY(vm, type, pointer, capacity) \
    pyro_realloc(vm, pointer, sizeof(type) * (capacity), 0)

// This function is a wrapper around the C standard library's realloc() and free() functions, adding
// bookkeeping for the VM. All the VM's dynamic memory management is funneled through this single
// function: allocating, reallocating, and freeing. [old_size] and [new_size] are both in bytes. To
// allocate a new block of memory use [pointer = NULL] and [old_size = 0]. To free a block of memory
// set [new_size = 0]. Allocation/reallocation can fail if sufficient memory is not available; in
// this case the function returns NULL. If reallocation fails, the input pointer remains valid, i.e.
// the memory it points to will not have been freed.
void* pyro_realloc(PyroVM* vm, void* pointer, size_t old_size, size_t new_size);

// This frees the object along with any heap-allocated memory it owns. This function should only
// be called from two places -- (1) from inside the garbage collector, and (2) from inside the
// pyro_free_vm() function.
void pyro_free_object(PyroVM* vm, Obj* object);

#endif
