#ifndef pyro_objects_h
#define pyro_objects_h

#include "common.h"
#include "values.h"

/* ------- */
/* Strings */
/* ------- */

struct ObjStr {
    Obj obj;
    uint64_t hash;
    size_t length;
    char* bytes;
};

// Creates a new string object by copying [length] bytes from [src], ignoring backslashed escapes.
// If [length] is 0, [src] can be NULL. Returns NULL if the attempt to allocate memory for the
// string object fails.
ObjStr* ObjStr_copy_raw(const char* source, size_t length, PyroVM* vm);

// Creates a new string object by copying [length] bytes from [src], processing backslashed escapes.
// If [length] is 0, [src] can be NULL. Returns NULL if the attempt to allocate memory for the
// string object fails.
ObjStr* ObjStr_copy_esc(const char* src, size_t length, PyroVM* vm);

// Creates a new string object taking ownership of a null-terminated, heap-allocated byte array,
// [src], where [length] is the number of bytes in the array, not including the terminating null.
// Returns NULL if the attempt to allocate memory for the new string object fails -- in this case
// the input array is not altered or freed.
ObjStr* ObjStr_take(char* src, size_t length, PyroVM* vm);

// Creates a new string object by concatenating two source strings. Returns NULL if memory cannot
// be allocated for the new string.
ObjStr* ObjStr_concat(ObjStr* s1, ObjStr* s2, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the source string.
// Returns NULL if memory cannot be allocated for the new string.
ObjStr* ObjStr_concat_n_copies(ObjStr* str, size_t n, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the codepoint encoded as utf-8.
// Returns NULL if memory cannot be allocated for the new string.
ObjStr* ObjStr_concat_n_codepoints_as_utf8(uint32_t codepoint, size_t n, PyroVM* vm);

// Creates a new string object by concatenating two utf-8 encoded codepoints.
// Returns NULL if memory cannot be allocated for the new string.
ObjStr* ObjStr_concat_codepoints_as_utf8(uint32_t cp1, uint32_t cp2, PyroVM* vm);

// Creates a new string by prepending/appending a utf-8 encoded codepoint to an existing string.
// Returns NULL if memory cannot be allocated for the new string.
ObjStr* ObjStr_prepend_codepoint_as_utf8(ObjStr* str, uint32_t codepoint, PyroVM* vm);
ObjStr* ObjStr_append_codepoint_as_utf8(ObjStr* str, uint32_t codepoint, PyroVM* vm);

// Returns the VM's cached empty string if it exists, otherwise creates a new empty string.
ObjStr* ObjStr_empty(PyroVM* vm);

// Returns a quoted, backslash-escaped string representation of the string's content. Returns NULL
// if memory could not be allocated for the new string.
ObjStr* ObjStr_debug_str(ObjStr* str, PyroVM* vm);

typedef enum {
    STR_ITER_BYTES,
    STR_ITER_CHARS,
} StrIterType;

typedef struct {
    Obj obj;
    ObjStr* string;
    StrIterType iter_type;
    size_t next_index;
} ObjStrIter;

// Creates a new string iterator. Returns NULL if the attempt to allocate memory fails.
ObjStrIter* ObjStrIter_new(ObjStr* string, StrIterType iter_type, PyroVM* vm);

/* ---- */
/* Maps */
/* ---- */

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

// Copies all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false] if
// the operation failed because memory could not be allocated for the new entries. The operation
// may fail after some of the entries have successfully been copied.
bool ObjMap_copy_entries(ObjMap* src, ObjMap* dst, PyroVM* vm);

// Attempts to update an existing entry. Returns [true] if successful, [false] if no corresponding
// entry was found.
bool ObjMap_update_entry(ObjMap* map, Value key, Value value, PyroVM* vm);

// Creates a new map object by copying [src]. Returns NULL if sufficient memory cannot be allocated
// for the copy.
ObjMap* ObjMap_copy(ObjMap* src, PyroVM* vm);

// This function can call into Pyro code and can trigger an exit or a panic. The caller should check
// the halt flag before using the return value. Returns NULL if memory cannot be allocated for the
// new string.
ObjStr* ObjMap_stringify(ObjMap* map, PyroVM* vm);

typedef enum {
    MAP_ITER_KEYS,
    MAP_ITER_VALUES,
    MAP_ITER_ENTRIES,
} MapIterType;

typedef struct {
    Obj obj;
    ObjMap* map;
    MapIterType iter_type;
    size_t next_index;
} ObjMapIter;

ObjMapIter* ObjMapIter_new(ObjMap* map, MapIterType iter_type, PyroVM* vm);
Value ObjMapIter_next(ObjMapIter* iterator, PyroVM* vm);

/* ------- */
/* Vectors */
/* ------- */

struct ObjVec {
    Obj obj;
    size_t count;
    size_t capacity;
    Value* values;
};

ObjVec* ObjVec_new(PyroVM* vm);
ObjVec* ObjVec_new_with_cap(size_t capacity, PyroVM* vm);
ObjVec* ObjVec_new_with_cap_and_fill(size_t capacity, Value fill_value, PyroVM* vm);
bool ObjVec_append(ObjVec* vec, Value value, PyroVM* vm);

// This function can call into Pyro code and can trigger an exit or a panic. The caller should check
// the halt flag before using the return value. Returns NULL if memory cannot be allocated for the
// new string.
ObjStr* ObjVec_stringify(ObjVec* vec, PyroVM* vm);

// Sorts the vector in-place using the mergesort algorithm. If [fn] is not NULL it will be used
// to compare pairs of values. This function calls into code which may panic or set the exit flag.
// Returns [false] if memory could not be allocated for the auxiliary array.
bool ObjVec_mergesort(ObjVec* vec, Value fn, PyroVM* vm);

// Returns a copy of the [src] vector. Returns NULL if memory cannot be allocated for the copy.
ObjVec* ObjVec_copy(ObjVec* src, PyroVM* vm);

// Appends all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false] if
// the operation failed because enough memory could not be allocated for the extra entries.
bool ObjVec_copy_entries(ObjVec* src, ObjVec* dst, PyroVM* vm);

// Removes and returns the last item from the vector. Panics and returns NULL_VAL if the vector
// is emtpy.
Value ObjVec_remove_last(ObjVec* vec, PyroVM* vm);

// Removes and returns the first item from the vector. Panics and returns NULL_VAL if the vector
// is emtpy.
Value ObjVec_remove_first(ObjVec* vec, PyroVM* vm);

// Removes and returns the item at [index]. Panics and returns NULL_VAL if the index is out of
// range.
Value ObjVec_remove_at_index(ObjVec* vec, size_t index, PyroVM* vm);

// Inserts [value] at [index], where [index] is less than or equal to the vector's item count.
// Panics if [index] is out of range or if memory allocation fails.
void ObjVec_insert_at_index(ObjVec* vec, size_t index, Value value, PyroVM* vm);

typedef struct {
    Obj obj;
    ObjVec* vec;
    size_t next_index;
} ObjVecIter;

ObjVecIter* ObjVecIter_new(ObjVec* vec, PyroVM* vm);

/* ------- */
/* Modules */
/* ------- */

typedef struct {
    Obj obj;
    ObjMap* globals;
    ObjMap* submodules;
} ObjModule;

ObjModule* ObjModule_new(PyroVM* vm);

/* -------------- */
/* Pyro Functions */
/* -------------- */

typedef struct {
    Obj obj;
    size_t upvalue_count;
    ObjStr* name;
    ObjStr* source;

    // The number of arguments required by the function.
    uint8_t arity;

    // The function's bytecode, stored as a dynamic array of bytes.
    uint8_t* code;
    size_t code_count;
    size_t code_capacity;

    // The function's constant table, stored as a dynamic array of values.
    Value* constants;
    size_t constants_count;
    size_t constants_capacity;

    // This information lets us reconstruct the source code line number corresponding to each
    // bytecode instruction. The [bpl] array stores the number of bytecode bytes per line of
    // source code, where the array index is the offset from [first_line_number].
    size_t first_line_number;
    uint16_t* bpl;
    size_t bpl_capacity;
} ObjFn;

ObjFn* ObjFn_new(PyroVM* vm);

// Writes [byte] to the function's bytecode array. Returns [true] if the write succeeded, [false] if
// the write failed because memory could not be allocated.
bool ObjFn_write(ObjFn* fn, uint8_t byte, size_t line_number, PyroVM* vm);

// This method adds a value to the function's constant table and returns its index. If an identical
// value is already present in the table it avoids adding a duplicate and returns the index of
// the existing entry instead. Returns -1 if the operation failed because sufficient memory
// could not be allocated for the constant table.
int64_t ObjFn_add_constant(ObjFn* fn, Value value, PyroVM* vm);

// Returns the length in bytes of the arguments for the opcode at the specified index.
size_t ObjFn_opcode_argcount(ObjFn* fn, size_t ip);

// This method returns the source code line number corresponding to the bytecode instruction at
// index [ip].
size_t ObjFn_get_line_number(ObjFn* fn, size_t ip);

/* ---------------- */
/* Native Functions */
/* ---------------- */

// Signature definition for Pyro functions and methods natively implemented in C.
typedef Value (*NativeFn)(PyroVM* vm, size_t arg_count, Value* args);

// [arity = -1] means that the function accepts a variable number of arguments.
typedef struct {
    Obj obj;
    NativeFn fn_ptr;
    ObjStr* name;
    int arity;
} ObjNativeFn;

ObjNativeFn* ObjNativeFn_new(PyroVM* vm, NativeFn fn_ptr, const char* name, int arity);

/* ------- */
/* Classes */
/* ------- */

struct ObjClass {
    Obj obj;
    ObjStr* name;                       // Can be NULL.
    ObjMap* methods;
    ObjVec* field_initializers;
    ObjMap* field_indexes;
    ObjClass* superclass;               // Can be NULL.
};

ObjClass* ObjClass_new(PyroVM* vm);

/* --------- */
/* Instances */
/* --------- */

typedef struct {
    Obj obj;
    Value fields[];
} ObjInstance;

ObjInstance* ObjInstance_new(PyroVM* vm, ObjClass* class);

/* -------- */
/* Upvalues */
/* -------- */

// The [next] pointer is used by the VM to maintain a linked list of open upvalues pointing to
// variables still on the stack.
typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

ObjUpvalue* ObjUpvalue_new(PyroVM* vm, Value* slot);

/* -------- */
/* Closures */
/* -------- */

typedef struct {
    Obj obj;
    ObjFn* fn;
    ObjModule* module;
    ObjUpvalue** upvalues;
    size_t upvalue_count;
} ObjClosure;

ObjClosure* ObjClosure_new(PyroVM* vm, ObjFn* func, ObjModule* module);

/* ------------- */
/* Bound Methods */
/* ------------- */

typedef struct {
    Obj obj;
    Value receiver;
    Obj* method; // ObjClosure or ObjNativeFn
} ObjBoundMethod;

ObjBoundMethod* ObjBoundMethod_new(PyroVM* vm, Value receiver, Obj* method);

/* ------ */
/* Tuples */
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

// This function can call into Pyro code and can trigger an exit or a panic. The caller should check
// the halt flag before using the return value. Returns NULL if memory cannot be allocated for the
// new string.
ObjStr* ObjTup_stringify(ObjTup* tup, PyroVM* vm);

typedef struct {
    Obj obj;
    ObjTup* tup;
    size_t next_index;
} ObjTupIter;

ObjTupIter* ObjTupIter_new(ObjTup* tup, PyroVM* vm);

/* ------ */
/* Ranges */
/* ------ */

typedef struct {
    Obj obj;
    int64_t next;
    int64_t stop;
    int64_t step;
} ObjRange;

ObjRange* ObjRange_new(int64_t start, int64_t stop, int64_t step, PyroVM* vm);

/* ------- */
/* Buffers */
/* ------- */

typedef struct {
    Obj obj;
    size_t count;
    size_t capacity;
    uint8_t* bytes;
} ObjBuf;

ObjBuf* ObjBuf_new(PyroVM* vm);
ObjBuf* ObjBuf_new_with_cap(size_t capacity, PyroVM* vm);
bool ObjBuf_append_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm);
bool ObjBuf_append_bytes(ObjBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm);
ObjStr* ObjBuf_to_str(ObjBuf* buf, PyroVM* vm);

// Appends a byte value to the buffer as a hex-escaped string: \x##.
bool ObjBuf_append_hex_escaped_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm);

/* ----- */
/* Files */
/* ----- */

typedef struct {
    Obj obj;
    FILE* stream;
} ObjFile;

ObjFile* ObjFile_new(PyroVM* vm);

/* --------- */
/* Iterators */
/* --------- */

typedef enum {
    ITER_STR_BYTES,
    ITER_STR_CHARS,
    ITER_VEC,
    ITER_MAP_KEYS,
    ITER_MAP_VALUES,
    ITER_MAP_ENTRIES,
    ITER_TUP,
} IterType;

typedef struct {
    Obj obj;
    Obj* source;
    IterType iter_type;
    size_t next_index;
} ObjIter;

// Creates a new iterator. Returns NULL if the attempt to allocate memory fails.
ObjIter* ObjIter_new(Obj* source, IterType iter_type, PyroVM* vm);

Value ObjIter_next(ObjIter* iter, PyroVM* vm);

#endif
