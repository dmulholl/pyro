#ifndef pyro_heap_h
#define pyro_heap_h

// Heap-allocate memory for a new array of [type] with space for [capacity] elements.
// - [capacity] is specified as number of elements, not number of bytes.
#define PYRO_ALLOCATE_ARRAY(vm, type, capacity) \
    (type*)pyro_realloc(vm, NULL, 0, sizeof(type) * (capacity))

// Resize an existing heap-allocated array.
// - [old_capacity] and [new_capacity] are specified as number of elements, not number of bytes.
// - Equivalent to PYRO_ALLOCATE_ARRAY() if [pointer = NULL] and [old_capacity = 0].
#define PYRO_REALLOCATE_ARRAY(vm, type, pointer, old_capacity, new_capacity) \
    (type*)pyro_realloc(vm, pointer, sizeof(type) * (old_capacity), sizeof(type) * (new_capacity))

// Free an array allocated using PYRO_ALLOCATE_ARRAY() or PYRO_REALLOCATE_ARRAY().
// - [capacity] is specified as number of elements, not number of bytes.
#define PYRO_FREE_ARRAY(vm, type, pointer, capacity) \
    pyro_realloc(vm, pointer, sizeof(type) * (capacity), 0)

// Calculates a new target capacity for growing a dynamic array.
// NB: the new capacity is not guaranteed to be a power of 2.
static inline size_t pyro_grow_capacity(size_t old_capacity) {
    if (old_capacity < 8) {
        return 8;
    }

    if (old_capacity < 1024) {
        // [new_capacity = old_capacity * 2]
        return old_capacity << 1;
    }

    // [new_capacity = old_capacity * 1.5]
    return (old_capacity >> 1) * 3;
}

// This function is a wrapper around the C standard library's realloc() and free() functions,
// adding bookkeeping for the VM. All the VM's dynamic memory management is funneled through
// this single function: allocating, reallocating, and freeing. [old_size] and [new_size] are
// both in bytes. To allocate a new block of memory use [pointer = NULL] and [old_size = 0].
// To free a block of memory set [new_size = 0]. Allocation/reallocation can fail if
// sufficient memory is not available; in this case the function returns NULL. If reallocation
// fails, the input pointer remains valid, i.e. the memory it points to will not have been
// freed.
void* pyro_realloc(PyroVM* vm, void* pointer, size_t old_size, size_t new_size);

// This frees the object along with any heap-allocated memory it owns. This function should
// only be called from two places -- (1) from inside the garbage collector, and (2) from
// inside the pyro_free_vm() function.
void pyro_free_object(PyroVM* vm, PyroObject* object);

#endif
