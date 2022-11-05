#include "../inc/panics.h"
#include "../inc/vm.h"
#include "../inc/io.h"


static void print_stack_trace(PyroVM* vm) {
    pyro_stderr_write(vm, "\nStack Trace:\n\n");

    for (size_t i = vm->frame_count; i > 0; i--) {
        CallFrame* frame = &vm->frames[i - 1];
        ObjPyroFn* fn = frame->closure->fn;

        size_t line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = ObjPyroFn_get_line_number(fn, ip);
        }

        pyro_stderr_write_f(vm, "%s:%zu\n", fn->source_id->bytes, line_number);
        pyro_stderr_write_f(vm, "  [%zu] --> in %s\n", i, fn->name->bytes);
    }
}


static void panic(
    PyroVM* vm,
    const char* format_string,
    va_list args,
    const char* syntax_error_source_id,
    size_t syntax_error_line_number
) {
    vm->panic_flag = true;
    vm->halt_flag = true;
    vm->exit_code = 1;

    vm->panic_count++;
    if (vm->panic_count > 1) {
        return;
    }

    // If we were executing Pyro code when the panic occured, determine the source ID and line
    // number of the last instruction.
    ObjStr* last_opcode_source_id = NULL;
    size_t last_opcode_line_number = 0;
    if (vm->frame_count > 0) {
        CallFrame* current_frame = &vm->frames[vm->frame_count - 1];
        ObjPyroFn* current_fn = current_frame->closure->fn;
        last_opcode_source_id = current_fn->source_id;
        last_opcode_line_number = 1;
        if (current_frame->ip > current_fn->code) {
            size_t current_ip = current_frame->ip - current_fn->code - 1;
            last_opcode_line_number = ObjPyroFn_get_line_number(current_fn, current_ip);
        } else if (vm->frame_count > 1) {
            CallFrame* outer_frame = &vm->frames[vm->frame_count - 2];
            ObjPyroFn* outer_fn = outer_frame->closure->fn;
            size_t outer_ip = outer_frame->ip - outer_fn->code - 1;
            last_opcode_source_id = outer_fn->source_id;
            last_opcode_line_number = ObjPyroFn_get_line_number(outer_fn, outer_ip);
        }
    }

    // If we're inside a try expression, write the error message to the panic buffer.
    if (vm->try_depth > 0) {
        ObjBuf_try_write_fv(vm->panic_buffer, vm, format_string, args);
        vm->panic_source_id = last_opcode_source_id;
        vm->panic_line_number = last_opcode_line_number;
        return;
    }

    // Special handling for syntax errors.
    if (syntax_error_source_id) {
        pyro_stderr_write_f(vm, "%s:%zu\n  syntax error: ", syntax_error_source_id, syntax_error_line_number);
        pyro_stderr_write_fv(vm, format_string, args);
        pyro_stderr_write(vm, "\n");
        if (vm->frame_count > 0) {
            pyro_stderr_write_f(vm,
                "\n%s:%zu\n  error: failed to compile Pyro source code\n",
                last_opcode_source_id->bytes,
                last_opcode_line_number
            );
        }
        if (vm->frame_count > 1) {
            print_stack_trace(vm);
        }
        return;
    }

    if (vm->frame_count > 0) {
        pyro_stderr_write_f(vm, "%s:%zu\n  ", last_opcode_source_id->bytes, last_opcode_line_number);
    }

    pyro_stderr_write(vm, "error: ");
    pyro_stderr_write_fv(vm, format_string, args);
    pyro_stderr_write(vm, "\n");

    if (vm->frame_count > 1) {
        print_stack_trace(vm);
    }
}


void pyro_panic(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    panic(vm, format_string, args, NULL, 0);
    va_end(args);
}


void pyro_syntax_error(PyroVM* vm, const char* source_id, size_t line_number, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    panic(vm, format_string, args, source_id, line_number);
    va_end(args);
}
