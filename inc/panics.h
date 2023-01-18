#ifndef pyro_panics_h
#define pyro_panics_h

// Pyro follows a SINGLE-PANIC rule -- i.e. if a sequence of panics occurs, we only report the first
// panic. Ideally we would never have a sequence of panics, but this can happen if, for example, we
// have a panic and then while exiting from that panic we execute clean-up code in 3 $end_with()
// methods and the code in 2 of those methods also panics. In this case we only report the original
// panic. Similarly, if a sequence of panics occurs inside a try-expression, the error returned by
// the try-expression will record only the original panic.

// Triggers a panic. This function:
//
// - Sets [vm->panic_flag] to true.
// - Sets [vm->halt_flag] to true.
// - Sets [vm->exit_code] to 1.
// - Increments [vm->panic_count].
//
// If this is the first panic, i.e. if [vm->panic_count == 0], it prints the panic message, or, if
// we're inside a try-expression, writes the panic message to the panic buffer.
void pyro_panic(PyroVM* vm, const char* format, ...);

// Triggers a custom panic for syntax errors. This is only used by the compiler.
void pyro_syntax_error(PyroVM* vm, const char* source_id, size_t line_number, const char* format, ...);

#endif
