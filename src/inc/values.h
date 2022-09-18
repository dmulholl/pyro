#ifndef pyro_values_h
#define pyro_values_h

#include "pyro.h"

typedef enum {
    VAL_BOOL,       // A Pyro boolean, true or false.
    VAL_NULL,       // A Pyro null.
    VAL_I64,        // A 64-bit signed integer.
    VAL_F64,        // A 64-bit float.
    VAL_CHAR,       // A 32-bit unsigned integer representing a Unicode code point.
    VAL_OBJ,        // A pointer to a heap-allocated object.
    VAL_TOMBSTONE,  // Used internally by the map implementation.
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double f64;
        int64_t i64;
        uint64_t u64;
        uint32_t u32;
        Obj* obj;
    } as;
} Value;

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_BUF,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_ERR,
    OBJ_FILE,
    OBJ_FN,
    OBJ_INSTANCE,
    OBJ_ITER,
    OBJ_MAP,
    OBJ_MAP_AS_SET,
    OBJ_MAP_AS_WEAKREF,
    OBJ_MODULE,
    OBJ_NATIVE_FN,
    OBJ_QUEUE,
    OBJ_RESOURCE_POINTER,
    OBJ_STR,
    OBJ_TUP,
    OBJ_UPVALUE,
    OBJ_VEC,
    OBJ_VEC_AS_STACK,
} ObjType;

// The VM maintains a linked list of all heap-allocated objects using the [next] pointers.
// Not every object has an associated class so [class] can be NULL.
struct Obj {
    Obj* next;
    ObjClass* class;
    ObjType type;
    bool is_marked;
};

// Macros for creating Value instances.
#define MAKE_I64(c_i64)             ((Value){VAL_I64, {.i64 = c_i64}})
#define MAKE_F64(c_f64)             ((Value){VAL_F64, {.f64 = c_f64}})
#define MAKE_CHAR(c_u32)            ((Value){VAL_CHAR, {.u32 = c_u32}})
#define MAKE_OBJ(c_ptr)             ((Value){VAL_OBJ, {.obj = (Obj*)c_ptr}})

// Inline functions for creating Value instance.
static inline Value pyro_make_bool(bool value) {
    return (Value){VAL_BOOL, {.boolean = value}};
}

static inline Value pyro_make_i64(int64_t value) {
    return (Value){VAL_I64, {.i64 = value}};
}

static inline Value pyro_make_f64(double value) {
    return (Value){VAL_F64, {.f64 = value}};
}

static inline Value pyro_make_char(uint32_t value) {
    return (Value){VAL_CHAR, {.u32 = value}};
}

static inline Value pyro_make_obj(void* value) {
    return (Value){VAL_OBJ, {.obj = (Obj*)value}};
}

static inline Value pyro_make_tombstone() {
    return (Value){VAL_TOMBSTONE, {.i64 = 0}};
}

static inline Value pyro_make_null() {
    return (Value){VAL_NULL, {.i64 = 0}};
}

// Macros for checking the type of a Value instance.
#define IS_BOOL(value)              ((value).type == VAL_BOOL)
#define IS_NULL(value)              ((value).type == VAL_NULL)
#define IS_I64(value)               ((value).type == VAL_I64)
#define IS_F64(value)               ((value).type == VAL_F64)
#define IS_OBJ(value)               ((value).type == VAL_OBJ)
#define IS_TOMBSTONE(value)         ((value).type == VAL_TOMBSTONE)
#define IS_CHAR(value)              ((value).type == VAL_CHAR)

// Macros for checking if a Value instance is an object of a specific type.
#define IS_STR(value)               pyro_is_obj_of_type(value, OBJ_STR)
#define IS_FN(value)                pyro_is_obj_of_type(value, OBJ_FN)
#define IS_CLOSURE(value)           pyro_is_obj_of_type(value, OBJ_CLOSURE)
#define IS_NATIVE_FN(value)         pyro_is_obj_of_type(value, OBJ_NATIVE_FN)
#define IS_CLASS(value)             pyro_is_obj_of_type(value, OBJ_CLASS)
#define IS_INSTANCE(value)          pyro_is_obj_of_type(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value)      pyro_is_obj_of_type(value, OBJ_BOUND_METHOD)
#define IS_MAP(value)               pyro_is_obj_of_type(value, OBJ_MAP)
#define IS_TUP(value)               pyro_is_obj_of_type(value, OBJ_TUP)
#define IS_MOD(value)               pyro_is_obj_of_type(value, OBJ_MODULE)
#define IS_VEC(value)               pyro_is_obj_of_type(value, OBJ_VEC)
#define IS_BUF(value)               pyro_is_obj_of_type(value, OBJ_BUF)
#define IS_FILE(value)              pyro_is_obj_of_type(value, OBJ_FILE)
#define IS_ITER(value)              pyro_is_obj_of_type(value, OBJ_ITER)
#define IS_STACK(value)             pyro_is_obj_of_type(value, OBJ_VEC_AS_STACK)
#define IS_SET(value)               pyro_is_obj_of_type(value, OBJ_MAP_AS_SET)
#define IS_QUEUE(value)             pyro_is_obj_of_type(value, OBJ_QUEUE)
#define IS_RESOURSE_POINTER(value)  pyro_is_obj_of_type(value, OBJ_RESOURCE_POINTER)
#define IS_ERR(value)               pyro_is_obj_of_type(value, OBJ_ERR)

// Macros for extracting object pointers from Value instances.
#define AS_OBJ(value)               ((value).as.obj)
#define AS_STR(value)               ((ObjStr*)AS_OBJ(value))
#define AS_FN(value)                ((ObjFn*)AS_OBJ(value))
#define AS_CLOSURE(value)           ((ObjClosure*)AS_OBJ(value))
#define AS_CLASS(value)             ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)          ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value)      ((ObjBoundMethod*)AS_OBJ(value))
#define AS_MAP(value)               ((ObjMap*)AS_OBJ(value))
#define AS_TUP(value)               ((ObjTup*)AS_OBJ(value))
#define AS_NATIVE_FN(value)         ((ObjNativeFn*)AS_OBJ(value))
#define AS_MOD(value)               ((ObjModule*)AS_OBJ(value))
#define AS_VEC(value)               ((ObjVec*)AS_OBJ(value))
#define AS_BUF(value)               ((ObjBuf*)AS_OBJ(value))
#define AS_FILE(value)              ((ObjFile*)AS_OBJ(value))
#define AS_ITER(value)              ((ObjIter*)AS_OBJ(value))
#define AS_QUEUE(value)             ((ObjQueue*)AS_OBJ(value))
#define AS_RESOURCE_POINTER(value)  ((ObjResourcePointer*)AS_OBJ(value))
#define AS_ERR(value)               ((ObjErr*)AS_OBJ(value))

// Returns a pointer to the value's class, if the value has a class, otherwise NULL.
ObjClass* pyro_get_class(PyroVM* vm, Value value);

// Returns the named method if it exists, otherwise NULL.
Value pyro_get_method(PyroVM* vm, Value value, ObjStr* method_name);
Value pyro_get_pub_method(PyroVM* vm, Value value, ObjStr* method_name);

// Returns true if the named method exists.
bool pyro_has_method(PyroVM* vm, Value value, ObjStr* method_name);
bool pyro_has_pub_method(PyroVM* vm, Value value, ObjStr* method_name);

// Dumps a value to the VM's output stream for debugging. This doesn't allocate memory or call into
// Pyro code.
void pyro_dump_value(PyroVM* vm, Value value);

// Returns the value's 64-bit hash.
uint64_t pyro_hash_value(PyroVM* vm, Value value);

// Checks for strict equality -- same type, same value/object.
bool pyro_compare_eq_strict(Value a, Value b);

// Returns true if the value is truthy.
bool pyro_is_truthy(Value value);

// True if [value] is an object of the specified type.
static inline bool pyro_is_obj_of_type(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
