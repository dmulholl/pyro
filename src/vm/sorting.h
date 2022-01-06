#ifndef pyro_sorting_h
#define pyro_sorting_h

#include "pyro.h"
#include "values.h"

// Shuffles the array using Fisher-Yates/Durstenfeld shuffling.
void pyro_shuffle(PyroVM* vm, Value* values, size_t count);

// Returns true if the array is sorted in ascending order.
// This function can call into Pyro code and can panic or set the exit flag.
bool pyro_is_sorted(PyroVM* vm, Value* values, size_t count);

// Returns true if the array is sorted in ascending order.
// This function can call into Pyro code and can set the panic or exit flags.
// [callback] should take two values [a] and [b] and return true if [a < b].
bool pyro_is_sorted_with_callback(PyroVM* vm, Value* values, size_t count, Value callback);

// Sorts the array using quicksort.
// This function can call into Pyro code and can set the panic or exit flags.
void pyro_quicksort(PyroVM* vm, Value* values, size_t count);

// Sorts the array using quicksort.
// This function can call into Pyro code and can set the panic or exit flags.
// [callback] should take two values [a] and [b] and return true if [a < b].
void pyro_quicksort_with_callback(PyroVM* vm, Value* values, size_t count, Value callback);

// Sorts the array using mergesort.
// This function can call into Pyro code and can set the panic or exit flags.
void pyro_mergesort(PyroVM* vm, Value* values, size_t count);

// Sorts the array using mergesort.
// This function can call into Pyro code and can set the panic or exit flags.
// [callback] should take two values [a] and [b] and return true if [a < b].
void pyro_mergesort_with_callback(PyroVM* vm, Value* values, size_t count, Value callback);

#endif
