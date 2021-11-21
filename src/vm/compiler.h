#ifndef pyro_compiler_h
#define pyro_compiler_h

#include "common.h"
#include "values.h"
#include "objects.h"

// One of three things can happen when this function is called:
//
//  * Returns [ObjFn*]. Compilation succeeded.
//
//  * Returns [NULL]. Compilation failed due to a syntax error in [src_code]. [vm->status_code] is
//    set to ERR_SYNTAX_ERROR. A message describing the syntax error has already been printed to
//    [vm->err_file].
//
//  * Returns [NULL]. Compilation failed due to an out-of-memory error. [vm->status_code] is set
//    to ERR_OUT_OF_MEMORY.
//
// Note that the compiler itself never triggers a panic, it simply returns [NULL] in case of
// failure.
ObjFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, ObjModule* mod);

// This function is used by the garbage collector.
void pyro_mark_compiler_roots(PyroVM* vm);

#endif
