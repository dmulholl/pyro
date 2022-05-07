#ifndef pyro_h
#define pyro_h

// Language version.
#define PYRO_VERSION_MAJOR 0
#define PYRO_VERSION_MINOR 6
#define PYRO_VERSION_PATCH 21

// Pre-release label, e.g. "alpha1", "beta2", "rc3".
#ifndef PYRO_VERSION_LABEL
    #define PYRO_VERSION_LABEL ""
#endif

// Build label, e.g. "release", "debug".
#ifndef PYRO_VERSION_BUILD
    #define PYRO_VERSION_BUILD ""
#endif

// Selects the hash function for strings.
#ifndef PYRO_STRING_HASH
    #define PYRO_STRING_HASH pyro_fnv1a_64_opt
#endif

// Sets the (count/capacity) threshold for map resizing.
#ifndef PYRO_MAX_HASHMAP_LOAD
    #define PYRO_MAX_HASHMAP_LOAD 0.5
#endif

// Max number of values on the stack. Each value is 16 bytes so the default size is 256KB.
#ifndef PYRO_STACK_SIZE
    #define PYRO_STACK_SIZE (1024 * 16)
#endif

// Max number of call frames.
#ifndef PYRO_MAX_CALL_FRAMES
    #define PYRO_MAX_CALL_FRAMES 1024
#endif

// Initial garbage collection threshold in bytes. Defaults to 1MB.
#ifndef PYRO_INIT_GC_THRESHOLD
    #define PYRO_INIT_GC_THRESHOLD (1024 * 1024)
#endif

// This value determines the interval between garbage collections. After each garbage collection
// the threshold for the next collection is set to vm->bytes_allocated * PYRO_GC_HEAP_GROW_FACTOR.
#ifndef PYRO_GC_HEAP_GROW_FACTOR
    #define PYRO_GC_HEAP_GROW_FACTOR 2
#endif

// Pi to the maximum accuracy limit of 64-bit IEEE 754 floats.
#ifndef PYRO_PI
    #define PYRO_PI 3.14159265358979323846
#endif

// Euler's constant to the maximum accuracy limit of 64-bit IEEE 754 floats.
#ifndef PYRO_E
    #define PYRO_E 2.7182818284590452354
#endif

// Defining this macro causes header files to expose definitions conforming to POSIX 2008.
#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700
#endif

// C standard library -- no OS-dependent headers.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>

// Forward declarations.
typedef struct PyroVM PyroVM;
typedef struct Obj Obj;
typedef struct ObjStr ObjStr;
typedef struct ObjMap ObjMap;
typedef struct ObjVec ObjVec;
typedef struct ObjClass ObjClass;
typedef struct ObjModule ObjModule;

#endif
