#ifndef pyro_values_h
#define pyro_values_h

// Type-set for Pyro's fundamental [PyroValue] type. Every [PyroValue] has a [.type] field with
// one of these enum values.
typedef enum {
    PYRO_VALUE_BOOL,       // A Pyro boolean, true or false.
    PYRO_VALUE_CHAR,       // A 32-bit unsigned integer representing a Unicode code point.
    PYRO_VALUE_F64,        // A 64-bit float.
    PYRO_VALUE_I64,        // A 64-bit signed integer.
    PYRO_VALUE_NULL,       // A Pyro null.
    PYRO_VALUE_OBJ,        // A pointer to a heap-allocated object.
    PYRO_VALUE_TOMBSTONE,  // Used internally by the map implementation.
} PyroValueType;

// Tagged union for Pyro's fundamental [PyroValue] type. Every value in Pyro is a [PyroValue], e.g.
// variables, vector entries, map keys, map values, etc.
typedef struct {
    PyroValueType type;
    union {
        bool boolean;
        double f64;
        int64_t i64;
        uint64_t u64;
        uint32_t u32;
        PyroObject* obj;
    } as;
} PyroValue;

// Type-set for heap-allocated Pyro objects, i.e. Pyro values with type [PYRO_VALUE_OBJ].
// Every [PyroObject] has a [.type] field with one of these enum values.
typedef enum {
    PYRO_OBJECT_BOUND_METHOD,
    PYRO_OBJECT_BUF,
    PYRO_OBJECT_CLASS,
    PYRO_OBJECT_CLOSURE,
    PYRO_OBJECT_ERR,
    PYRO_OBJECT_FILE,
    PYRO_OBJECT_INSTANCE,
    PYRO_OBJECT_ITER,
    PYRO_OBJECT_MAP,
    PYRO_OBJECT_MAP_AS_SET,
    PYRO_OBJECT_MAP_AS_WEAKREF,
    PYRO_OBJECT_MODULE,
    PYRO_OBJECT_NATIVE_FN,
    PYRO_OBJECT_FN,
    PYRO_OBJECT_QUEUE,
    PYRO_OBJECT_RESOURCE_POINTER,
    PYRO_OBJECT_STR,
    PYRO_OBJECT_TUP,
    PYRO_OBJECT_UPVALUE,
    PYRO_OBJECT_VEC,
    PYRO_OBJECT_VEC_AS_STACK,
} PyroObjectType;

// Base type for all heap-allocated objects, i.e. Pyro values with type [PYRO_VALUE_OBJ].
// The VM maintains a linked list of all heap-allocated objects using the [.next] pointers.
// Not every object has an associated class so [.class] can be NULL.
struct PyroObject {
    PyroObject* next;
    PyroClass* class;
    PyroObjectType type;
    bool is_marked;
};

// Converts a C boolean to a Pyro value.
static inline PyroValue pyro_bool(bool value) {
    return (PyroValue){PYRO_VALUE_BOOL, {.boolean = value}};
}

// Converts a C integer to a Pyro value.
static inline PyroValue pyro_i64(int64_t value) {
    return (PyroValue){PYRO_VALUE_I64, {.i64 = value}};
}

// Converts a C double to a Pyro value.
static inline PyroValue pyro_f64(double value) {
    return (PyroValue){PYRO_VALUE_F64, {.f64 = value}};
}

// Converts a C integer to a Pyro value.
static inline PyroValue pyro_char(uint32_t value) {
    return (PyroValue){PYRO_VALUE_CHAR, {.u32 = value}};
}

// Converts a C pointer to a Pyro value.
static inline PyroValue pyro_obj(void* value) {
    return (PyroValue){PYRO_VALUE_OBJ, {.obj = (PyroObject*)value}};
}

// Creates a Pyro tombstone value.
static inline PyroValue pyro_tombstone() {
    return (PyroValue){PYRO_VALUE_TOMBSTONE, {.i64 = 0}};
}

// Creates a Pyro null value.
static inline PyroValue pyro_null() {
    return (PyroValue){PYRO_VALUE_NULL, {.i64 = 0}};
}

// Macros for checking the type of a PyroValue instance.
#define PYRO_IS_BOOL(value)              ((value).type == PYRO_VALUE_BOOL)
#define PYRO_IS_NULL(value)              ((value).type == PYRO_VALUE_NULL)
#define PYRO_IS_I64(value)               ((value).type == PYRO_VALUE_I64)
#define PYRO_IS_F64(value)               ((value).type == PYRO_VALUE_F64)
#define PYRO_IS_OBJ(value)               ((value).type == PYRO_VALUE_OBJ)
#define PYRO_IS_TOMBSTONE(value)         ((value).type == PYRO_VALUE_TOMBSTONE)
#define PYRO_IS_CHAR(value)              ((value).type == PYRO_VALUE_CHAR)

// Macros for checking if a PyroValue instance is an object of a specific type.
#define PYRO_IS_STR(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_STR)
#define PYRO_IS_PYRO_FN(value)           pyro_is_obj_of_type(value, PYRO_OBJECT_FN)
#define PYRO_IS_CLOSURE(value)           pyro_is_obj_of_type(value, PYRO_OBJECT_CLOSURE)
#define PYRO_IS_NATIVE_FN(value)         pyro_is_obj_of_type(value, PYRO_OBJECT_NATIVE_FN)
#define PYRO_IS_CLASS(value)             pyro_is_obj_of_type(value, PYRO_OBJECT_CLASS)
#define PYRO_IS_INSTANCE(value)          pyro_is_obj_of_type(value, PYRO_OBJECT_INSTANCE)
#define PYRO_IS_BOUND_METHOD(value)      pyro_is_obj_of_type(value, PYRO_OBJECT_BOUND_METHOD)
#define PYRO_IS_MAP(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_MAP)
#define PYRO_IS_TUP(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_TUP)
#define PYRO_IS_MOD(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_MODULE)
#define PYRO_IS_VEC(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_VEC)
#define PYRO_IS_BUF(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_BUF)
#define PYRO_IS_FILE(value)              pyro_is_obj_of_type(value, PYRO_OBJECT_FILE)
#define PYRO_IS_ITER(value)              pyro_is_obj_of_type(value, PYRO_OBJECT_ITER)
#define PYRO_IS_STACK(value)             pyro_is_obj_of_type(value, PYRO_OBJECT_VEC_AS_STACK)
#define PYRO_IS_SET(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_MAP_AS_SET)
#define PYRO_IS_QUEUE(value)             pyro_is_obj_of_type(value, PYRO_OBJECT_QUEUE)
#define PYRO_IS_RESOURSE_POINTER(value)  pyro_is_obj_of_type(value, PYRO_OBJECT_RESOURCE_POINTER)
#define PYRO_IS_ERR(value)               pyro_is_obj_of_type(value, PYRO_OBJECT_ERR)

// Macros for extracting object pointers from PyroValue instances.
#define PYRO_AS_OBJ(value)               ((value).as.obj)
#define PYRO_AS_STR(value)               ((PyroStr*)PYRO_AS_OBJ(value))
#define PYRO_AS_PYRO_FN(value)           ((PyroFn*)PYRO_AS_OBJ(value))
#define PYRO_AS_CLOSURE(value)           ((PyroClosure*)PYRO_AS_OBJ(value))
#define PYRO_AS_CLASS(value)             ((PyroClass*)PYRO_AS_OBJ(value))
#define PYRO_AS_INSTANCE(value)          ((PyroInstance*)PYRO_AS_OBJ(value))
#define PYRO_AS_BOUND_METHOD(value)      ((PyroBoundMethod*)PYRO_AS_OBJ(value))
#define PYRO_AS_MAP(value)               ((PyroMap*)PYRO_AS_OBJ(value))
#define PYRO_AS_TUP(value)               ((PyroTup*)PYRO_AS_OBJ(value))
#define PYRO_AS_NATIVE_FN(value)         ((PyroNativeFn*)PYRO_AS_OBJ(value))
#define PYRO_AS_MOD(value)               ((PyroMod*)PYRO_AS_OBJ(value))
#define PYRO_AS_VEC(value)               ((PyroVec*)PYRO_AS_OBJ(value))
#define PYRO_AS_BUF(value)               ((PyroBuf*)PYRO_AS_OBJ(value))
#define PYRO_AS_FILE(value)              ((PyroFile*)PYRO_AS_OBJ(value))
#define PYRO_AS_ITER(value)              ((PyroIter*)PYRO_AS_OBJ(value))
#define PYRO_AS_QUEUE(value)             ((PyroQueue*)PYRO_AS_OBJ(value))
#define PYRO_AS_RESOURCE_POINTER(value)  ((PyroResourcePointer*)PYRO_AS_OBJ(value))
#define PYRO_AS_ERR(value)               ((PyroErr*)PYRO_AS_OBJ(value))

// Returns a pointer to the value's class, if the value has a class, otherwise NULL.
PyroClass* pyro_get_class(PyroVM* vm, PyroValue value);

// Returns the named method if it exists, otherwise NULL.
PyroValue pyro_get_method(PyroVM* vm, PyroValue value, PyroStr* method_name);
PyroValue pyro_get_pub_method(PyroVM* vm, PyroValue value, PyroStr* method_name);

// Returns true if the named method exists.
bool pyro_has_method(PyroVM* vm, PyroValue value, PyroStr* method_name);
bool pyro_has_pub_method(PyroVM* vm, PyroValue value, PyroStr* method_name);

// Dumps a value to the VM's output stream for debugging. This doesn't allocate memory or call into
// Pyro code.
void pyro_dump_value(PyroVM* vm, PyroValue value);

// Returns the value's 64-bit hash.
uint64_t pyro_hash_value(PyroVM* vm, PyroValue value);

// Checks for strict equality -- same type, same value/object.
bool pyro_compare_eq_strict(PyroValue a, PyroValue b);

// Returns true if the value is truthy.
bool pyro_is_truthy(PyroValue value);

// True if [value] is an object of the specified type.
static inline bool pyro_is_obj_of_type(PyroValue value, PyroObjectType type) {
    return PYRO_IS_OBJ(value) && PYRO_AS_OBJ(value)->type == type;
}

#endif
