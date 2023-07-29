#ifndef pyro_string_pool_h
#define pyro_string_pool_h

// A linear-probing hash set.
typedef struct {
    // The number of live entries in the pool. (Does not include tombstones.)
    size_t live_entry_count;

    // [entry_array_count] includes both live entries and tombstones.
    PyroStr** entry_array;
    size_t entry_array_count;
    size_t entry_array_capacity;

    // This gets recalculated every time [entry_array] is resized using the formula:
    //
    //   [max_load_threshold] = [entry_array_capacity] * PYRO_MAX_HASHMAP_LOAD
    //
    // Invariant: [entry_array_count] <= [max_load_threshold].
    size_t max_load_threshold;
} PyroStrPool;

void PyroStrPool_init(PyroStrPool* pool);
bool PyroStrPool_add(PyroStrPool* pool, PyroStr* string, PyroVM* vm);
void PyroStrPool_remove(PyroStrPool* pool, PyroStr* string);

// If the pool contains an entry identical to [string], returns it, otherwise NULL.
PyroStr* PyroStrPool_contains(PyroStrPool* pool, const char* string, size_t count, uint64_t hash);

// Frees the memory owned by the pool, not the [pool] object itself.
void PyroStrPool_free(PyroStrPool* pool, PyroVM* vm);

#endif
