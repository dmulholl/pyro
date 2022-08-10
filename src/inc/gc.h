#ifndef pyro_gc_h
#define pyro_gc_h

#include "pyro.h"

// Runs Pyro's simple mark-and-sweep garbage collector.
// This function can trigger a hard panic if sufficient memory cannot be allocated for the grey
// stack so the caller should check [vm->halt_flag] immediately on return.
void pyro_collect_garbage(PyroVM* vm);

#endif
