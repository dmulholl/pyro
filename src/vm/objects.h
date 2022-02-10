#ifndef pyro_objects_h
#define pyro_objects_h

#include "pyro.h"
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

/* ---- */
/* Maps */
/* ---- */

typedef struct {
    Value key;
    Value value;
} MapEntry;

// A linear-probing hash map. New entries are appended to [entry_array] so iterating over this
// array returns the entries in insertion order.
struct ObjMap {
    Obj obj;

    // The number of live entries in the map. In addition to these live entries [entry_array]
    // and [index_array] can contain independently varying numbers of tombstones.
    size_t live_entry_count;

    // This array is the map's data store -- new entries are appended to this array so iterating
    // over it preserves insertion order. [entry_array_count] includes both live entries and
    // tombstones.
    MapEntry* entry_array;
    size_t entry_array_capacity;
    size_t entry_array_count;

    // This is the array that gets linearly-probed using key hashes. It stores the indexes of
    // entries in [entry_array]. Negative values indicate that a slot is empty or a tombstone.
    // [index_array_count] includes both live entries and tombstones.
    int64_t* index_array;
    size_t index_array_capacity;
    size_t index_array_count;

    // This gets recalculated every time [index_array] is resized using the formula:
    //
    //   [max_load_threshold] = [index_array_capacity] * PYRO_MAX_HASHMAP_LOAD
    //
    // Invariant: [index_array_count] <= [max_load_threshold].
    size_t max_load_threshold;
};

ObjMap* ObjMap_new(PyroVM* vm);
ObjMap* ObjMap_new_as_weakref(PyroVM* vm);
ObjMap* ObjMap_new_as_set(PyroVM* vm);
bool ObjMap_get(ObjMap* map, Value key, Value* value, PyroVM* vm);
bool ObjMap_remove(ObjMap* map, Value key, PyroVM* vm);

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
ObjVec* ObjVec_new_as_stack(PyroVM* vm);

// Returns true if the value was successfully appended, false if memory allocation failed.
bool ObjVec_append(ObjVec* vec, Value value, PyroVM* vm);

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

/* ------- */
/* Modules */
/* ------- */

struct ObjModule {
    Obj obj;
    ObjMap* globals;
    ObjMap* submodules;
};

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
    ObjMap* methods;
    ObjVec* field_initializers;
    ObjMap* field_indexes;
    ObjStr* name;                       // Can be NULL.
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
uint64_t ObjTup_hash(PyroVM* vm, ObjTup* tup);
bool ObjTup_check_equal(ObjTup* a, ObjTup* b, PyroVM* vm);

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
ObjBuf* ObjBuf_new_with_cap_and_fill(size_t capacity, uint8_t fill_value, PyroVM* vm);
ObjBuf* ObjBuf_new_from_string(ObjStr* string, PyroVM* vm);
bool ObjBuf_append_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm);
bool ObjBuf_append_bytes(ObjBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm);

// This function converts the contents of the buffer into a string, leaving a valid but empty
// buffer behind. Returns NULL if memory cannot be allocated for the new string object -- in this
// case the buffer is unchanged.
ObjStr* ObjBuf_to_str(ObjBuf* buf, PyroVM* vm);

// Appends a byte value to the buffer as a hex-escaped string: \x##.
bool ObjBuf_append_hex_escaped_byte(ObjBuf* buf, uint8_t byte, PyroVM* vm);

// Writes a printf-style formatted string to the buffer. Returns the number of bytes written if the
// entire string can be written to the buffer. Otherwise writes nothing to the buffer and returns -1.
int64_t ObjBuf_write(ObjBuf* buf, PyroVM* vm, const char* format_string, ...);
int64_t ObjBuf_write_v(ObjBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// Writes a printf-style formatted string to the buffer. Returns true if the entire string can be
// written to the buffer. Otherwise, writes as much as possible of the string to the buffer and
// return false.
bool ObjBuf_best_effort_write_v(ObjBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// Attempts to grow the buffer to *at least* the required capacity. Returns true on success, false
// if memory allocation failed. In this case the buffer is unchanged.
bool ObjBuf_grow(ObjBuf* buf, size_t required_capacity, PyroVM* vm);

/* ----- */
/* Files */
/* ----- */

typedef struct {
    Obj obj;
    FILE* stream;
} ObjFile;

ObjFile* ObjFile_new(PyroVM* vm, FILE* stream);

// Reads the next line from the file and returns it as a string, stripping the terminating LF or
// CRLF. Returns NULL on EOF. Will panic and return NULL if memory allocation fails or if an I/O
// read error occurs.
ObjStr* ObjFile_read_line(ObjFile* file, PyroVM* vm);

/* ------ */
/* Queues */
/* ------ */

typedef struct QueueItem {
    Value value;
    struct QueueItem* next;
} QueueItem;

typedef struct {
    Obj obj;
    QueueItem* head;
    QueueItem* tail;
    size_t count;
} ObjQueue;

// Creates a new queue object. Returns NULL if memory allocation failed.
ObjQueue* ObjQueue_new(PyroVM* vm);

// Returns true if the value was successfully enqueued, false if memory allocation failed.
bool ObjQueue_enqueue(ObjQueue* queue, Value value, PyroVM* vm);

// Returns true if a value was successfully dequeued, false if the queue was empty.
bool ObjQueue_dequeue(ObjQueue* queue, Value* value, PyroVM* vm);

/* --------- */
/* Iterators */
/* --------- */

typedef enum {
    ITER_ENUM,
    ITER_FILE_LINES,
    ITER_FUNC_FILTER,
    ITER_FUNC_MAP,
    ITER_GENERIC,
    ITER_MAP_ENTRIES,
    ITER_MAP_KEYS,
    ITER_MAP_VALUES,
    ITER_QUEUE,
    ITER_RANGE,
    ITER_STR_BYTES,
    ITER_STR_CHARS,
    ITER_STR_LINES,
    ITER_TUP,
    ITER_VEC,
} IterType;

typedef struct {
    Obj obj;
    Obj* source;
    IterType iter_type;
    size_t next_index;
    int64_t next_enum;
    int64_t range_next;
    int64_t range_stop;
    int64_t range_step;
    QueueItem* next_queue_item;
    Obj* callback;
} ObjIter;

// Creates a new iterator. Returns NULL if the attempt to allocate memory fails.
ObjIter* ObjIter_new(Obj* source, IterType iter_type, PyroVM* vm);

// Returns the next item from the sequence or an [err] if the sequence has been exhausted.
// Note that this method may call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the return value.
Value ObjIter_next(ObjIter* iter, PyroVM* vm);

// Stringifies the elements returned by the iterator and joins them into a string, separated by
// [sep]. This method can call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the result.
ObjStr* ObjIter_join(ObjIter* iter, const char* sep, size_t sep_length, PyroVM* vm);

#endif
