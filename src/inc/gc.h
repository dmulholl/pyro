#ifndef pyro_gc_h
#define pyro_gc_h

#include "pyro.h"

// Runs Pyro's garbage collector which uses a simple mark-and-sweep algorithm. This function can
// trigger a hard panic if memory cannot be (re)allocated for the grey stack so the caller should
// check [vm->halt_flag] immediately on return.
void pyro_collect_garbage(PyroVM* vm);

#endif
