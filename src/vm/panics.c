#include "../inc/panics.h"
#include "../inc/vm.h"
#include "../inc/io.h"


static void print_stack_trace(PyroVM* vm) {
    pyro_stderr_write(vm, "\nTraceback (most recent function call first):\n\n");

    for (size_t i = vm->frame_count; i > 0; i--) {
        CallFrame* frame = &vm->frames[i - 1];
        ObjFn* fn = frame->closure->fn;

        size_t line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = ObjFn_get_line_number(fn, ip);
        }

        pyro_stderr_write_f(vm, "%s:%zu\n", fn->source_id->bytes, line_number);
        pyro_stderr_write_f(vm, "  [%zu] --> in %s\n", i, fn->name->bytes);
    }
}


void pyro_panic(PyroVM* vm, const char* format_string, ...) {
    // It's generally a bad sign if we try to raise a panic when we're already in a panic state --
    // it means that we haven't properly handled the result of a fallible operation. On the other
    // hand, sometimes we want the option of running a sequence of fallible operations -- e.g.
    // pushing multiple values onto the stack -- and only checking once for a panic at the end.
    if (vm->panic_flag) {
        #ifdef DEBUG
            pyro_stderr_write(vm, "DEBUG: redundant panic:\n  ");
            va_list args;
            va_start(args, format_string);
            pyro_stderr_write_fv(vm, format_string, args);
            va_end(args);
            pyro_stderr_write(vm, "\n");
            return;
        #else
            return;
        #endif
    }

    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->exit_code = 1;
    vm->panic_line_number = 0;
    vm->panic_source_id = vm->empty_string;

    // If we were executing Pyro code when the panic occured, determine the source ID and line
    // number of the last instruction.
    ObjStr* source_id = NULL;
    size_t line_number = 0;

    if (vm->frame_count > 0) {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        ObjFn* fn = frame->closure->fn;
        source_id = fn->source_id;
        line_number = 1;

        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = ObjFn_get_line_number(fn, ip);
        }

        vm->panic_source_id = fn->source_id;
        vm->panic_line_number = line_number;
    }

    // If we're inside a try expression and the panic is catchable, write the error message to the
    // panic buffer so it can be returned by the try expression.
    if (vm->try_depth > 0 && !vm->hard_panic) {
        va_list args;
        va_start(args, format_string);

        #ifdef DEBUG
            va_list args_copy;
            va_copy(args_copy, args);
            pyro_stderr_write(vm, "DEBUG: writing to panic buffer:\n  ");
            pyro_stderr_write_fv(vm, format_string, args_copy);
            pyro_stderr_write(vm, "\n");
            va_end(args_copy);
        #endif

        ObjBuf_best_effort_write_fv(vm->panic_buffer, vm, format_string, args);
        va_end(args);
        return;
    }

    // If we were executing Pyro code when the panic occured, print the source ID and line
    // number of the last instruction.
    if (vm->frame_count > 0) {
        pyro_stderr_write_f(vm, "%s:%zu\n  ", source_id->bytes, line_number);
    }

    // Print the error message.
    pyro_stderr_write(vm, "Error: ");
    va_list args;
    va_start(args, format_string);
    pyro_stderr_write_fv(vm, format_string, args);
    va_end(args);
    if (strlen(format_string) > 0 && format_string[strlen(format_string) - 1] != '.') {
        pyro_stderr_write(vm, ".\n");
    } else {
        pyro_stderr_write(vm, "\n");
    }

    // Print a stack trace if the panic occurred inside a Pyro function.
    if (vm->frame_count > 1) {
        print_stack_trace(vm);
    }
}


void pyro_syntax_error(PyroVM* vm, const char* source_id, size_t source_line, const char* format_string, ...) {
    if (vm->panic_flag) {
        #ifdef DEBUG
            pyro_stderr_write(vm, "DEBUG: redundant panic:\n  ");
            va_list args;
            va_start(args, format_string);
            pyro_stderr_write_fv(vm, format_string, args);
            va_end(args);
            pyro_stderr_write(vm, "\n");
            return;
        #else
            return;
        #endif
    }

    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->exit_code = 1;
    vm->panic_line_number = source_line;

    vm->panic_source_id = STR(source_id);
    if (!vm->panic_source_id) {
        vm->panic_source_id = vm->empty_string;
    }

    // If we're inside a try expression and the panic is catchable, write the error message to the
    // panic buffer so it can be returned by the try expression.
    if (vm->try_depth > 0 && !vm->hard_panic) {
        va_list args;
        va_start(args, format_string);
        ObjBuf_best_effort_write_fv(vm->panic_buffer, vm, format_string, args);
        va_end(args);
        return;
    }

    // Print the syntax error message.
    pyro_stderr_write_f(vm, "%s:%zu\n  Syntax Error: ", source_id, source_line);
    va_list args;
    va_start(args, format_string);
    pyro_stderr_write_fv(vm, format_string, args);
    va_end(args);
    if (strlen(format_string) > 0 && format_string[strlen(format_string) - 1] != '.') {
        pyro_stderr_write(vm, ".\n");
    } else {
        pyro_stderr_write(vm, "\n");
    }

    // If we were executing Pyro code when the error occured, print the source filename and line
    // number of the last instruction.
    if (vm->frame_count > 0) {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        ObjFn* fn = frame->closure->fn;
        size_t instruction_line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            instruction_line_number = ObjFn_get_line_number(fn, ip);
        }
        pyro_stderr_write_f(
            vm,
            "\n%s:%zu\n  Error: failed to compile Pyro source code.\n",
            fn->source_id->bytes,
            instruction_line_number
        );
    }

    // Print a stack trace if the error occurred inside a Pyro function.
    if (vm->frame_count > 1) {
        print_stack_trace(vm);
    }
}
