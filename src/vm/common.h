#ifndef pyro_common_h
#define pyro_common_h

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

// Local includes.
#include "../pyro.h"

// Forward declarations.
typedef struct PyroVM PyroVM;
typedef struct Obj Obj;
typedef struct ObjStr ObjStr;
typedef struct ObjMap ObjMap;
typedef struct ObjVec ObjVec;
typedef struct ObjClass ObjClass;
typedef struct ObjModule ObjModule;

// Error codes used by the VM.
typedef enum {
    ERR_OK,
    ERR_ERROR,
    ERR_OUT_OF_MEMORY,
    ERR_OS_ERROR,
    ERR_ARGS_ERROR,
    ERR_ASSERTION_FAILED,
    ERR_TYPE_ERROR,
    ERR_NAME_ERROR,
    ERR_VALUE_ERROR,
    ERR_MODULE_NOT_FOUND,
    ERR_SYNTAX_ERROR,
} PyroErrorCode;

#endif
