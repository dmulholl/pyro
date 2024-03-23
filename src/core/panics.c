#include "../includes/pyro.h"


static void print_stack_trace(PyroVM* vm) {
    pyro_stderr_write(vm, "\nStack Trace:\n\n");

    for (size_t i = vm->call_stack_count; i > 0; i--) {
        PyroCallFrame* frame = &vm->call_stack[i - 1];
        PyroFn* fn = frame->closure->fn;

        size_t line_number = 1;
        if (frame->ip > fn->code) {
            size_t ip = frame->ip - fn->code - 1;
            line_number = PyroFn_get_line_number(fn, ip);
        }

        pyro_stderr_write_f(vm, "%s:%zu\n", fn->source_id->bytes, line_number);
        pyro_stderr_write_f(vm, "  [%zu] --> in %s\n", i, fn->name->bytes);
    }
}


static void handle_panic(
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
    PyroStr* last_opcode_source_id = NULL;
    size_t last_opcode_line_number = 0;

    if (vm->call_stack_count > 0) {
        PyroCallFrame* current_frame = &vm->call_stack[vm->call_stack_count - 1];
        PyroFn* current_fn = current_frame->closure->fn;
        last_opcode_source_id = current_fn->source_id;
        last_opcode_line_number = 1;
        if (current_frame->ip > current_fn->code) {
            size_t current_ip = current_frame->ip - current_fn->code - 1;
            last_opcode_line_number = PyroFn_get_line_number(current_fn, current_ip);
        } else if (vm->call_stack_count > 1) {
            PyroCallFrame* outer_frame = &vm->call_stack[vm->call_stack_count - 2];
            PyroFn* outer_fn = outer_frame->closure->fn;
            size_t outer_ip = outer_frame->ip - outer_fn->code - 1;
            last_opcode_source_id = outer_fn->source_id;
            last_opcode_line_number = PyroFn_get_line_number(outer_fn, outer_ip);
        }
    }

    // If we're inside a try expression, write the error message to the panic buffer.
    if (vm->try_depth > 0) {
        PyroBuf_try_write_fv(vm->panic_buffer, vm, format_string, args);

        if (syntax_error_source_id) {
            vm->panic_line_number = syntax_error_line_number;

            PyroStr* source_id = PyroStr_COPY(syntax_error_source_id);
            if (!source_id) {
                vm->panic_source_id = vm->empty_string;
                return;
            }

            vm->panic_source_id = source_id;
            return;
        }

        vm->panic_source_id = last_opcode_source_id;
        vm->panic_line_number = last_opcode_line_number;
        return;
    }

    // Special handling for syntax errors.
    if (syntax_error_source_id) {
        pyro_stderr_write_f(vm, "%s:%zu\n  syntax error: ", syntax_error_source_id, syntax_error_line_number);
        pyro_stderr_write_fv(vm, format_string, args);
        pyro_stderr_write(vm, "\n");

        if (vm->call_stack_count > 0) {
            pyro_stderr_write_f(vm,
                "\n%s:%zu\n  panic: invalid source code\n",
                last_opcode_source_id->bytes,
                last_opcode_line_number
            );
        }

        if (vm->call_stack_count > 1) {
            print_stack_trace(vm);
        }

        return;
    }

    if (vm->call_stack_count > 0) {
        pyro_stderr_write_f(vm, "%s:%zu\n  ", last_opcode_source_id->bytes, last_opcode_line_number);
    }

    pyro_stderr_write(vm, "panic: ");
    pyro_stderr_write_fv(vm, format_string, args);
    pyro_stderr_write(vm, "\n");

    if (vm->call_stack_count > 1) {
        print_stack_trace(vm);
    }
}


void pyro_panic(PyroVM* vm, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    handle_panic(vm, format_string, args, NULL, 0);
    va_end(args);
}


void pyro_panic_with_syntax_error(PyroVM* vm, const char* source_id, size_t line_number, const char* format_string, ...) {
    va_list args;
    va_start(args, format_string);
    handle_panic(vm, format_string, args, source_id, line_number);
    va_end(args);
}
