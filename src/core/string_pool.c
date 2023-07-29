#include "../includes/pyro.h"


// We use the address of this variable as a tombstone entry in the pool.
static int pyro_string_pool_tombstone;


static PyroStr** find_entry_slot(PyroStr** entry_array, size_t entry_array_capacity, PyroStr* string) {
    assert(entry_array_capacity > 0);

    // Capacity is always a power of 2 so we can use bitwise-AND as a fast modulo operator,
    // i.e. this is equivalent to: i = hash % entry_array_capacity.
    size_t i = (size_t)string->hash & (entry_array_capacity - 1);

    PyroStr** first_tombstone = NULL;

    for (;;) {
        PyroStr** slot = &entry_array[i];

        if (*slot == NULL) {
            return first_tombstone == NULL ? slot : first_tombstone;
        } else if (*slot == (PyroStr*)&pyro_string_pool_tombstone) {
            if (first_tombstone == NULL) first_tombstone = slot;
        } else if (*slot == string) {
            return slot;
        }

        i = (i + 1) & (entry_array_capacity - 1);
    }
}


static bool resize_entry_array(PyroStrPool* pool, PyroVM* vm) {
    size_t new_entry_array_capacity = 8;
    if (pool->entry_array_capacity > 0) {
        new_entry_array_capacity = pool->entry_array_capacity * 2;
    }

    PyroStr** new_entry_array = PYRO_ALLOCATE_ARRAY(vm, PyroStr*, new_entry_array_capacity);
    if (!new_entry_array) {
        return false;
    }

    for (size_t i = 0; i < new_entry_array_capacity; i++) {
        new_entry_array[i] = NULL;
    }

    for (size_t i = 0; i < pool->entry_array_capacity; i++) {
        PyroStr* entry = pool->entry_array[i];

        if (entry == NULL || entry == (PyroStr*)&pyro_string_pool_tombstone) {
            continue;
        }

        PyroStr** slot = find_entry_slot(
            new_entry_array,
            new_entry_array_capacity,
            entry
        );
        *slot = entry;
    }

    PYRO_FREE_ARRAY(vm, PyroStr*, pool->entry_array, pool->entry_array_capacity);
    pool->entry_array = new_entry_array;
    pool->entry_array_count = pool->live_entry_count;
    pool->entry_array_capacity = new_entry_array_capacity;
    pool->max_load_threshold = new_entry_array_capacity * PYRO_MAX_HASHMAP_LOAD;

    return true;
}


void PyroStrPool_init(PyroStrPool* pool) {
    pool->entry_array = NULL;
    pool->entry_array_count = 0;
    pool->entry_array_capacity = 0;
    pool->live_entry_count = 0;
    pool->max_load_threshold = 0;
}


void PyroStrPool_free(PyroStrPool* pool, PyroVM* vm) {
    PYRO_FREE_ARRAY(vm, PyroStr*, pool->entry_array, pool->entry_array_capacity);
    pool->entry_array = NULL;
    pool->entry_array_count = 0;
    pool->entry_array_capacity = 0;
    pool->live_entry_count = 0;
    pool->max_load_threshold = 0;
}


bool PyroStrPool_add(PyroStrPool* pool, PyroStr* string, PyroVM* vm) {
    if (pool->entry_array_capacity == 0) {
        if (!resize_entry_array(pool, vm)) {
            return false;
        }
    }

    PyroStr** slot = find_entry_slot(pool->entry_array, pool->entry_array_capacity, string);

    // 1. The slot is empty.
    if (*slot == NULL) {
        if (pool->entry_array_count == pool->max_load_threshold) {
            if (!resize_entry_array(pool, vm)) {
                return false;
            }
            slot = find_entry_slot(pool->entry_array, pool->entry_array_capacity, string);
        }
        *slot = string;
        pool->live_entry_count++;
        pool->entry_array_count++;
        return true;
    }

    // 2. The slot contains a tombstone.
    if (*slot == (PyroStr*)&pyro_string_pool_tombstone) {
        *slot = string;
        pool->live_entry_count++;
        return true;
    }

    // 3. The slot already contains [string].
    assert(false);
    return true;
}


void PyroStrPool_remove(PyroStrPool* pool, PyroStr* string) {
    if (pool->live_entry_count == 0) {
        return;
    }

    PyroStr** slot = find_entry_slot(pool->entry_array, pool->entry_array_capacity, string);

    if (*slot == NULL || *slot == (PyroStr*)&pyro_string_pool_tombstone) {
        return;
    }

    *slot = (PyroStr*)&pyro_string_pool_tombstone;
    pool->live_entry_count--;
}


PyroStr* PyroStrPool_contains(PyroStrPool* pool, const char* string, size_t count, uint64_t hash) {
    if (pool->live_entry_count == 0) {
        return NULL;
    }
    assert(pool->entry_array_capacity > 0);

    // Capacity is always a power of 2 so we can use bitwise-AND as a fast modulo operator,
    // i.e. this is equivalent to: i = hash % entry_array_capacity.
    size_t i = (size_t)hash & (pool->entry_array_capacity - 1);

    for (;;) {
        PyroStr* entry = pool->entry_array[i];

        if (entry == NULL) {
            return NULL;
        } else if (entry == (PyroStr*)&pyro_string_pool_tombstone) {
            // Skip over the tombstone and keep looking for a matching entry.
        } else {
            if (entry->hash == hash) {
                if (entry->count == count) {
                    if (memcmp(entry->bytes, string, count) == 0) {
                        return entry;
                    }
                }
            }
        }

        i = (i + 1) & (pool->entry_array_capacity - 1);
    }
}
