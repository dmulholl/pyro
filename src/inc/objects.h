#ifndef pyro_objects_h
#define pyro_objects_h

#include "pyro.h"
#include "values.h"

/* ------- */
/* Strings */
/* ------- */

struct PyroObjStr {
    PyroObj obj;
    uint64_t hash;
    size_t length;
    char* bytes;
};

// Creates a new string object by copying the null-terminated C-string [src], ignoring backslashed
// escapes.
PyroObjStr* PyroObjStr_new(const char* src, PyroVM* vm);

// Creates a new string object by copying [length] bytes from [src], ignoring backslashed escapes.
// If [length] is 0, [src] can be NULL. Returns NULL if the attempt to allocate memory for the
// string object fails.
PyroObjStr* PyroObjStr_copy_raw(const char* src, size_t length, PyroVM* vm);

// Creates a new string object by copying [length] bytes from [src], processing backslashed escapes.
// If [length] is 0, [src] can be NULL. Returns NULL if the attempt to allocate memory for the
// string object fails.
PyroObjStr* PyroObjStr_copy_esc(const char* src, size_t length, PyroVM* vm);

// Creates a new string object taking ownership of a null-terminated, heap-allocated byte array,
// [src], where [length] is the number of bytes in the array, not including the terminating null.
// Returns NULL if the attempt to allocate memory for the new string object fails -- in this case
// the input array is not altered or freed.
PyroObjStr* PyroObjStr_take(char* src, size_t length, PyroVM* vm);

// Creates a new string object by concatenating two source strings. Returns NULL if memory cannot
// be allocated for the new string.
PyroObjStr* PyroObjStr_concat(PyroObjStr* s1, PyroObjStr* s2, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the source string.
// Returns NULL if memory cannot be allocated for the new string.
PyroObjStr* PyroObjStr_concat_n_copies(PyroObjStr* str, size_t n, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the codepoint encoded as utf-8.
// Returns NULL if memory cannot be allocated for the new string.
PyroObjStr* PyroObjStr_concat_n_codepoints_as_utf8(uint32_t codepoint, size_t n, PyroVM* vm);

// Creates a new string object by concatenating two utf-8 encoded codepoints.
// Returns NULL if memory cannot be allocated for the new string.
PyroObjStr* PyroObjStr_concat_codepoints_as_utf8(uint32_t cp1, uint32_t cp2, PyroVM* vm);

// Creates a new string by prepending/appending a utf-8 encoded codepoint to an existing string.
// Returns NULL if memory cannot be allocated for the new string.
PyroObjStr* PyroObjStr_prepend_codepoint_as_utf8(PyroObjStr* str, uint32_t codepoint, PyroVM* vm);
PyroObjStr* PyroObjStr_append_codepoint_as_utf8(PyroObjStr* str, uint32_t codepoint, PyroVM* vm);

// Returns the VM's cached empty string if it exists, otherwise creates a new empty string.
PyroObjStr* PyroObjStr_empty(PyroVM* vm);

// Returns a string with '%' symbols in the input escaped as '%%'.
// Returns NULL if memory cannot be allocated for the new string.
PyroObjStr* PyroObjStr_esc_percents(const char* src, size_t length, PyroVM* vm);

/* ---- */
/* Maps */
/* ---- */

typedef struct {
    PyroValue key;
    PyroValue value;
} PyroMapEntry;

// A linear-probing hash map. New entries are appended to [entry_array] so iterating over this
// array returns the entries in insertion order.
struct PyroObjMap {
    PyroObj obj;

    // The number of live entries in the map. In addition to these live entries [entry_array]
    // and [index_array] can contain independently varying numbers of tombstones.
    size_t live_entry_count;

    // This array is the map's data store -- new entries are appended to this array so iterating
    // over it preserves insertion order. [entry_array_count] includes both live entries and
    // tombstones.
    PyroMapEntry* entry_array;
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

PyroObjMap* PyroObjMap_new(PyroVM* vm);
PyroObjMap* PyroObjMap_new_as_weakref(PyroVM* vm);
PyroObjMap* PyroObjMap_new_as_set(PyroVM* vm);
bool PyroObjMap_get(PyroObjMap* map, PyroValue key, PyroValue* value, PyroVM* vm);
bool PyroObjMap_contains(PyroObjMap* map, PyroValue key, PyroVM* vm);
void PyroObjMap_clear(PyroObjMap* map, PyroVM* vm);

// Removes an entry from the map. Returns true if the map contained an entry for [key].
bool PyroObjMap_remove(PyroObjMap* map, PyroValue key, PyroVM* vm);

// Adds a new entry to the map or updates an existing entry.
// - Returns 0 if the entry was not added because additional memory could not be allocated.
// - Returns 1 if a new entry was successfully added to the map.
// - Returns 2 if an existing entry was successfully updated.
int PyroObjMap_set(PyroObjMap* map, PyroValue key, PyroValue value, PyroVM* vm);

// Copies all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false] if
// the operation failed because memory could not be allocated for the new entries. The operation
// may fail after some of the entries have successfully been copied.
bool PyroObjMap_copy_entries(PyroObjMap* src, PyroObjMap* dst, PyroVM* vm);

// Attempts to update an existing entry. Returns [true] if successful, [false] if no corresponding
// entry was found.
bool PyroObjMap_update_entry(PyroObjMap* map, PyroValue key, PyroValue value, PyroVM* vm);

// Creates a new map object by copying [src]. Returns NULL if sufficient memory cannot be allocated
// for the copy.
PyroObjMap* PyroObjMap_copy(PyroObjMap* src, PyroVM* vm);

/* ------- */
/* Vectors */
/* ------- */

struct PyroObjVec {
    PyroObj obj;
    size_t count;
    size_t capacity;
    PyroValue* values;
};

PyroObjVec* PyroObjVec_new(PyroVM* vm);
PyroObjVec* PyroObjVec_new_with_cap(size_t capacity, PyroVM* vm);
PyroObjVec* PyroObjVec_new_with_cap_and_fill(size_t capacity, PyroValue fill_value, PyroVM* vm);
PyroObjVec* PyroObjVec_new_as_stack(PyroVM* vm);
void PyroObjVec_clear(PyroObjVec* vec, PyroVM* vm);

// Returns true if the value was successfully appended, false if memory allocation failed.
bool PyroObjVec_append(PyroObjVec* vec, PyroValue value, PyroVM* vm);

// Returns a copy of the [src] vector. Returns NULL if memory cannot be allocated for the copy.
PyroObjVec* PyroObjVec_copy(PyroObjVec* src, PyroVM* vm);

// Appends all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false] if
// the operation failed because enough memory could not be allocated for the extra entries.
bool PyroObjVec_copy_entries(PyroObjVec* src, PyroObjVec* dst, PyroVM* vm);

// Removes and returns the last item from the vector. Panics and returns NULL_VAL if the vector
// is emtpy.
PyroValue PyroObjVec_remove_last(PyroObjVec* vec, PyroVM* vm);

// Removes and returns the first item from the vector. Panics and returns NULL_VAL if the vector
// is emtpy.
PyroValue PyroObjVec_remove_first(PyroObjVec* vec, PyroVM* vm);

// Removes and returns the item at [index]. Panics and returns NULL_VAL if the index is out of
// range.
PyroValue PyroObjVec_remove_at_index(PyroObjVec* vec, size_t index, PyroVM* vm);

// Inserts [value] at [index], where [index] is less than or equal to the vector's item count.
// Panics if [index] is out of range or if memory allocation fails.
void PyroObjVec_insert_at_index(PyroObjVec* vec, size_t index, PyroValue value, PyroVM* vm);

/* ------- */
/* Modules */
/* ------- */

struct PyroObjModule {
    PyroObj obj;
    PyroObjMap* submodules;

    // Stores the module's global members.
    PyroObjVec* members;

    // Maps [PyroObjStr] names to [i64] indexes in the module's [members] vector.
    // - [all_member_indexes] contains indexes for both public and private members.
    // - [pub_member_indexes] contains indexes for only public members.
    PyroObjMap* all_member_indexes;
    PyroObjMap* pub_member_indexes;
};

PyroObjModule* PyroObjModule_new(PyroVM* vm);

/* -------------- */
/* Pyro Functions */
/* -------------- */

typedef struct {
    PyroObj obj;
    PyroObjStr* name;
    size_t upvalue_count;

    // The function's [source_id] is used in error messages to identify the function's source.
    // Typically, it will be the name of the file that contained the function.
    PyroObjStr* source_id;

    // The number of arguments required by the function.
    uint8_t arity;

    // True if the function is variadic.
    bool is_variadic;

    // The function's bytecode, stored as a dynamic array of bytes.
    uint8_t* code;
    size_t code_count;
    size_t code_capacity;

    // The function's constant table, stored as a dynamic array of values.
    PyroValue* constants;
    size_t constants_count;
    size_t constants_capacity;

    // This information lets us reconstruct the source code line number corresponding to each
    // bytecode instruction. The [bpl] array stores the number of bytecode bytes per line of
    // source code, where the array index is the offset from [first_line_number].
    size_t first_line_number;
    uint16_t* bpl;
    size_t bpl_capacity;
} PyroObjPyroFn;

PyroObjPyroFn* PyroObjPyroFn_new(PyroVM* vm);

// Writes [byte] to the function's bytecode array. Returns [true] if the write succeeded, [false] if
// the write failed because memory could not be allocated.
bool PyroObjPyroFn_write(PyroObjPyroFn* fn, uint8_t byte, size_t line_number, PyroVM* vm);

// This method adds a value to the function's constant table and returns its index. If an identical
// value is already present in the table it avoids adding a duplicate and returns the index of
// the existing entry instead. Returns -1 if the operation failed because sufficient memory
// could not be allocated for the constant table.
int64_t PyroObjPyroFn_add_constant(PyroObjPyroFn* fn, PyroValue value, PyroVM* vm);

// Returns the length in bytes of the arguments for the opcode at the specified index.
size_t PyroObjPyroFn_opcode_argcount(PyroObjPyroFn* fn, size_t ip);

// This method returns the source code line number corresponding to the bytecode instruction at
// index [ip].
size_t PyroObjPyroFn_get_line_number(PyroObjPyroFn* fn, size_t ip);

/* ---------------- */
/* Native Functions */
/* ---------------- */

// Signature definition for Pyro functions and methods natively implemented in C.
typedef PyroValue (*pyro_native_fn_t)(PyroVM* vm, size_t arg_count, PyroValue* args);

// [arity = -1] means that the function accepts a variable number of arguments.
typedef struct {
    PyroObj obj;
    PyroObjStr* name;
    pyro_native_fn_t fn_ptr;
    int arity;
} PyroObjNativeFn;

PyroObjNativeFn* PyroObjNativeFn_new(PyroVM* vm, pyro_native_fn_t fn_ptr, const char* name, int arity);

/* ------- */
/* Classes */
/* ------- */

struct PyroObjClass {
    PyroObj obj;

    // The class's name, if it has one. This field can be NULL.
    PyroObjStr* name;

    // The class's superclass, if it has one. This field can be NULL.
    PyroObjClass* superclass;

    // Instance methods. Maps [PyroObjStr] names to [PyroObjClosure] or [PyroObjNativeFn] values.
    // - [all_instance_methods] contains both public and private instance methods.
    // - [pub_instance_methods] contains only public instance methods.
    PyroObjMap* all_instance_methods;
    PyroObjMap* pub_instance_methods;

    // Initialization values for fields. Each new [PyroObjInstance] gets a copy of this vector.
    PyroObjVec* default_field_values;

    // Maps [PyroObjStr] names to [i64] indexes in the instance's [fields] vector.
    // - [all_field_indexes] contains indexes for both public and private fields.
    // - [pub_field_indexes] contains indexes for only public fields.
    PyroObjMap* all_field_indexes;
    PyroObjMap* pub_field_indexes;

    // Static members.
    PyroObjMap* static_methods;
    PyroObjMap* static_fields;

    // Cached results from the last method lookups.
    PyroObjStr* all_instance_methods_cached_name;
    PyroValue all_instance_methods_cached_value;
    PyroObjStr* pub_instance_methods_cached_name;
    PyroValue pub_instance_methods_cached_value;

    // If the class has an $init() method, we cache it here to avoid map lookups.
    PyroValue init_method;
};

PyroObjClass* PyroObjClass_new(PyroVM* vm);

/* --------- */
/* Instances */
/* --------- */

typedef struct {
    PyroObj obj;
    PyroValue fields[];
} PyroObjInstance;

PyroObjInstance* PyroObjInstance_new(PyroVM* vm, PyroObjClass* class);

/* -------- */
/* Upvalues */
/* -------- */

// The [next] pointer is used by the VM to maintain a linked list of open upvalues pointing to
// variables still on the stack.
typedef struct PyroObjUpvalue {
    PyroObj obj;
    PyroValue* location;
    PyroValue closed;
    struct PyroObjUpvalue* next;
} PyroObjUpvalue;

PyroObjUpvalue* PyroObjUpvalue_new(PyroVM* vm, PyroValue* slot);

/* -------- */
/* Closures */
/* -------- */

typedef struct {
    PyroObj obj;
    PyroObjPyroFn* fn;
    PyroObjModule* module;
    PyroObjVec* default_values;
    PyroObjUpvalue** upvalues;
    size_t upvalue_count;
} PyroObjClosure;

PyroObjClosure* PyroObjClosure_new(PyroVM* vm, PyroObjPyroFn* func, PyroObjModule* module);

/* ------------- */
/* Bound Methods */
/* ------------- */

typedef struct {
    PyroObj obj;
    PyroValue receiver;
    PyroObj* method; // PyroObjClosure or PyroObjNativeFn
} PyroObjBoundMethod;

PyroObjBoundMethod* PyroObjBoundMethod_new(PyroVM* vm, PyroValue receiver, PyroObj* method);

/* ------ */
/* Tuples */
/* ------ */

typedef struct {
    PyroObj obj;
    size_t count;
    PyroValue values[];
} PyroObjTup;

PyroObjTup* PyroObjTup_new(size_t count, PyroVM* vm);
bool PyroObjTup_check_equal(PyroObjTup* a, PyroObjTup* b, PyroVM* vm);

/* ------- */
/* Buffers */
/* ------- */

typedef struct {
    PyroObj obj;
    size_t count;
    size_t capacity;
    uint8_t* bytes;
} PyroObjBuf;

PyroObjBuf* PyroObjBuf_new(PyroVM* vm);
PyroObjBuf* PyroObjBuf_new_with_cap(size_t capacity, PyroVM* vm);
PyroObjBuf* PyroObjBuf_new_with_cap_and_fill(size_t capacity, uint8_t fill_value, PyroVM* vm);
PyroObjBuf* PyroObjBuf_new_from_string(PyroObjStr* string, PyroVM* vm);
bool PyroObjBuf_append_byte(PyroObjBuf* buf, uint8_t byte, PyroVM* vm);
bool PyroObjBuf_append_bytes(PyroObjBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm);
void PyroObjBuf_clear(PyroObjBuf* buf, PyroVM* vm);

// This function converts the contents of the buffer into a string, leaving a valid but empty
// buffer behind. Returns NULL if memory cannot be allocated for the new string object -- in this
// case the buffer is unchanged.
PyroObjStr* PyroObjBuf_to_str(PyroObjBuf* buf, PyroVM* vm);

// Appends a byte value to the buffer as a hex-escaped string: \x##.
bool PyroObjBuf_append_hex_escaped_byte(PyroObjBuf* buf, uint8_t byte, PyroVM* vm);

// Writes a printf-style formatted string to the buffer. Returns the number of bytes written if the
// entire string can be written to the buffer. Otherwise writes nothing to the buffer and returns -1.
int64_t PyroObjBuf_write_f(PyroObjBuf* buf, PyroVM* vm, const char* format_string, ...);
int64_t PyroObjBuf_write_fv(PyroObjBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// Writes a printf-style formatted string to the buffer. If sufficient memory cannot be allocated to
// write the entire string, writes as much of the string as possible.
void PyroObjBuf_try_write_fv(PyroObjBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// Attempts to grow the buffer to at least the required capacity. Returns true on success, false
// if memory allocation fails. In this case the buffer is unchanged.
bool PyroObjBuf_grow(PyroObjBuf* buf, size_t required_capacity, PyroVM* vm);

// Attempts to grow the buffer to at least the required capacity. If the buffer's capacity needs to
// be increased, it will be increased to exactly the required capacity. Returns true on success,
// false if memory allocation fails. In this case the buffer is unchanged.
bool PyroObjBuf_grow_to_fit(PyroObjBuf* buf, size_t required_capacity, PyroVM* vm);

// Attempts to grow the buffer's capacity by [n] bytes. Returns true on success, false if memory
// allocation fails. In this case the buffer is unchanged.
bool PyroObjBuf_grow_by_n_bytes(PyroObjBuf* buf, size_t n, PyroVM* vm);

/* ----- */
/* Files */
/* ----- */

struct PyroObjFile {
    PyroObj obj;
    FILE* stream; // May be NULL.
    PyroObjStr* path; // May be NULL.
};

PyroObjFile* PyroObjFile_new(PyroVM* vm, FILE* stream);

// Reads the next line from the file and returns it as a string, stripping the terminating LF or
// CRLF. Returns NULL on EOF. Will panic and return NULL if memory allocation fails or if an I/O
// read error occurs.
PyroObjStr* PyroObjFile_read_line(PyroObjFile* file, PyroVM* vm);

/* ------ */
/* Queues */
/* ------ */

typedef struct QueueItem {
    PyroValue value;
    struct QueueItem* next;
} QueueItem;

typedef struct {
    PyroObj obj;
    QueueItem* head;
    QueueItem* tail;
    size_t count;
} PyroObjQueue;

// Creates a new queue object. Returns NULL if memory allocation failed.
PyroObjQueue* PyroObjQueue_new(PyroVM* vm);

// Returns true if the value was successfully enqueued, false if memory allocation failed.
bool PyroObjQueue_enqueue(PyroObjQueue* queue, PyroValue value, PyroVM* vm);

// Returns true if a value was successfully dequeued, false if the queue was empty.
bool PyroObjQueue_dequeue(PyroObjQueue* queue, PyroValue* value, PyroVM* vm);

// Clears all entries from the queue.
void PyroObjQueue_clear(PyroObjQueue* queue, PyroVM* vm);

/* --------- */
/* Iterators */
/* --------- */

typedef enum {
    PYRO_ITER_EMPTY,
    PYRO_ITER_ENUM,
    PYRO_ITER_FILE_LINES,
    PYRO_ITER_FUNC_FILTER,
    PYRO_ITER_FUNC_MAP,
    PYRO_ITER_GENERIC,
    PYRO_ITER_MAP_ENTRIES,
    PYRO_ITER_MAP_KEYS,
    PYRO_ITER_MAP_VALUES,
    PYRO_ITER_QUEUE,
    PYRO_ITER_RANGE,
    PYRO_ITER_STR,
    PYRO_ITER_STR_BYTES,
    PYRO_ITER_STR_CHARS,
    PYRO_ITER_STR_LINES,
    PYRO_ITER_TUP,
    PYRO_ITER_VEC,
} PyroIterType;

typedef struct {
    PyroObj obj;
    PyroObj* source;
    PyroIterType iter_type;
    size_t next_index;
    int64_t next_enum;
    int64_t range_next;
    int64_t range_stop;
    int64_t range_step;
    QueueItem* next_queue_item;
    PyroObj* callback;
} PyroObjIter;

// Creates a new iterator. Returns NULL if the attempt to allocate memory fails.
PyroObjIter* PyroObjIter_new(PyroObj* source, PyroIterType iter_type, PyroVM* vm);

// Creates a new empty iterator. Returns NULL if the attempt to allocate memory fails.
PyroObjIter* PyroObjIter_empty(PyroVM* vm);

// Returns the next item from the sequence or an [err] if the sequence has been exhausted.
// Note that this method may call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the return value.
PyroValue PyroObjIter_next(PyroObjIter* iter, PyroVM* vm);

// Stringifies the elements returned by the iterator and joins them into a string, separated by
// [sep]. This method can call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the result.
PyroObjStr* PyroObjIter_join(PyroObjIter* iter, const char* sep, size_t sep_length, PyroVM* vm);

/* ------------------- */
/*  Resource Pointers  */
/* ------------------- */

// This object type is a wrapper for heap-allocated objects not managed by Pyro's garbage collector.
// The garbage collector will call the callback function before freeing the object.

typedef void (*pyro_free_rp_callback_t)(PyroVM* vm, void* pointer);

typedef struct {
    PyroObj obj;
    void* pointer;
    pyro_free_rp_callback_t callback;
} PyroObjResourcePointer;

PyroObjResourcePointer* PyroObjResourcePointer_new(void* pointer, pyro_free_rp_callback_t callback, PyroVM* vm);

/* -------- */
/*  Errors  */
/* -------- */

typedef struct {
    PyroObj obj;
    PyroObjStr* message;
    PyroObjMap* details;
} PyroObjErr;

PyroObjErr* PyroObjErr_new(PyroVM* vm);

#endif
