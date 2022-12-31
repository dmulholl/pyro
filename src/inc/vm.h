#ifndef pyro_vm_h
#define pyro_vm_h

#include "pyro.h"
#include "values.h"
#include "objects.h"
#include "utils.h"
#include "panics.h"

#include "../../lib/mt64/mt64.h"

// We create a new [CallFrame] on the [vm->frames] stack for each function call.
// - [ip] is the instruction pointer -- it points to the next bytecode instruction to be executed.
// - [fp] is the frame pointer -- it points to slot zero on the value stack for the function call.
typedef struct {
    PyroObjClosure* closure;
    uint8_t* ip;
    PyroValue* fp;
    size_t with_stack_count_on_entry;
} CallFrame;

struct PyroVM {
    // Class objects for builtin types.
    PyroObjClass* class_str;
    PyroObjClass* class_map;
    PyroObjClass* class_tup;
    PyroObjClass* class_vec;
    PyroObjClass* class_buf;
    PyroObjClass* class_file;
    PyroObjClass* class_iter;
    PyroObjClass* class_stack;
    PyroObjClass* class_set;
    PyroObjClass* class_queue;
    PyroObjClass* class_err;
    PyroObjClass* class_module;
    PyroObjClass* class_char;

    // Halt signal, true if [exit_flag] or [panic_flag] is set.
    bool halt_flag;

    // Exit signal, set by the $exit() function.
    bool exit_flag;

    // Panic signal, set by pyro_panic().
    bool panic_flag;

    // Incremented for every panic, if we have a sequence of panics.
    size_t panic_count;

    // Metadata for the [err] returned by a try-expression.
    PyroObjStr* panic_source_id;
    size_t panic_line_number;

    // Exit code, defaults to zero. If the $exit() function is called, this will be set to the
    // specified exit code. If a panic is raised, this will be set to a non-zero value.
    int64_t exit_code;

    // Counts the number of nested 'try' expressions.
    size_t try_depth;

    // The VM's standard output stream. This defaults to an PyroObjFile wrapping stdout.
    // This is the stream that the echo statement and $print() functions write to.
    PyroObjFile* stdout_file;

    // The VM's standard error stream. This defaults to an PyroObjFile wrapping stderr.
    // - This is the stream that panic messages are written to.
    // - It's also the stream that the $eprint() functions write to.
    PyroObjFile* stderr_file;

    // The VM's standard input stream. This defaults to an PyroObjFile wrapping stdin.
    PyroObjFile* stdin_file;

    // The call stack.
    CallFrame* frames;
    size_t frame_count;
    size_t frame_capacity;

    // The value stack.
    PyroValue* stack;
    PyroValue* stack_top;
    PyroValue* stack_max;
    size_t stack_size;

    // 64-bit Mersenne Twister PRNG.
    MT64 mt64;

    // Stores global functions and variables, available in all modules.
    PyroObjMap* superglobals;

    // The tree of imported modules.
    PyroObjMap* modules;

    // The main module is the context in which script files and the REPL execute.
    PyroObjModule* main_module;

    // Root directories to check when attempting to import modules.
    PyroObjVec* import_roots;

    // Buffer for recording panic messages inside 'try' expressions.
    PyroObjBuf* panic_buffer;

    // Interned string pool.
    PyroObjMap* strings;

    // Linked list of open upvalues pointing to variables still on the stack.
    PyroObjUpvalue* open_upvalues;

    // Linked list of all heap-allocated objects.
    PyroObj* objects;

    // Canned objects.
    PyroObjStr* empty_string;
    PyroObjErr* error;

    // String constants.
    PyroObjStr* str_dollar_init;
    PyroObjStr* str_dollar_str;
    PyroObjStr* str_true;
    PyroObjStr* str_false;
    PyroObjStr* str_null;
    PyroObjStr* str_dollar_fmt;
    PyroObjStr* str_dollar_iter;
    PyroObjStr* str_dollar_next;
    PyroObjStr* str_dollar_get_index;
    PyroObjStr* str_dollar_set_index;
    PyroObjStr* str_dollar_debug;
    PyroObjStr* str_op_binary_equals_equals;
    PyroObjStr* str_op_binary_less;
    PyroObjStr* str_op_binary_less_equals;
    PyroObjStr* str_op_binary_greater;
    PyroObjStr* str_op_binary_greater_equals;
    PyroObjStr* str_op_binary_plus;
    PyroObjStr* str_op_binary_minus;
    PyroObjStr* str_op_binary_bar;
    PyroObjStr* str_op_binary_amp;
    PyroObjStr* str_op_binary_star;
    PyroObjStr* str_op_binary_slash;
    PyroObjStr* str_op_binary_caret;
    PyroObjStr* str_op_binary_percent;
    PyroObjStr* str_op_binary_star_star;
    PyroObjStr* str_op_binary_slash_slash;
    PyroObjStr* str_dollar_hash;
    PyroObjStr* str_dollar_call;
    PyroObjStr* str_op_unary_plus;
    PyroObjStr* str_op_unary_minus;
    PyroObjStr* str_dollar_contains;
    PyroObjStr* str_bool;
    PyroObjStr* str_i64;
    PyroObjStr* str_f64;
    PyroObjStr* str_char;
    PyroObjStr* str_method;
    PyroObjStr* str_buf;
    PyroObjStr* str_class;
    PyroObjStr* str_fn;
    PyroObjStr* str_instance;
    PyroObjStr* str_file;
    PyroObjStr* str_iter;
    PyroObjStr* str_map;
    PyroObjStr* str_set;
    PyroObjStr* str_vec;
    PyroObjStr* str_stack;
    PyroObjStr* str_queue;
    PyroObjStr* str_str;
    PyroObjStr* str_module;
    PyroObjStr* str_tup;
    PyroObjStr* str_err;
    PyroObjStr* str_dollar_end_with;
    PyroObjStr* str_source;
    PyroObjStr* str_line;

    // The grey stack used by the garbage collector.
    PyroObj** grey_stack;
    size_t grey_stack_count;
    size_t grey_stack_capacity;

    // The VM's current memory allocation measured in bytes.
    size_t bytes_allocated;

    // The VM's maximum allowed memory allocation measured in bytes.
    size_t max_bytes;

    // The next garbage collection will trigger when [bytes_allocated] breaches this threshold.
    size_t next_gc_threshold;

    // This flag starts off false. It gets toggled to true if an attempt to allocate memory fails.
    bool memory_allocation_failed;

    // This acts like a stack of disallow tokens for the garbage collector. To disallow garbage
    // collection, add 1. To restore the previous state, subtract 1. The garbage collector can
    // only run when this value is 0.
    int gc_disallows;

    // Set to true if the VM executing in a REPL.
    bool in_repl;

    // Stack of values with pending $end_with() method calls.
    PyroValue* with_stack;
    size_t with_stack_count;
    size_t with_stack_capacity;
};


// Peeks at a value on the stack without popping it. Pass [distance = 0] to peek at the value on
// top of the stack, [distance = 1] to peek at the value below that, etc.
static inline PyroValue pyro_peek(PyroVM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

// Pushes a value onto the stack. Panics if the stack overflows.
static inline bool pyro_push(PyroVM* vm, PyroValue value) {
    if (vm->stack_top == vm->stack_max) {
        pyro_panic(vm, "stack overflow");
        return false;
    }
    *vm->stack_top = value;
    vm->stack_top++;
    return true;
}

// Pops the top value from the stack.
static inline PyroValue pyro_pop(PyroVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

#endif
