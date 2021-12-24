#include "panics.h"
#include "vm.h"


static void print_stack_trace(PyroVM* vm) {
    if (vm->err_file) {
        fprintf(vm->err_file, "\nTraceback (most recent function call first):\n\n");

        for (size_t i = vm->frame_count; i > 0; i--) {
            CallFrame* frame = &vm->frames[i - 1];
            ObjFn* fn = frame->closure->fn;

            size_t line_number = 1;
            if (frame->ip > fn->code) {
                size_t ip = frame->ip - fn->code - 1;
                line_number = ObjFn_get_line_number(fn, ip);
            }

            fprintf(vm->err_file, "%s:%zu\n", fn->source->bytes, line_number);
            fprintf(vm->err_file, "  [%zu] --> in %s\n", i, fn->name->bytes);
        }
    }
}


void pyro_panic(PyroVM* vm, int64_t error_code, const char* format, ...) {
    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->status_code = error_code;

    // If we're inside a try expression and the panic is catchable, we don't print anything to the
    // error stream. We simply store the error message in the panic buffer so it can be returned
    // by the try expression.
    if (vm->try_depth > 0 && !vm->hard_panic) {
        va_list args;
        va_start(args, format);
        vsnprintf(vm->panic_buffer, PYRO_PANIC_BUFFER_SIZE, format, args);
        va_end(args);
        return;
    }

    // If we were executing Pyro code when the panic occured, print the source filename and line
    // number of the last instruction.
    if (vm->frame_count > 0 && vm->err_file) {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        ObjFn* fn = frame->closure->fn;
        size_t line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = ObjFn_get_line_number(fn, ip);
        }
        if (vm->in_repl) {
            fprintf(vm->err_file, "line:%zu\n  ", line_number);
        } else {
            fprintf(vm->err_file, "%s:%zu\n  ", fn->source->bytes, line_number);
        }
    }

    // Print the error message.
    if (vm->err_file) {
        fprintf(vm->err_file, "Error: ");
        va_list args;
        va_start(args, format);
        vfprintf(vm->err_file, format, args);
        va_end(args);
        fprintf(vm->err_file, "\n");
    }

    // Print a stack trace if the panic occurred inside a Pyro function.
    if (vm->frame_count > 1 && vm->err_file) {
        print_stack_trace(vm);
    }
}


void pyro_syntax_error(PyroVM* vm, const char* source_id, size_t source_line, const char* format, ...) {
    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->status_code = ERR_SYNTAX_ERROR;

    // If we're inside a try expression and the panic is catchable, we don't print anything to the
    // error stream. We simply store the error message in the panic buffer so it can be returned
    // by the try expression.
    if (vm->try_depth > 0 && !vm->hard_panic) {
        va_list args;
        va_start(args, format);
        vsnprintf(vm->panic_buffer, PYRO_PANIC_BUFFER_SIZE, format, args);
        va_end(args);
        return;
    }

    // Print the syntax error message.
    if (vm->err_file) {
        if (vm->in_repl) {
            fprintf(vm->err_file, "line:%zu\n  Syntax Error: ", source_line);
        } else {
            fprintf(vm->err_file, "%s:%zu\n  Syntax Error: ", source_id, source_line);
        }
        va_list args;
        va_start(args, format);
        vfprintf(vm->err_file, format, args);
        va_end(args);
        fprintf(vm->err_file, "\n");
    }

    // If we were executing Pyro code when the error occured, print the source filename and line
    // number of the last instruction.
    if (vm->frame_count > 0 && vm->err_file) {
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        ObjFn* fn = frame->closure->fn;
        size_t instruction_line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            instruction_line_number = ObjFn_get_line_number(fn, ip);
        }
        fprintf(
            vm->err_file,
            "\n%s:%zu\n  Error: Syntax error.\n",
            fn->source->bytes,
            instruction_line_number
        );
    }

    // Print a stack trace if the error occurred inside a Pyro function.
    if (vm->frame_count > 1 && vm->err_file) {
        print_stack_trace(vm);
    }
}
