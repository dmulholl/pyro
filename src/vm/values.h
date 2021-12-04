#ifndef pyro_values_h
#define pyro_values_h

#include "common.h"

typedef enum {
    VAL_BOOL,       // A Pyro boolean, true or false.
    VAL_NULL,       // A Pyro null.
    VAL_I64,        // A 64-bit signed integer.
    VAL_F64,        // A 64-bit float.
    VAL_CHAR,       // A 32-bit unsigned integer representing a Unicode code point.
    VAL_OBJ,        // A pointer to a heap-allocated object.
    VAL_EMPTY,      // Used internally by the map implementation.
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
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FN,
    OBJ_INSTANCE,
    OBJ_ITER,
    OBJ_MAP,
    OBJ_MAP_ITER,
    OBJ_NATIVE_FN,
    OBJ_STR,
    OBJ_STR_ITER,
    OBJ_TUP,
    OBJ_TUP_ITER,
    OBJ_UPVALUE,
    OBJ_WEAKREF_MAP,
    OBJ_MODULE,
    OBJ_VEC,
    OBJ_VEC_ITER,
    OBJ_ERR,
    OBJ_RANGE,
    OBJ_BUF,
    OBJ_FILE,
} ObjType;

struct Obj {
    Obj* next;
    ObjClass* class;    // Can be NULL.
    ObjType type;
    bool is_marked;
};

// Macros for creating Value instances.
#define BOOL_VAL(c_bool)            ((Value){VAL_BOOL, {.boolean = c_bool}})
#define I64_VAL(c_i64)              ((Value){VAL_I64, {.i64 = c_i64}})
#define F64_VAL(c_f64)              ((Value){VAL_F64, {.f64 = c_f64}})
#define OBJ_VAL(c_ptr)              ((Value){VAL_OBJ, {.obj = (Obj*)c_ptr}})
#define TOMBSTONE_VAL()             ((Value){VAL_TOMBSTONE, {.i64 = 0}})
#define EMPTY_VAL()                 ((Value){VAL_EMPTY, {.i64 = 0}})
#define NULL_VAL()                  ((Value){VAL_NULL, {.i64 = 0}})
#define CHAR_VAL(c_u32)             ((Value){VAL_CHAR, {.u32 = c_u32}})

// Macros for checking the type of a Value instance.
#define IS_BOOL(value)              ((value).type == VAL_BOOL)
#define IS_NULL(value)              ((value).type == VAL_NULL)
#define IS_I64(value)               ((value).type == VAL_I64)
#define IS_F64(value)               ((value).type == VAL_F64)
#define IS_OBJ(value)               ((value).type == VAL_OBJ)
#define IS_TOMBSTONE(value)         ((value).type == VAL_TOMBSTONE)
#define IS_EMPTY(value)             ((value).type == VAL_EMPTY)
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
#define IS_ERR(value)               pyro_is_obj_of_type(value, OBJ_ERR)
#define IS_RANGE(value)             pyro_is_obj_of_type(value, OBJ_RANGE)
#define IS_BUF(value)               pyro_is_obj_of_type(value, OBJ_BUF)
#define IS_FILE(value)              pyro_is_obj_of_type(value, OBJ_FILE)

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
#define AS_TUP_ITER(value)          ((ObjTupIter*)AS_OBJ(value))
#define AS_VEC_ITER(value)          ((ObjVecIter*)AS_OBJ(value))
#define AS_MAP_ITER(value)          ((ObjMapIter*)AS_OBJ(value))
#define AS_STR_ITER(value)          ((ObjStrIter*)AS_OBJ(value))
#define AS_RANGE(value)             ((ObjRange*)AS_OBJ(value))
#define AS_BUF(value)               ((ObjBuf*)AS_OBJ(value))
#define AS_FILE(value)              ((ObjFile*)AS_OBJ(value))
#define AS_ITER(value)              ((ObjIter*)AS_OBJ(value))

// Macros for creating string objects by copying C strings.
#define STR_OBJ(c_string)           ObjStr_copy_raw(c_string, strlen(c_string), vm)

// Returns a pointer to the value's class, if the value has a class, otherwise NULL.
ObjClass* pyro_get_class(Value value);

// Returns the named method if it is defined for the value, otherwise NULL_VAL().
Value pyro_get_method(Value value, ObjStr* method_name);

// Returns [true] if the named method is defined for the value.
bool pyro_has_method(Value value, ObjStr* method_name);

// Dumps a value to the VM's output stream for debugging. This doesn't allocate memory or call into
// Pyro code.
void pyro_dump_value(PyroVM* vm, Value value);

// Returns the value's 64-bit hash.
uint64_t pyro_hash_value(Value value);

// Returns [true] if the values are equal.
bool pyro_check_equal(Value a, Value b);

// Returns [true] if the value is truthy.
bool pyro_is_truthy(Value value);

// Compares two values of the same type.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
// - Returns 2 if the values can not be compared.
int pyro_compare_values(Value a, Value b);

// Can compare any combination of i64, f64, or char values.
// Returns -1 if a < b.
// Returns 0 if a == b.
// Returns 1 if a > b.
// Returns 2 or -2 if the values cannot be compared.
int pyro_compare_values_numerically(Value a, Value b);

// Constructs and returns the default string representation of a value. A lot can go wrong here:
//
// 1 - This function attempts to allocate memory and can trigger the GC.
// 2 - This function assumes that any object passed into it has been fully initialized.
// 3 - This function can call into Pyro code to execute an object's :$str() method.
// 4 - This function can trigger a panic, exit, or memory error, setting the halt flag.
//
// The caller should check the halt flag immediately on return. If the flag is set the caller should
// clean up any allocated resources and unwind the call stack.
//
// These functions return NULL if sufficient memory could not be allocated for the string.
ObjStr* pyro_stringify_value(PyroVM* vm, Value value);

// Constructs and returns the formatted string representation of a value using the format string
// [format]. A lot can go wrong here:
//
// 1 - This function attempts to allocate memory and can trigger the GC.
// 2 - This function assumes that any object passed into it has been fully initialized.
// 3 - This function can call into Pyro code to execute an object's :$fmt() method.
// 4 - This function can trigger a panic, exit, or memory error, setting the halt flag.
//
// The caller should check the halt flag immediately on return. If the flag is set the caller should
// clean up any allocated resources and unwind the call stack.
//
// This function returns NULL if sufficient memory could not be allocated for the string.
ObjStr* pyro_format_value(PyroVM* vm, Value value, const char* format);

// Returns a pointer to a static string. Can be safely called from the GC.
char* pyro_stringify_obj_type(ObjType type);

// Performs a lexicographic comparison using byte values.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
int pyro_compare_strings(ObjStr* a, ObjStr* b);

// Creates a quoted, backslash-escaped string representing the character's value. Returns NULL if
// memory could not be allocated for the new string.
ObjStr* pyro_char_to_debug_str(PyroVM* vm, Value c);

// True if [value] is an object of the specified type.
static inline bool pyro_is_obj_of_type(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
