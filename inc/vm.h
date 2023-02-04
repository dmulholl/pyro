#ifndef pyro_vm_h
#define pyro_vm_h

#include "../lib/mt64/mt64.h"

// We create a new [CallFrame] on the [vm->frames] stack for each function call.
// - [ip] is the instruction pointer -- it points to the next bytecode instruction to be executed.
// - [fp] is the frame pointer -- it points to slot zero on the value stack for the function call.
typedef struct {
    PyroClosure* closure;
    uint8_t* ip;
    PyroValue* fp;
    size_t with_stack_count_on_entry;
} CallFrame;

struct PyroVM {
    // Class objects for builtin types.
    PyroClass* class_str;
    PyroClass* class_map;
    PyroClass* class_tup;
    PyroClass* class_vec;
    PyroClass* class_buf;
    PyroClass* class_file;
    PyroClass* class_iter;
    PyroClass* class_stack;
    PyroClass* class_set;
    PyroClass* class_queue;
    PyroClass* class_err;
    PyroClass* class_module;
    PyroClass* class_char;

    // Halt signal, true if [exit_flag] or [panic_flag] is set.
    bool halt_flag;

    // Exit signal, set by the $exit() function.
    bool exit_flag;

    // Panic signal, set by pyro_panic().
    bool panic_flag;

    // Incremented for every panic, if we have a sequence of panics.
    size_t panic_count;

    // Metadata for the [err] returned by a try-expression.
    PyroStr* panic_source_id;
    size_t panic_line_number;

    // Exit code, defaults to zero. If the $exit() function is called, this will be set to the
    // specified exit code. If a panic is raised, this will be set to a non-zero value.
    int64_t exit_code;

    // Counts the number of nested 'try' expressions.
    size_t try_depth;

    // The VM's standard output stream. This defaults to an PyroFile wrapping stdout.
    // This is the stream that the echo statement and $print() functions write to.
    PyroFile* stdout_file;

    // The VM's standard error stream. This defaults to an PyroFile wrapping stderr.
    // - This is the stream that panic messages are written to.
    // - It's also the stream that the $eprint() functions write to.
    PyroFile* stderr_file;

    // The VM's standard input stream. This defaults to an PyroFile wrapping stdin.
    PyroFile* stdin_file;

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
    PyroMap* superglobals;

    // The tree of imported modules.
    PyroMap* modules;

    // The main module is the context in which script files and the REPL execute.
    PyroMod* main_module;

    // Root directories to check when attempting to import modules.
    PyroVec* import_roots;

    // Buffer for recording panic messages inside 'try' expressions.
    PyroBuf* panic_buffer;

    // Interned string pool.
    PyroMap* string_pool;

    // Linked list of open upvalues pointing to variables still on the stack.
    PyroUpvalue* open_upvalues;

    // Linked list of all heap-allocated objects.
    PyroObject* objects;

    // Canned objects.
    PyroStr* empty_string;
    PyroErr* error;

    // String constants.
    PyroStr* str_dollar_init;
    PyroStr* str_dollar_str;
    PyroStr* str_true;
    PyroStr* str_false;
    PyroStr* str_null;
    PyroStr* str_dollar_fmt;
    PyroStr* str_dollar_iter;
    PyroStr* str_dollar_next;
    PyroStr* str_dollar_get_index;
    PyroStr* str_dollar_set_index;
    PyroStr* str_dollar_debug;
    PyroStr* str_op_binary_equals_equals;
    PyroStr* str_op_binary_less;
    PyroStr* str_op_binary_less_equals;
    PyroStr* str_op_binary_greater;
    PyroStr* str_op_binary_greater_equals;
    PyroStr* str_op_binary_plus;
    PyroStr* str_op_binary_minus;
    PyroStr* str_op_binary_bar;
    PyroStr* str_op_binary_amp;
    PyroStr* str_op_binary_star;
    PyroStr* str_op_binary_slash;
    PyroStr* str_op_binary_caret;
    PyroStr* str_op_binary_percent;
    PyroStr* str_op_binary_star_star;
    PyroStr* str_op_binary_slash_slash;
    PyroStr* str_dollar_hash;
    PyroStr* str_dollar_call;
    PyroStr* str_op_unary_plus;
    PyroStr* str_op_unary_minus;
    PyroStr* str_dollar_contains;
    PyroStr* str_bool;
    PyroStr* str_i64;
    PyroStr* str_f64;
    PyroStr* str_char;
    PyroStr* str_method;
    PyroStr* str_buf;
    PyroStr* str_class;
    PyroStr* str_fn;
    PyroStr* str_instance;
    PyroStr* str_file;
    PyroStr* str_iter;
    PyroStr* str_map;
    PyroStr* str_set;
    PyroStr* str_vec;
    PyroStr* str_stack;
    PyroStr* str_queue;
    PyroStr* str_str;
    PyroStr* str_module;
    PyroStr* str_tup;
    PyroStr* str_err;
    PyroStr* str_dollar_end_with;
    PyroStr* str_source;
    PyroStr* str_line;

    // The grey stack used by the garbage collector.
    PyroObject** grey_stack;
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
