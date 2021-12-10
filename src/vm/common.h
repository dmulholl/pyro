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

// Local includes.
#include "../pyro.h"

// Forward declarations.
typedef struct Obj Obj;
typedef struct ObjStr ObjStr;
typedef struct ObjMap ObjMap;
typedef struct ObjVec ObjVec;
typedef struct ObjClass ObjClass;

#endif
