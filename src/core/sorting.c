#include "../includes/pyro.h"


static void swap(PyroValue* a, PyroValue* b) {
    PyroValue temp = *a;
    *a = *b;
    *b = temp;
}


/* ----------- */
/*  Shuffling  */
/* ----------- */


// Fisher-Yates/Durstenfeld shuffling: we iterate over the array and at each index choose
// randomly from the remaining unshuffled entries, i.e. for each i we choose j from the
// interval [i, count).
void pyro_shuffle(PyroVM* vm, PyroValue* values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        size_t j = i + pyro_xoshiro256ss_next_in_range(&vm->prng_state, count - i);
        swap(&values[i], &values[j]);
    }
}


/* ---------------- */
/*  Insertion Sort  */
/* ---------------- */


// Sorts the slice array[low..high] where the indices are inclusive.
static void insertion_sort_slice(PyroVM* vm, PyroValue* array, size_t low, size_t high) {
    for (size_t i = low + 1; i <= high; i++) {
        for (size_t j = i; j > low; j--) {
            bool item_j_is_less_than_previous = pyro_op_compare_lt(vm, array[j], array[j - 1]);
            if (vm->halt_flag) {
                return;
            }
            if (item_j_is_less_than_previous) {
                swap(&array[j], &array[j - 1]);
            } else {
                break;
            }
        }
    }
}


// Sorts the slice array[low..high] where the indices are inclusive.
static void insertion_sort_slice_with_cb(PyroVM* vm, PyroValue* array, size_t low, size_t high, PyroValue callback) {
    for (size_t i = low + 1; i <= high; i++) {
        for (size_t j = i; j > low; j--) {
            if (!pyro_push(vm, callback)) return;
            if (!pyro_push(vm, array[j])) return;
            if (!pyro_push(vm, array[j - 1])) return;
            PyroValue item_j_is_less_than_previous = pyro_call_function(vm, 2);

            if (vm->halt_flag) {
                return;
            }

            if (!PYRO_IS_BOOL(item_j_is_less_than_previous)) {
                pyro_panic(vm, "comparison function must return a boolean");
                return;
            }

            if (item_j_is_less_than_previous.as.boolean) {
                swap(&array[j], &array[j - 1]);
            } else {
                break;
            }
        }
    }
}


/* ----------- */
/*  Quicksort  */
/* ----------- */


// The partitioning routine uses the last element as the pivot. It returns the sorted (i.e.
// final) index of the pivot value. After partitioning, no element to the left of the pivot is
// greater than it, no element to the right of the pviot is less than it.


// Partitions the slice array[low..high] where the indices are inclusive.
static size_t partition_slice_with_cb(PyroVM* vm, PyroValue* array, size_t low, size_t high, PyroValue callback) {
    PyroValue pivot = array[high];
    size_t i = low;

    for (size_t j = low; j < high; j++) {
        if (!pyro_push(vm, callback)) return 0;
        if (!pyro_push(vm, array[j])) return 0;
        if (!pyro_push(vm, pivot)) return 0;
        PyroValue item_j_is_less_than_pivot = pyro_call_function(vm, 2);

        if (vm->halt_flag) {
            return 0;
        }

        if (!PYRO_IS_BOOL(item_j_is_less_than_pivot)) {
            pyro_panic(vm, "comparison function must return a boolean");
            return 0;
        }

        if (item_j_is_less_than_pivot.as.boolean) {
            swap(&array[i], &array[j]);
            i++;
        }
    }

    swap(&array[i], &array[high]);
    return i;
}


// Partitions the slice array[low..high] where the indices are inclusive.
static size_t partition_slice(PyroVM* vm, PyroValue* array, size_t low, size_t high) {
    PyroValue pivot = array[high];
    size_t i = low;

    for (size_t j = low; j < high; j++) {
        bool item_j_is_less_than_pivot = pyro_op_compare_lt(vm, array[j], pivot);
        if (vm->halt_flag) {
            return 0;
        }

        if (item_j_is_less_than_pivot) {
            swap(&array[i], &array[j]);
            i++;
        }
    }

    swap(&array[i], &array[high]);
    return i;
}


// Sorts the slice array[low..high] where the indices are inclusive.
static void quicksort_slice_with_cb(PyroVM* vm, PyroValue* array, size_t low, size_t high, PyroValue callback) {
    if (vm->halt_flag) {
        return;
    }

    // Use insertion sort for small slices.
    if (high <= low + 10) {
        insertion_sort_slice_with_cb(vm, array, low, high, callback);
        return;
    }

    // After partitioning, the element at index p is at its final location.
    size_t p = partition_slice_with_cb(vm, array, low, high, callback);
    if (vm->halt_flag) {
        return;
    }

    // Sort the elements before the partition index.
    if (p > 1) {
        quicksort_slice_with_cb(vm, array, low, p - 1, callback);
        if (vm->halt_flag) {
            return;
        }
    }

    // Sort the elements after the partition index.
    quicksort_slice_with_cb(vm, array, p + 1, high, callback);
}


// Sorts the slice array[low..high] where the indices are inclusive.
static void quicksort_slice(PyroVM* vm, PyroValue* array, size_t low, size_t high) {
    if (vm->halt_flag) {
        return;
    }

    // Use insertion sort for small slices.
    if (high <= low + 10) {
        insertion_sort_slice(vm, array, low, high);
        return;
    }

    // After partitioning, the element at index p is at its final location.
    size_t p = partition_slice(vm, array, low, high);
    if (vm->halt_flag) {
        return;
    }

    // Sort the elements before the partition index.
    if (p > 1) {
        quicksort_slice(vm, array, low, p - 1);
        if (vm->halt_flag) {
            return;
        }
    }

    // Sort the elements after the partition index.
    quicksort_slice(vm, array, p + 1, high);
}


void pyro_quicksort_with_callback(PyroVM* vm, PyroValue* values, size_t count, PyroValue callback) {
    if (count > 1) {
        pyro_shuffle(vm, values, count);
        quicksort_slice_with_cb(vm, values, 0, count - 1, callback);
    }
}


void pyro_quicksort(PyroVM* vm, PyroValue* values, size_t count) {
    if (count > 1) {
        pyro_shuffle(vm, values, count);
        quicksort_slice(vm, values, 0, count - 1);
    }
}


/* ----------- */
/*  Mergesort  */
/* ----------- */


// Merges the sorted slices array[low..mid] and array[mid+1..high]. Indices are inclusive.
static void merge(PyroVM* vm, PyroValue* array, PyroValue* aux_array, size_t low, size_t mid, size_t high) {
    if (vm->halt_flag) {
        return;
    }

    size_t i = low;
    size_t j = mid + 1;

    // Copy the slice from [array] to [aux_array].
    memcpy(&aux_array[low], &array[low], sizeof(PyroValue) * (high - low + 1));

    for (size_t k = low; k <= high; k++) {
        if (i > mid) {
            array[k] = aux_array[j];
            j++;
            continue;
        }

        if (j > high) {
            array[k] = aux_array[i];
            i++;
            continue;
        }

        bool item_j_is_less_than_item_i = pyro_op_compare_lt(vm, aux_array[j], aux_array[i]);
        if (vm->halt_flag) {
            return;
        }

        if (item_j_is_less_than_item_i) {
            array[k] = aux_array[j];
            j++;
        } else {
            array[k] = aux_array[i];
            i++;
        }
    }
}


// Merges the sorted slices array[low..mid] and array[mid+1..high]. Indices are inclusive.
static void merge_with_cb(
    PyroVM* vm,
    PyroValue* array,
    PyroValue* aux_array,
    size_t low,
    size_t mid,
    size_t high,
    PyroValue callback
) {
    if (vm->halt_flag) {
        return;
    }

    size_t i = low;
    size_t j = mid + 1;

    // Copy the slice from [array] to [aux_array].
    memcpy(&aux_array[low], &array[low], sizeof(PyroValue) * (high - low + 1));

    for (size_t k = low; k <= high; k++) {
        if (i > mid) {
            array[k] = aux_array[j];
            j++;
            continue;
        }

        if (j > high) {
            array[k] = aux_array[i];
            i++;
            continue;
        }

        if (!pyro_push(vm, callback)) return;
        if (!pyro_push(vm, aux_array[j])) return;
        if (!pyro_push(vm, aux_array[i])) return;
        PyroValue item_j_is_less_than_item_i = pyro_call_function(vm, 2);

        if (vm->halt_flag) {
            return;
        }

        if (!PYRO_IS_BOOL(item_j_is_less_than_item_i)) {
            pyro_panic(vm, "comparison function must return a boolean");
            return;
        }

        if (item_j_is_less_than_item_i.as.boolean) {
            array[k] = aux_array[j];
            j++;
        } else {
            array[k] = aux_array[i];
            i++;
        }
    }
}


// Sorts the slice array[low..high] where the indices are inclusive.
static void mergesort_slice_with_cb(
    PyroVM* vm,
    PyroValue* array,
    PyroValue* aux_array,
    size_t low,
    size_t high,
    PyroValue callback
) {
    // Use insertion sort for small slices.
    if (high <= low + 10) {
        insertion_sort_slice_with_cb(vm, array, low, high, callback);
        return;
    }

    // A classic bug here is to use (low + high) / 2, ignoring the possibility of overflow.
    size_t mid = low + (high - low) / 2;

    mergesort_slice_with_cb(vm, array, aux_array, low, mid, callback);
    if (vm->halt_flag) {
        return;
    }

    mergesort_slice_with_cb(vm, array, aux_array, mid + 1, high, callback);
    if (vm->halt_flag) {
        return;
    }

    merge_with_cb(vm, array, aux_array, low, mid, high, callback);
}


// Sorts the slice array[low..high] where the indices are inclusive.
static void mergesort_slice(PyroVM* vm, PyroValue* array, PyroValue* aux_array, size_t low, size_t high) {
    // Use insertion sort for small slices.
    if (high <= low + 10) {
        insertion_sort_slice(vm, array, low, high);
        return;
    }

    // A classic bug here is to use (low + high) / 2, ignoring the possibility of overflow.
    size_t mid = low + (high - low) / 2;

    mergesort_slice(vm, array, aux_array, low, mid);
    if (vm->halt_flag) {
        return;
    }

    mergesort_slice(vm, array, aux_array, mid + 1, high);
    if (vm->halt_flag) {
        return;
    }

    merge(vm, array, aux_array, low, mid, high);
}


void pyro_mergesort_with_callback(PyroVM* vm, PyroValue* values, size_t count, PyroValue callback) {
    if (count < 2) {
        return;
    }

    PyroValue* aux_array = PYRO_ALLOCATE_ARRAY(vm, PyroValue, count);
    if (!aux_array) {
        pyro_panic(vm, "out of memory");
        return;
    }

    mergesort_slice_with_cb(vm, values, aux_array, 0, count - 1, callback);
    PYRO_FREE_ARRAY(vm, PyroValue, aux_array, count);
}


void pyro_mergesort(PyroVM* vm, PyroValue* values, size_t count) {
    if (count < 2) {
        return;
    }

    PyroValue* aux_array = PYRO_ALLOCATE_ARRAY(vm, PyroValue, count);
    if (!aux_array) {
        pyro_panic(vm, "out of memory");
        return;
    }

    mergesort_slice(vm, values, aux_array, 0, count - 1);
    PYRO_FREE_ARRAY(vm, PyroValue, aux_array, count);
}


/* --------- */
/*  Testing  */
/* --------- */


bool pyro_is_sorted(PyroVM* vm, PyroValue* values, size_t count) {
    if (count < 2) {
        return true;
    }

    for (size_t i = 0; i < count - 1; i++) {
        PyroValue a = values[i];
        PyroValue b = values[i + 1];

        bool b_is_less_than_a = pyro_op_compare_lt(vm, b, a);
        if (vm->halt_flag) {
            return false;
        }

        if (b_is_less_than_a) {
            return false;
        }
    }

    return true;
}


bool pyro_is_sorted_with_callback(PyroVM* vm, PyroValue* values, size_t count, PyroValue callback) {
    if (count < 2) {
        return true;
    }

    for (size_t i = 0; i < count - 1; i++) {
        PyroValue a = values[i];
        PyroValue b = values[i + 1];

        if (!pyro_push(vm, callback)) return false;
        if (!pyro_push(vm, b)) return false;
        if (!pyro_push(vm, a)) return false;
        PyroValue b_is_less_than_a = pyro_call_function(vm, 2);

        if (vm->halt_flag) {
            return false;
        }

        if (!PYRO_IS_BOOL(b_is_less_than_a)) {
            pyro_panic(vm, "comparison function must return a boolean");
            return false;
        }

        if (b_is_less_than_a.as.boolean) {
            return false;
        }
    }

    return true;
}
