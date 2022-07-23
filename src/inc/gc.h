#ifndef pyro_gc_h
#define pyro_gc_h

#include "pyro.h"

// Runs the garbage collector. Calling this function can trigger a hard panic if sufficient memory
// cannot be allocated for the grey stack.
void pyro_collect_garbage(PyroVM* vm);

#endif
