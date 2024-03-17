#ifndef pyro_h
#define pyro_h

// Language version.
#define PYRO_VERSION_MAJOR 0
#define PYRO_VERSION_MINOR 16
#define PYRO_VERSION_PATCH 0

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

// Sets the initial capacity of the call stack -- the argument is the number of call frames.
#ifndef PYRO_INITIAL_CALL_STACK_CAPACITY
    #ifdef PYRO_DEBUG
        #define PYRO_INITIAL_CALL_STACK_CAPACITY 2
    #else
        #define PYRO_INITIAL_CALL_STACK_CAPACITY 64
    #endif
#endif

// Sets the initial capacity of the value stack -- the argument is the number of values.
#ifndef PYRO_INITIAL_VALUE_STACK_CAPACITY
    #ifdef PYRO_DEBUG
        #define PYRO_INITIAL_VALUE_STACK_CAPACITY 2
    #else
        #define PYRO_INITIAL_VALUE_STACK_CAPACITY 1024
    #endif
#endif

// Initial garbage collection threshold in bytes. Defaults to 4MB.
#ifndef PYRO_INIT_GC_THRESHOLD
    #define PYRO_INIT_GC_THRESHOLD (1024 * 1024 * 4)
#endif

// This value determines the interval between garbage collections. After each garbage
// collection the threshold for the next collection is set to:
// [vm->bytes_allocated *  PYRO_GC_HEAP_GROW_FACTOR]
#ifndef PYRO_GC_HEAP_GROW_FACTOR
    #define PYRO_GC_HEAP_GROW_FACTOR 2
#endif

// The multiplier to use when increasing a buffer's memory allocation.
#ifndef PYRO_BUF_MEMORY_MULTIPLIER
    #define PYRO_BUF_MEMORY_MULTIPLIER 2
#endif

// The multiplier to use when increasing a vector's memory allocation.
#ifndef PYRO_VEC_MEMORY_MULTIPLIER
    #define PYRO_VEC_MEMORY_MULTIPLIER 2
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
// This macro must be defined *before* the standard library headers are included.
// Ref: https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
#ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE 700
#endif

#ifdef __has_include
    #define __pyro_has_include(x) __has_include(x)
#else
    #define __pyro_has_include(x) 0
#endif

#ifdef __has_builtin
    #define __pyro_has_builtin(x) __has_builtin(x)
#else
    #define __pyro_has_builtin(x) 0
#endif

// C standard library headers -- no OS-dependent headers.
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

#if __pyro_has_include(<stdckdint.h>)
    #include <stdckdint.h>
#endif

// Forward declarations.
typedef struct PyroVM PyroVM;
typedef struct PyroObject PyroObject;
typedef struct PyroStr PyroStr;
typedef struct PyroMap PyroMap;
typedef struct PyroVec PyroVec;
typedef struct PyroClass PyroClass;
typedef struct PyroMod PyroMod;
typedef struct PyroFile PyroFile;

// Pyro headers.
#include "./opcodes.h"
#include "./lexer.h"
#include "./values.h"
#include "./objects.h"
#include "./panics.h"
#include "./prng.h"
#include "./string_pool.h"
#include "./vm.h"
#include "./compiler.h"
#include "./debug.h"
#include "./exec.h"
#include "./gc.h"
#include "./heap.h"
#include "./imports.h"
#include "./io.h"
#include "./operators.h"
#include "./os.h"
#include "./setup.h"
#include "./sorting.h"
#include "./stringify.h"
#include "./utf8.h"
#include "./utils.h"
#include "./stdlib_builtins.h"
#include "./stdlib_modules.h"
#include "./embeds.h"

#endif
