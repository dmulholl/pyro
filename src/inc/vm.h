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
    ObjClosure* closure;
    uint8_t* ip;
    Value* fp;
    size_t with_stack_count_on_entry;
} CallFrame;

struct PyroVM {
    // Class objects for builtin types.
    ObjClass* class_str;
    ObjClass* class_map;
    ObjClass* class_tup;
    ObjClass* class_vec;
    ObjClass* class_buf;
    ObjClass* class_file;
    ObjClass* class_iter;
    ObjClass* class_stack;
    ObjClass* class_set;
    ObjClass* class_queue;
    ObjClass* class_err;
    ObjClass* class_module;
    ObjClass* class_char;

    // Halt signal, true if [exit_flag] or [panic_flag] is set.
    bool halt_flag;

    // Exit signal, set by the $exit() function.
    bool exit_flag;

    // Panic signal, set by pyro_panic().
    bool panic_flag;

    // Incremented for every panic, if we have a sequence of panics.
    size_t panic_count;

    // Metadata for the [err] returned by a try-expression.
    ObjStr* panic_source_id;
    size_t panic_line_number;

    // Exit code, defaults to zero. If the $exit() function is called, this will be set to the
    // specified exit code. If a panic is raised, this will be set to a non-zero value.
    int64_t exit_code;

    // Counts the number of nested 'try' expressions.
    size_t try_depth;

    // The VM's standard output stream. This defaults to an ObjFile wrapping stdout.
    // This is the stream that the echo statement and $print() functions write to.
    Obj* stdout_stream;

    // The VM's standard error stream. This defaults to an ObjFile wrapping stderr.
    // This is the stream that panic messages are written to. It's also the stream that the
    // $eprint() functions write to.
    Obj* stderr_stream;

    // The VM's standard input stream. This defaults to an ObjFile wrapping stdin.
    ObjFile* stdin_file;

    // The call stack.
    CallFrame* frames;
    size_t frame_count;
    size_t frame_capacity;

    // The value stack.
    Value* stack;
    Value* stack_top;
    Value* stack_max;
    size_t stack_size;

    // 64-bit Mersenne Twister PRNG.
    MT64 mt64;

    // Stores global functions and variables, available in all modules.
    ObjMap* superglobals;

    // The tree of imported modules.
    ObjMap* modules;

    // The main module is the context in which script files and the REPL execute.
    ObjModule* main_module;

    // Root directories to check when attempting to import modules.
    ObjVec* import_roots;

    // Buffer for recording panic messages inside 'try' expressions.
    ObjBuf* panic_buffer;

    // Interned string pool.
    ObjMap* strings;

    // Linked list of open upvalues pointing to variables still on the stack.
    ObjUpvalue* open_upvalues;

    // Linked list of all heap-allocated objects.
    Obj* objects;

    // Canned objects.
    ObjStr* empty_string;
    ObjErr* error;

    // String constants.
    ObjStr* str_dollar_init;
    ObjStr* str_dollar_str;
    ObjStr* str_true;
    ObjStr* str_false;
    ObjStr* str_null;
    ObjStr* str_dollar_fmt;
    ObjStr* str_dollar_iter;
    ObjStr* str_dollar_next;
    ObjStr* str_dollar_get_index;
    ObjStr* str_dollar_set_index;
    ObjStr* str_dollar_debug;
    ObjStr* str_op_binary_equals_equals;
    ObjStr* str_op_binary_less;
    ObjStr* str_op_binary_less_equals;
    ObjStr* str_op_binary_greater;
    ObjStr* str_op_binary_greater_equals;
    ObjStr* str_op_binary_plus;
    ObjStr* str_op_binary_minus;
    ObjStr* str_op_binary_bar;
    ObjStr* str_op_binary_amp;
    ObjStr* str_op_binary_star;
    ObjStr* str_op_binary_slash;
    ObjStr* str_op_binary_caret;
    ObjStr* str_op_binary_percent;
    ObjStr* str_op_binary_star_star;
    ObjStr* str_op_binary_slash_slash;
    ObjStr* str_dollar_hash;
    ObjStr* str_dollar_call;
    ObjStr* str_op_unary_plus;
    ObjStr* str_op_unary_minus;
    ObjStr* str_dollar_contains;
    ObjStr* str_bool;
    ObjStr* str_i64;
    ObjStr* str_f64;
    ObjStr* str_char;
    ObjStr* str_method;
    ObjStr* str_buf;
    ObjStr* str_class;
    ObjStr* str_fn;
    ObjStr* str_instance;
    ObjStr* str_file;
    ObjStr* str_iter;
    ObjStr* str_map;
    ObjStr* str_set;
    ObjStr* str_vec;
    ObjStr* str_stack;
    ObjStr* str_queue;
    ObjStr* str_str;
    ObjStr* str_module;
    ObjStr* str_tup;
    ObjStr* str_err;
    ObjStr* str_dollar_end_with;
    ObjStr* str_source;
    ObjStr* str_line;

    // The grey stack used by the garbage collector.
    Obj** grey_stack;
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
    Value* with_stack;
    size_t with_stack_count;
    size_t with_stack_capacity;
};


// Peeks at a value on the stack without popping it. Pass [distance = 0] to peek at the value on
// top of the stack, [distance = 1] to peek at the value below that, etc.
static inline Value pyro_peek(PyroVM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

// Pushes a value onto the stack. Panics if the stack overflows.
static inline bool pyro_push(PyroVM* vm, Value value) {
    if (vm->stack_top == vm->stack_max) {
        pyro_panic(vm, "stack overflow");
        return false;
    }
    *vm->stack_top = value;
    vm->stack_top++;
    return true;
}

// Pops the top value from the stack.
static inline Value pyro_pop(PyroVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

#endif
