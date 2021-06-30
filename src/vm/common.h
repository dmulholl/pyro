#ifndef pyro_common_h
#define pyro_common_h

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

#include <dirent.h>
#include <unistd.h>

#include "../pyro.h"

typedef struct Parser Parser;
typedef struct Obj Obj;
typedef struct ObjStr ObjStr;
typedef struct ObjMap ObjMap;
typedef struct ObjClass ObjClass;

// Selects the hash function for strings.
#ifndef PYRO_STRING_HASH
    #define PYRO_STRING_HASH pyro_fnv1a_64
#endif

// Sets the (count/capacity) threshold for map resizing.
#ifndef PYRO_MAX_HASHMAP_LOAD
    #define PYRO_MAX_HASHMAP_LOAD 0.65
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

// This value determines the interval between garbage collections.
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

#endif
