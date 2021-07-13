#ifndef pyro_vm_h
#define pyro_vm_h

#include "common.h"
#include "values.h"
#include "../lib/mt64/mt64.h"


typedef struct {
    ObjClosure* closure;
    uint8_t* ip; // instruction pointer, points to the next instruction
    Value* fp;   // frame pointer, points to slot zero on the value stack for the current call
} CallFrame;


struct PyroVM {
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

    bool halt_flag;
    bool exit_flag;
    bool panic_flag;
    bool memory_error_flag;
    int exit_code;
    int try_depth;

    FILE* out_file;
    FILE* err_file;

    CallFrame frames[PYRO_MAX_CALL_FRAMES];
    size_t frame_count;

    Value stack[PYRO_STACK_SIZE];
    Value* stack_top;
    Value* stack_max;

    // 64-bit Mersenne Twister PRNG.
    MT64* mt64;

    ObjMap* globals;
    ObjMap* modules;
    ObjModule* main_module;
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

    // The grey set for the garbage collector.
    int grey_count;
    int grey_capacity;
    Obj** grey_stack; // not yet counted in bytes_allocated

    size_t bytes_allocated;
    size_t next_gc_threshold;
};


void pyro_define_global(PyroVM* vm, const char* name, Value value);
void pyro_define_global_fn(PyroVM* vm, const char* name, NativeFn fn_ptr, int arity);

void pyro_define_member(PyroVM* vm, ObjModule* module, const char* name, Value member);
void pyro_define_member_fn(PyroVM* vm, ObjModule* module, const char* name, NativeFn fn_ptr, int arity);

void pyro_define_method(PyroVM* vm, ObjClass* class, const char* name, NativeFn fn_ptr, int arity);

ObjModule* pyro_define_module_1(PyroVM* vm, const char* name);
ObjModule* pyro_define_module_2(PyroVM* vm, const char* parent, const char* name);
ObjModule* pyro_define_module_3(PyroVM* vm, const char* grandparent, const char* parent, const char* name);

Value pyro_call_method(PyroVM* vm, Value method, int arg_count);

void pyro_panic(PyroVM* vm, const char* format, ...);
void pyro_memory_error(PyroVM* vm);

// Write a printf-style formatted string to the VM's output stream, unless that stream is NULL.
void pyro_out(PyroVM* vm, const char* format, ...);

// Write a printf-style formatted string to the VM's error stream, unless that stream is NULL.
void pyro_err(PyroVM* vm, const char* format, ...);


void pyro_exec_file_as_module(PyroVM* vm, const char* path, ObjModule* module);


// Peek at a value on the stack without popping it. Pass [distance = 0] to peek at the value on
// top of the stack, [distance = 1] to peek at the value below that, etc.
static inline Value pyro_peek(PyroVM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}


// Push a value onto the stack.
static inline void pyro_push(PyroVM* vm, Value value) {
    if (vm->stack_top == vm->stack_max) {
        pyro_panic(vm, "Stack overflow.");
        return;
    }
    *vm->stack_top = value;
    vm->stack_top++;
}


// Pop the top value from the stack.
static inline Value pyro_pop(PyroVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

#endif
