#ifndef pyro_objects_h
#define pyro_objects_h

/* ------- */
/* Strings */
/* ------- */

struct PyroStr {
    PyroObject obj;
    size_t count;
    size_t capacity;
    char* bytes;
    uint64_t hash;
};

// Creates a new string object by copying the null-terminated C-string [src].
// - Ignores backslashed escape sequences.
#define PyroStr_COPY(src) PyroStr_copy(src, strlen((src)), false, vm)

// Creates a new string object by copying [count] bytes from [src], which must be non-NULL.
// - Returns NULL if the attempt to allocate memory for the new string object fails.
// - If [count] is zero, returns an empty string.
PyroStr* PyroStr_copy(const char* src, size_t count, bool process_backslashed_escapes, PyroVM* vm);

// Creates a new string object by taking ownership of the heap-allocated array [bytes].
// Assumes [bytes] was allocated using PYRO_ALLOCATE_ARRAY(); it will be freed by the garbage
// collector using PYRO_FREE_ARRAY().
// - Returns NULL if the attempt to allocate memory for the new string object fails.
// - If the return value is NULL, ownership of [bytes] is returned to the caller.
// - If the return value is NULL, [bytes] is unchanged.
// - Precondition: [capacity >= count + 1].
// - Precondition: [bytes[count] == '\0'].
PyroStr* PyroStr_take(char* bytes, size_t count, size_t capacity, PyroVM* vm);

// Creates a new string object by concatenating two source strings. Returns NULL if memory
// cannot be allocated for the new string.
PyroStr* PyroStr_concat(PyroStr* s1, PyroStr* s2, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the source string.
// Returns NULL if memory cannot be allocated for the new string.
PyroStr* PyroStr_concat_n_copies(PyroStr* str, size_t n, PyroVM* vm);

// Creates a new string object by concatenating [n] copies of the codepoint encoded as utf-8.
// Returns NULL if memory cannot be allocated for the new string.
PyroStr* PyroStr_concat_n_codepoints_as_utf8(uint32_t codepoint, size_t n, PyroVM* vm);

// Creates a new string object by concatenating two utf-8 encoded codepoints.
// Returns NULL if memory cannot be allocated for the new string.
PyroStr* PyroStr_concat_codepoints_as_utf8(uint32_t cp1, uint32_t cp2, PyroVM* vm);

// Creates a new string by prepending/appending a utf-8 encoded codepoint to an existing
// string. Returns NULL if memory cannot be allocated for the new string.
PyroStr* PyroStr_prepend_codepoint_as_utf8(PyroStr* str, uint32_t codepoint, PyroVM* vm);
PyroStr* PyroStr_append_codepoint_as_utf8(PyroStr* str, uint32_t codepoint, PyroVM* vm);

/* ---- */
/* Maps */
/* ---- */

typedef struct {
    PyroValue key;
    PyroValue value;
} PyroMapEntry;

// A linear-probing hash map. New entries are appended to [entry_array] so iterating over this
// array returns the entries in insertion order.
struct PyroMap {
    PyroObject obj;

    // The number of live entries in the map. In addition to these live entries [entry_array]
    // and [index_array] can contain independently varying numbers of tombstones.
    size_t live_entry_count;

    // This array is the map's data store -- new entries are appended to this array so
    // iterating over it preserves insertion order. [entry_array_count] includes both live
    // entries and tombstones.
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

PyroMap* PyroMap_new(PyroVM* vm);
PyroMap* PyroMap_new_as_set(PyroVM* vm);

// Gets an entry from the map. This function can call into Pyro code via pyro_op_compare_eq()
// and so can set the panic and/or exit flags.
bool PyroMap_get(PyroMap* map, PyroValue key, PyroValue* value, PyroVM* vm);

// Adds a new entry to the map or updates an existing entry. This function can call into Pyro
// code via pyro_op_compare_eq() and so can set the panic and/or exit flags.
// - Returns 0 if the entry was not added because additional memory could not be allocated.
// - Returns 1 if a new entry was successfully added to the map.
// - Returns 2 if an existing entry was successfully updated.
int PyroMap_set(PyroMap* map, PyroValue key, PyroValue value, PyroVM* vm);

// Removes an entry from the map. Returns true if the map contained an entry for [key].
// This function can call into Pyro code via pyro_op_compare_eq() and so can set the panic
// and/or exit flags.
bool PyroMap_remove(PyroMap* map, PyroValue key, PyroVM* vm);

// Returns true if the map contains [key].  This function can call into Pyro code via
// pyro_op_compare_eq() and so can set the panic and/or exit flags.
bool PyroMap_contains(PyroMap* map, PyroValue key, PyroVM* vm);

// Attempts to update an existing entry. Returns [true] if successful, [false] if no
// corresponding entry was found.  This function can call into Pyro code via
// pyro_op_compare_eq() and so can set the panic and/or exit flags.
bool PyroMap_update_entry(PyroMap* map, PyroValue key, PyroValue value, PyroVM* vm);

// Copies all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false]
// if the operation failed because memory could not be allocated for the new entries. The
// operation may fail after some of the entries have successfully been copied.  This function
// can call into Pyro code via pyro_op_compare_eq() and so can set the panic and/or exit
// flags.
bool PyroMap_copy_entries(PyroMap* src, PyroMap* dst, PyroVM* vm);

// Creates a new map object by copying [src]. Returns NULL if sufficient memory cannot be
// allocated for the copy.
PyroMap* PyroMap_copy(PyroMap* src, PyroVM* vm);

// Clears all entries from the map.
void PyroMap_clear(PyroMap* map, PyroVM* vm);

// Optimized methods for maps where all keys are guaranteed to be PyroStr values. These
// functions cannot panic and are safe to call while the VM is in a panicking state.
bool PyroMap_fast_remove(PyroMap* map, PyroStr* key, PyroVM* vm);
bool PyroMap_fast_get(PyroMap* map, PyroStr* key, PyroValue* value, PyroVM* vm);

/* ------- */
/* Vectors */
/* ------- */

struct PyroVec {
    PyroObject obj;
    size_t count;
    size_t capacity;
    PyroValue* values;
};

PyroVec* PyroVec_new(PyroVM* vm);
PyroVec* PyroVec_new_with_capacity(size_t capacity, PyroVM* vm);
PyroVec* PyroVec_new_as_stack(PyroVM* vm);
void PyroVec_clear(PyroVec* vec, PyroVM* vm);

// Returns true if the value was successfully appended, false if memory allocation failed.
bool PyroVec_append(PyroVec* vec, PyroValue value, PyroVM* vm);

// Returns a copy of the [src] vector. Returns NULL if memory allocation fails.
PyroVec* PyroVec_copy(PyroVec* src, PyroVM* vm);

// Appends all entries from [src] to [dst]. Returns [true] if the operation succeeded, [false]
// if the operation failed because enough memory could not be allocated for the extra entries.
bool PyroVec_copy_entries(PyroVec* src, PyroVec* dst, PyroVM* vm);

// Removes and returns the last item from the vector. Panics and returns NULL_VAL if the
// vector is emtpy.
PyroValue PyroVec_remove_last(PyroVec* vec, PyroVM* vm);

// Removes and returns the first item from the vector. Panics and returns NULL_VAL if the
// vector is emtpy.
PyroValue PyroVec_remove_first(PyroVec* vec, PyroVM* vm);

// Removes and returns the item at [index]. Panics and returns NULL_VAL if the index is out of
// range.
PyroValue PyroVec_remove_at_index(PyroVec* vec, size_t index, PyroVM* vm);

// Inserts [value] at [index], where [index] is less than or equal to the vector's item count.
// Panics if [index] is out of range or if memory allocation fails.
void PyroVec_insert_at_index(PyroVec* vec, size_t index, PyroValue value, PyroVM* vm);

/* ------- */
/* Modules */
/* ------- */

struct PyroMod {
    PyroObject obj;

    // Stores the module's global members.
    PyroVec* members;

    // Maps [PyroStr] names to [i64] indexes in the module's [members] vector.
    // - [all_member_indexes] contains indexes for both public and private members.
    // - [pub_member_indexes] contains indexes for only public members.
    PyroMap* all_member_indexes;
    PyroMap* pub_member_indexes;
};

PyroMod* PyroMod_new(PyroVM* vm);

/* -------------- */
/* Pyro Functions */
/* -------------- */

typedef struct {
    PyroObject obj;
    PyroStr* name;
    size_t upvalue_count;

    // The function's [source_id] is used in error messages to identify the function's source.
    // Typically, it will be the name of the file that contained the function.
    PyroStr* source_id;

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
} PyroFn;

PyroFn* PyroFn_new(PyroVM* vm);

// Writes [byte] to the function's bytecode array. Returns [true] if the write succeeded,
// [false] if the write failed because memory could not be allocated.
bool PyroFn_write(PyroFn* fn, uint8_t byte, size_t line_number, PyroVM* vm);

// This method adds a value to the function's constant table and returns its index. If an
// identical value is already present in the table it avoids adding a duplicate and returns
// the index of the existing entry instead. Returns -1 if the operation failed because
// sufficient memory could not be allocated for the constant table.
int64_t PyroFn_add_constant(PyroFn* fn, PyroValue value, PyroVM* vm);

// Returns the length in bytes of the arguments for the opcode at the specified index.
size_t PyroFn_opcode_argcount(PyroFn* fn, size_t ip);

// This method returns the source code line number corresponding to the bytecode instruction
// at index [ip].
size_t PyroFn_get_line_number(PyroFn* fn, size_t ip);

/* ---------------- */
/* Native Functions */
/* ---------------- */

// Signature definition for Pyro functions and methods natively implemented in C.
typedef PyroValue (*pyro_native_fn_t)(PyroVM* vm, size_t arg_count, PyroValue* args);

// [arity = -1] means that the function accepts a variable number of arguments.
typedef struct {
    PyroObject obj;
    PyroStr* name;
    pyro_native_fn_t fn_ptr;
    int arity;
} PyroNativeFn;

PyroNativeFn* PyroNativeFn_new(PyroVM* vm, pyro_native_fn_t fn_ptr, const char* name, int arity);

/* ------- */
/* Classes */
/* ------- */

struct PyroClass {
    PyroObject obj;

    // The class's name, if it has one. This field can be NULL.
    PyroStr* name;

    // The class's superclass, if it has one. This field can be NULL.
    PyroClass* superclass;

    // Instance methods. Maps [PyroStr] names to [PyroClosure] or [PyroNativeFn] values.
    // - [all_instance_methods] contains both public and private instance methods.
    // - [pub_instance_methods] contains only public instance methods.
    PyroMap* all_instance_methods;
    PyroMap* pub_instance_methods;

    // Initialization values for fields. Each new [PyroInstance] gets a copy of this vector.
    PyroVec* default_field_values;

    // Maps [PyroStr] names to [i64] indexes in the instance's [fields] vector.
    // - [all_field_indexes] contains indexes for both public and private fields.
    // - [pub_field_indexes] contains indexes for only public fields.
    PyroMap* all_field_indexes;
    PyroMap* pub_field_indexes;

    // Static members.
    PyroMap* static_methods;
    PyroMap* static_fields;

    // Cached results from the last method lookups.
    PyroStr* all_instance_methods_cached_name;
    PyroValue all_instance_methods_cached_value;
    PyroStr* pub_instance_methods_cached_name;
    PyroValue pub_instance_methods_cached_value;

    // If the class has an $init() method, we cache it here to avoid map lookups.
    PyroValue init_method;
};

PyroClass* PyroClass_new(PyroVM* vm);

/* --------- */
/* Instances */
/* --------- */

typedef struct {
    PyroObject obj;
    PyroValue fields[];
} PyroInstance;

PyroInstance* PyroInstance_new(PyroVM* vm, PyroClass* class);

/* -------- */
/* Upvalues */
/* -------- */

// The [next] pointer is used by the VM to maintain a linked list of open upvalues pointing to
// variables still on the stack.
typedef struct PyroUpvalue {
    PyroObject obj;
    PyroValue* location;
    PyroValue closed;
    struct PyroUpvalue* next;
} PyroUpvalue;

PyroUpvalue* PyroUpvalue_new(PyroVM* vm, PyroValue* slot);

/* -------- */
/* Closures */
/* -------- */

typedef struct {
    PyroObject obj;
    PyroFn* fn;
    PyroMod* module;
    PyroVec* default_values;
    PyroUpvalue** upvalues;
    size_t upvalue_count;
} PyroClosure;

PyroClosure* PyroClosure_new(PyroVM* vm, PyroFn* func, PyroMod* module);

/* ------------- */
/* Bound Methods */
/* ------------- */

typedef struct {
    PyroObject obj;
    PyroValue receiver;
    PyroObject* method; // PyroClosure or PyroNativeFn
} PyroBoundMethod;

PyroBoundMethod* PyroBoundMethod_new(PyroVM* vm, PyroValue receiver, PyroObject* method);

/* ------ */
/* Tuples */
/* ------ */

typedef struct {
    PyroObject obj;
    size_t count;
    PyroValue values[];
} PyroTup;

PyroTup* PyroTup_new(size_t count, PyroVM* vm);
bool PyroTup_check_equal(PyroTup* a, PyroTup* b, PyroVM* vm);

/* ------- */
/* Buffers */
/* ------- */

typedef struct {
    PyroObject obj;
    size_t count;
    size_t capacity;
    uint8_t* bytes;
} PyroBuf;

PyroBuf* PyroBuf_new(PyroVM* vm);
PyroBuf* PyroBuf_new_with_capacity(size_t capacity, PyroVM* vm);
PyroBuf* PyroBuf_new_from_string(PyroStr* string, PyroVM* vm);

// Clears the buffer, i.e. frees the [bytes] array and sets [count] and [capacity] to zero.
void PyroBuf_clear(PyroBuf* buf, PyroVM* vm);

// Resizes the buffer's capacity to [new_capacity].
// - Returns true on success.
// - Returns false if memory allocation fails. In this case, the buffer is unchanged.
// - Requires: [new_capacity >= count].
bool PyroBuf_resize_capacity(PyroBuf* buf, size_t new_capacity, PyroVM* vm);

bool PyroBuf_append_byte(PyroBuf* buf, uint8_t byte, PyroVM* vm);
bool PyroBuf_append_bytes(PyroBuf* buf, size_t count, uint8_t* bytes, PyroVM* vm);

// Appends a byte value to the buffer as a hex-escaped string: \x##.
bool PyroBuf_append_hex_escaped_byte(PyroBuf* buf, uint8_t byte, PyroVM* vm);

// Writes a printf-style formatted string to the buffer. Returns the number of bytes written
// if the entire string can be written to the buffer. Otherwise writes nothing to the buffer
// and returns -1.
int64_t PyroBuf_write_f(PyroBuf* buf, PyroVM* vm, const char* format_string, ...);
int64_t PyroBuf_write_fv(PyroBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// Writes a printf-style formatted string to the buffer. If sufficient memory cannot be
// allocated to write the entire string, writes as much of the string as possible.
void PyroBuf_try_write_fv(PyroBuf* buf, PyroVM* vm, const char* format_string, va_list args);

// This function converts the contents of the buffer into a string, leaving a valid but empty
// buffer behind. Returns NULL if memory cannot be allocated for the new string object -- in
// this case the buffer is unchanged.
PyroStr* PyroBuf_to_str(PyroBuf* buf, PyroVM* vm);

/* ----- */
/* Files */
/* ----- */

struct PyroFile {
    PyroObject obj;
    FILE* stream; // May be NULL.
    PyroStr* path; // May be NULL.
};

PyroFile* PyroFile_new(PyroVM* vm, FILE* stream);

// Reads the next line from the file and returns it as a string, stripping the terminating LF
// or CRLF. Returns NULL on EOF. Will panic and return NULL if memory allocation fails or if
// an I/O read error occurs.
PyroStr* PyroFile_read_line(PyroFile* file, PyroVM* vm);

// Reads the file into a a buffer. Panics and returns NULL if an error occurs.
PyroBuf* PyroFile_read(PyroFile* file, const char* err_prefix, PyroVM* vm);

/* ------ */
/* Queues */
/* ------ */

typedef struct PyroQueueItem {
    PyroValue value;
    struct PyroQueueItem* next;
} PyroQueueItem;

typedef struct {
    PyroObject obj;
    PyroQueueItem* head;
    PyroQueueItem* tail;
    size_t count;
} PyroQueue;

// Creates a new queue object. Returns NULL if memory allocation failed.
PyroQueue* PyroQueue_new(PyroVM* vm);

// Returns true if the value was successfully enqueued, false if memory allocation failed.
bool PyroQueue_enqueue(PyroQueue* queue, PyroValue value, PyroVM* vm);

// Returns true if a value was successfully dequeued, false if the queue was empty.
bool PyroQueue_dequeue(PyroQueue* queue, PyroValue* value, PyroVM* vm);

// Returns the next value without removing it. Returns true on success, false if the queue was
// empty.
bool PyroQueue_peek(PyroQueue* queue, PyroValue* value, PyroVM* vm);

// Clears all entries from the queue.
void PyroQueue_clear(PyroQueue* queue, PyroVM* vm);

/* --------- */
/* Iterators */
/* --------- */

typedef enum {
    PYRO_ITER_EMPTY,
    PYRO_ITER_ENUMERATE,
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
    PyroObject obj;
    PyroObject* source;
    PyroIterType iter_type;
    size_t next_index;
    int64_t next_enum;
    int64_t range_next;
    int64_t range_stop;
    int64_t range_step;
    PyroQueueItem* next_queue_item;
    PyroObject* callback;
} PyroIter;

// Creates a new iterator. Returns NULL if the attempt to allocate memory fails.
PyroIter* PyroIter_new(PyroObject* source, PyroIterType iter_type, PyroVM* vm);

// Creates a new empty iterator. Returns NULL if the attempt to allocate memory fails.
PyroIter* PyroIter_empty(PyroVM* vm);

// Returns the next item from the sequence or an [err] if the sequence has been exhausted.
// Note that this method may call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the return value.
PyroValue PyroIter_next(PyroIter* iter, PyroVM* vm);

// Stringifies the elements returned by the iterator and joins them into a string, separated
// by [sep]. This method can call into Pyro code and can panic or set the exit flag. Check
// [vm->halt_flag] before relying on the result.
PyroStr* PyroIter_join(PyroIter* iter, const char* sep, size_t sep_length, PyroVM* vm);

/* ------------------- */
/*  Resource Pointers  */
/* ------------------- */

// This object type is a wrapper for heap-allocated objects not managed by Pyro's garbage
// collector.  The garbage collector will call the callback function before freeing the
// object.

typedef void (*pyro_free_rp_callback_t)(PyroVM* vm, void* pointer);

typedef struct {
    PyroObject obj;
    void* pointer;
    pyro_free_rp_callback_t callback;
} PyroResourcePointer;

PyroResourcePointer* PyroResourcePointer_new(void* pointer, pyro_free_rp_callback_t callback, PyroVM* vm);

/* -------- */
/*  Errors  */
/* -------- */

typedef struct {
    PyroObject obj;
    PyroStr* message;
    PyroMap* details;
} PyroErr;

PyroErr* PyroErr_new(PyroVM* vm);

#endif
