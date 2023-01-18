#ifndef pyro_gc_h
#define pyro_gc_h

// Runs Pyro's garbage collector which uses a simple mark-and-sweep algorithm.
// - This function can panic and set the [vm->panic_flag] if garbage collection fails.
void pyro_collect_garbage(PyroVM* vm);

#endif
