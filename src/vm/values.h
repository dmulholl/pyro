#ifndef pyro_values_h
#define pyro_values_h

#include "common.h"


/* ------ */
/* Values */
/* ------ */


typedef enum {
    VAL_BOOL,
    VAL_NULL,
    VAL_I64,
    VAL_F64,
    VAL_OBJ,
    VAL_TOMBSTONE,
    VAL_EMPTY,
    VAL_CHAR,
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


/* ------- */
/* Objects */
/* ------- */


typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FN,
    OBJ_INSTANCE,
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
} ObjType;


typedef enum {
    MAP_ITER_KEYS,
    MAP_ITER_VALUES,
    MAP_ITER_ENTRIES,
} MapIterType;


typedef enum {
    STR_ITER_BYTES,
    STR_ITER_CHARS,
} StrIterType;


struct Obj {
    Obj* next;
    ObjClass* class;
    ObjType type;
    bool is_marked;
};


/* ------ */
/* ObjStr */
/* ------ */


struct ObjStr {
    Obj obj;
    uint64_t hash;
    size_t length;
    char* bytes;
};

// Create a new string object by copying [length] bytes from [src], ignoring backslashed escapes.
ObjStr* ObjStr_copy_raw(const char* source, size_t length, PyroVM* vm);

// Create a new string object by copying [length] bytes from [src], processing backslashed escapes.
ObjStr* ObjStr_copy_esc(const char* src, size_t length, PyroVM* vm);

// Create a new string object by taking ownership of [src].
ObjStr* ObjStr_take(char* src, size_t length, PyroVM* vm);

// Create a new string object by concatenating two source strings.
ObjStr* ObjStr_concat(ObjStr* s1, ObjStr* s2, PyroVM* vm);

// Returns the VM's cached empty string if it exists, otherwise creates a new empty string.
ObjStr* ObjStr_empty(PyroVM* vm);

typedef struct {
    Obj obj;
    ObjStr* string;
    StrIterType iter_type;
    size_t next_index;
} ObjStrIter;

ObjStrIter* ObjStrIter_new(ObjStr* string, StrIterType iter_type, PyroVM* vm);


/* ------ */
/* ObjMap */
/* ------ */


typedef struct {
    Value key;
    Value value;
} MapEntry;

struct ObjMap {
    Obj obj;
    size_t count;
    size_t capacity;
    size_t tombstone_count;
    size_t max_load_threshold;
    MapEntry* entries;
};

ObjMap* ObjMap_new(PyroVM* vm);
ObjMap* ObjMap_new_weakref(PyroVM* vm);
bool ObjMap_get(ObjMap* map, Value key, Value* value);
bool ObjMap_remove(ObjMap* map, Value key);

// Adds a new entry to the map or updates an existing entry.
// - Returns 0 if the entry was not added because additional memory could not be allocated.
// - Returns 1 if a new entry was successfully added to the map.
// - Returns 2 if an existing entry was successfully updated.
int ObjMap_set(ObjMap* map, Value key, Value value, PyroVM* vm);

// Copy all entries from [src] to [dst].
void ObjMap_copy_entries(ObjMap* src, ObjMap* dst, PyroVM* vm);

// Attempts to update an existing entry. Returns [true] if successful, [false] if no corresponding
// entry was found.
bool ObjMap_update_entry(ObjMap* map, Value key, Value value, PyroVM* vm);

typedef struct {
    Obj obj;
    ObjMap* map;
    MapIterType iter_type;
    size_t next_index;
} ObjMapIter;

ObjMapIter* ObjMapIter_new(ObjMap* map, MapIterType iter_type, PyroVM* vm);
Value ObjMapIter_next(ObjMapIter* iterator, PyroVM* vm);


/* ------ */
/* ObjVec */
/* ------ */


typedef struct {
    Obj obj;
    size_t count;
    size_t capacity;
    Value* values;
} ObjVec;

ObjVec* ObjVec_new(PyroVM* vm);
ObjVec* ObjVec_new_with_cap(size_t capacity, PyroVM* vm);
ObjVec* ObjVec_new_with_cap_and_fill(size_t capacity, Value fill_value, PyroVM* vm);
void ObjVec_append(ObjVec* vec, Value value, PyroVM* vm);

// Copy all entries from [src] to [dst].
void ObjVec_copy_entries(ObjVec* src, ObjVec* dst, PyroVM* vm);

typedef struct {
    Obj obj;
    ObjVec* vec;
    size_t next_index;
} ObjVecIter;

ObjVecIter* ObjVecIter_new(ObjVec* vec, PyroVM* vm);


/* --------- */
/* ObjModule */
/* --------- */


typedef struct {
    Obj obj;
    ObjMap* globals;
    ObjMap* submodules;
} ObjModule;

ObjModule* ObjModule_new(PyroVM* vm);


/* ----- */
/* ObjFn */
/* ----- */


typedef struct {
    Obj obj;
    int arity;
    int upvalue_count;
    ObjStr* name;
    ObjStr* source;
    ObjModule* module;

    // The function's bytecode, stored as a dynamic array of bytes.
    uint8_t* code;
    size_t code_count;
    size_t code_capacity;

    // The function's constant table, stored as a dynamic array of values.
    Value* constants;
    size_t constants_count;
    size_t constants_capacity;

    // Number of bytecode bytes per line of source code.
    size_t first_line_number;
    uint16_t* bpl;
    size_t bpl_capacity;
} ObjFn;

ObjFn* ObjFn_new(PyroVM* vm);
void ObjFn_write(ObjFn* fn, uint8_t byte, size_t line_number, PyroVM* vm);
size_t ObjFn_add_constant(ObjFn* fn, Value value, PyroVM* vm);
size_t ObjFn_opcode_argcount(ObjFn* fn, size_t ip);
size_t ObjFn_get_line_number(ObjFn* fn, size_t ip);


/* ----------- */
/* ObjNativeFn */
/* ----------- */


typedef Value (*NativeFn)(PyroVM* vm, size_t arg_count, Value* args);

typedef struct {
    Obj obj;
    int arity;
    NativeFn fn_ptr;
    ObjStr* name;
} ObjNativeFn;

ObjNativeFn* ObjNativeFn_new(PyroVM* vm, NativeFn fn_ptr, ObjStr* name, int arity);


/* -------- */
/* ObjClass */
/* -------- */


struct ObjClass {
    Obj obj;
    ObjStr* name;
    ObjMap* methods;
    ObjVec* field_initializers;
    ObjMap* field_indexes;
};

ObjClass* ObjClass_new(PyroVM* vm, ObjStr* name);


/* ----------- */
/* ObjInstance */
/* ----------- */


typedef struct {
    Obj obj;
    Value fields[];
} ObjInstance;

ObjInstance* ObjInstance_new(PyroVM* vm, ObjClass* class);


/* ---------- */
/* ObjUpvalue */
/* ---------- */


// The [next] pointer is used by the VM to maintain a linked list of open upvalues pointing to
// variables still on the stack.
typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

ObjUpvalue* ObjUpvalue_new(PyroVM* vm, Value* slot);


/* ---------- */
/* ObjClosure */
/* ---------- */


typedef struct {
    Obj obj;
    ObjFn* fn;
    ObjUpvalue** upvalues;
    int upvalue_count;
} ObjClosure;

ObjClosure* ObjClosure_new(PyroVM* vm, ObjFn* func);


/* -------------- */
/* ObjBoundMethod */
/* -------------- */


typedef struct {
    Obj obj;
    Value receiver;
    Obj* method; // ObjClosure or ObjNativeFn
} ObjBoundMethod;

ObjBoundMethod* ObjBoundMethod_new(PyroVM* vm, Value receiver, Obj* method);


/* ------ */
/* ObjTup */
/* ------ */


typedef struct {
    Obj obj;
    size_t count;
    Value values[];
} ObjTup;

ObjTup* ObjTup_new(size_t count, PyroVM* vm);
ObjTup* ObjTup_new_err(size_t count, PyroVM* vm);
uint64_t ObjTup_hash(ObjTup* tup);
bool ObjTup_check_equal(ObjTup* a, ObjTup* b);

typedef struct {
    Obj obj;
    ObjTup* tup;
    size_t next_index;
} ObjTupIter;

ObjTupIter* ObjTupIter_new(ObjTup* tup, PyroVM* vm);


/* -------- */
/* ObjRange */
/* -------- */


typedef struct {
    Obj obj;
    int64_t next;
    int64_t stop;
    int64_t step;
} ObjRange;

ObjRange* ObjRange_new(int64_t start, int64_t stop, int64_t step, PyroVM* vm);


/* ------ */
/* ObjBuf */
/* ------ */


typedef struct {
    Obj obj;
    size_t count;
    size_t capacity;
    uint8_t* bytes;
} ObjBuf;

ObjBuf* ObjBuf_new(PyroVM* vm);
bool ObjBuf_append_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm);
bool ObjBuf_append_bytes(ObjBuf* buf, size_t count, uint8_t* bytes, bool newline, PyroVM* vm);


/* ------ */
/* Macros */
/* ------ */


#define IS_BOOL(value)              ((value).type == VAL_BOOL)
#define IS_NULL(value)              ((value).type == VAL_NULL)
#define IS_I64(value)               ((value).type == VAL_I64)
#define IS_F64(value)               ((value).type == VAL_F64)
#define IS_OBJ(value)               ((value).type == VAL_OBJ)
#define IS_TOMBSTONE(value)         ((value).type == VAL_TOMBSTONE)
#define IS_EMPTY(value)             ((value).type == VAL_EMPTY)
#define IS_CHAR(value)              ((value).type == VAL_CHAR)

#define BOOL_VAL(c_bool)            ((Value){VAL_BOOL, {.boolean = c_bool}})
#define I64_VAL(c_i64)              ((Value){VAL_I64, {.i64 = c_i64}})
#define F64_VAL(c_f64)              ((Value){VAL_F64, {.f64 = c_f64}})
#define OBJ_VAL(c_ptr)              ((Value){VAL_OBJ, {.obj = (Obj*)c_ptr}})
#define TOMBSTONE_VAL()             ((Value){VAL_TOMBSTONE, {.i64 = 0}})
#define EMPTY_VAL()                 ((Value){VAL_EMPTY, {.i64 = 0}})
#define NULL_VAL()                  ((Value){VAL_NULL, {.i64 = 0}})
#define CHAR_VAL(c_u32)             ((Value){VAL_CHAR, {.u32 = c_u32}})

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

// Make a string object or value by copying a C string.
#define STR_OBJ(c_string)           ObjStr_copy_raw(c_string, strlen(c_string), vm)
#define STR_VAL(c_string)           OBJ_VAL(STR_OBJ(c_string))


/* --------- */
/* Functions */
/* --------- */


uint64_t pyro_hash(Value value);
bool pyro_check_equal(Value a, Value b);

void pyro_debug_value(PyroVM* vm, Value value);
void pyro_debug_object(PyroVM* vm, Obj* object);

ObjStr* pyro_stringify_value(PyroVM* vm, Value value, bool call_method);
ObjStr* pyro_stringify_object(PyroVM* vm, Obj* object, bool call_method);
ObjStr* pyro_format_value(PyroVM* vm, Value value, const char* format);

char* pyro_stringify_obj_type(ObjType type);

ObjClass* pyro_get_class(Value value);
Value pyro_get_method(PyroVM* vm, Value receiver, ObjStr* method_name);
bool pyro_has_method(PyroVM* vm, Value receiver, ObjStr* method_name);

// Performs a lexicographic comparison using byte values.
// - Returns -1 if a < b.
// - Returns 0 if a == b.
// - Returns 1 if a > b.
int pyro_compare_strings(ObjStr* a, ObjStr* b);

static inline bool pyro_is_obj_of_type(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

bool pyro_is_truthy(Value value);

#endif
