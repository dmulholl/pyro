#ifndef pyro_vm_h
#define pyro_vm_h

#include "common.h"
#include "values.h"
#include "../lib/mt64/mt64.h"


// We create a new [CallFrame] on the call stack for each function call. [ip] is the instruction
// pointer -- it points to the next instruction in the function's bytecode to be executed. [fp] is
// the frame pointer -- it points to slot zero on the value stack for the function call.
typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* fp;
} CallFrame;


struct PyroVM {
    // Class objects for builtin types.
    ObjClass* str_class;
    ObjClass* map_class;
    ObjClass* tup_class;
    ObjClass* vec_class;
    ObjClass* buf_class;
    ObjClass* tup_iter_class;
    ObjClass* vec_iter_class;
    ObjClass* map_iter_class;
    ObjClass* str_iter_class;
    ObjClass* range_class;
    ObjClass* file_class;

    // Exit signal, set by the $exit() function.
    bool exit_flag;

    // Signals that an error has occurred.
    bool panic_flag;

    // Signals that the error is unrecoverable.
    bool hard_panic;

    // Halt signal, true if [exit_flag] or [panic_flag] is set.
    bool halt_flag;

    // Exit code, defaults to zero, can be set by $exit() or by the VM itself.
    int exit_code;

    // Counts the number of nested 'try' expressions.
    size_t try_depth;

    // Output stream, defaults to [stdout].
    FILE* out_file;

    // Error stream, defaults to [stderr].
    FILE* err_file;

    // The call stack.
    CallFrame frames[PYRO_MAX_CALL_FRAMES];
    size_t frame_count;

    // The value stack.
    Value stack[PYRO_STACK_SIZE];
    Value* stack_top;
    Value* stack_max;

    // 64-bit Mersenne Twister PRNG.
    MT64* mt64;

    // Stores global functions and variables, available in all modules.
    ObjMap* globals;

    // The tree of imported modules.
    ObjMap* modules;

    // The main module is the context in which script files and the REPL execute.
    ObjModule* main_module;

    // Root directories to check when attempting to import modules.
    ObjVec* import_roots;

    // Buffer for recording panic messages inside 'try' expressions.
    char panic_buffer[PYRO_PANIC_BUFFER_SIZE];

    // Interned string pool.
    ObjMap* strings;

    // This reference will be NULL except when actively compiling source code.
    Parser* parser;

    // Linked list of open upvalues pointing to variables still on the stack.
    ObjUpvalue* open_upvalues;

    // Linked list of all heap-allocated objects.
    Obj* objects;

    // Canned objects.
    ObjTup* empty_error;
    ObjStr* empty_string;
    ObjStr* str_init;
    ObjStr* str_str;
    ObjStr* str_true;
    ObjStr* str_false;
    ObjStr* str_null;
    ObjStr* str_fmt;
    ObjStr* str_iter;
    ObjStr* str_next;
    ObjStr* str_get_index;
    ObjStr* str_set_index;

    // The grey stack used by the garbage collector.
    size_t grey_count;
    size_t grey_capacity;
    Obj** grey_stack;

    // The VM's current memory allocation measured in bytes.
    size_t bytes_allocated;

    // The next garbage collection will trigger when [bytes_allocated] breaches this threshold.
    size_t next_gc_threshold;
};


// Adds a new variable to the VM's globals map.
void pyro_define_global(PyroVM* vm, const char* name, Value value);

// Adds a new native function to the VM's globals map.
void pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity);

// Adds a new variable to the module's globals map.
void pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value member);

// Adds a new native function to the module's globals map.
void pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity);

// Adds a new method to the class.
void pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity);

// Defines a new top-level module.
ObjModule* pyro_define_module_1(PyroVM* vm, const char* name);

// Defines a new 2nd level module where [parent] is a top-level module.
ObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name);

// Defines a new 3rd level module where [grandparent] is a top-level module and [parent] is a
// submodule of [grandparent].
ObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name);

// Calls a method from a native function. Returns the value returned by the method. Before calling
// this function the receiver and the method's arguments should be pushed onto the stack. These
// values (and the return value of the method) will be popped off the stack before this function
// returns. The called method can set the panic or exit flags so the caller should check the
// [vm->halt_flag] immediately on return before using the result. If the halt flag is set the
// caller should clean up any allocated resources and unwind the call stack.
Value pyro_call_method(PyroVM* vm, Value method, uint8_t arg_count);

// Calls a function [fn] from a native function. Returns the value returned by [fn]. Before calling
// this function [fn] itself and its arguments should be pushed onto the stack. These values (and
// the return value of the function) will be popped off the stack before this function returns. The
// called function can set the panic or exit flags so the caller should check the [vm->halt_flag]
// immediately on return before using the result. If the halt flag is set the caller should clean up
// any allocated resources and unwind the call stack.
Value pyro_call_fn(PyroVM* vm, Value fn, uint8_t arg_count);

// Called to signal that an error has occurred.
void pyro_panic(PyroVM* vm, const char* format, ...);

// Writes a printf-style formatted string to the VM's output stream, unless that stream is NULL.
void pyro_out(PyroVM* vm, const char* format, ...);

// Writes a printf-style formatted string to the VM's error stream, unless that stream is NULL.
void pyro_err(PyroVM* vm, const char* format, ...);

// Executes a file in the context of the specified module.
void pyro_exec_file_as_module(PyroVM* vm, const char* path, ObjModule* module);

// Peeks at a value on the stack without popping it. Pass [distance = 0] to peek at the value on
// top of the stack, [distance = 1] to peek at the value below that, etc.
static inline Value pyro_peek(PyroVM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

// Pushes a value onto the stack.
static inline void pyro_push(PyroVM* vm, Value value) {
    if (vm->stack_top == vm->stack_max) {
        pyro_panic(vm, "Stack overflow.");
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}

// Pops the top value from the stack.
static inline Value pyro_pop(PyroVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

#endif
