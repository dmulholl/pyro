#ifndef pyro_h
#define pyro_h

// Language version.
#define PYRO_VERSION_MAJOR 0
#define PYRO_VERSION_MINOR 8
#define PYRO_VERSION_PATCH 60

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

// Sets the initial capacity of the call-frame stack.
#ifndef PYRO_INITIAL_CALL_FRAME_CAPACITY
    #ifdef DEBUG
        #define PYRO_INITIAL_CALL_FRAME_CAPACITY 2
    #else
        #define PYRO_INITIAL_CALL_FRAME_CAPACITY 64
    #endif
#endif

// Initial garbage collection threshold in bytes. Defaults to 4MB.
#ifndef PYRO_INIT_GC_THRESHOLD
    #define PYRO_INIT_GC_THRESHOLD (1024 * 1024 * 4)
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
// This macro must be defined *before* the standard library headers are imorted.
// Ref: https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
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
typedef struct PyroObj PyroObj;
typedef struct PyroObjStr PyroObjStr;
typedef struct PyroObjMap PyroObjMap;
typedef struct PyroObjVec PyroObjVec;
typedef struct PyroObjClass PyroObjClass;
typedef struct PyroObjModule PyroObjModule;
typedef struct PyroObjFile PyroObjFile;

#endif
