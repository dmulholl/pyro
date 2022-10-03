#include "../inc/compiler.h"
#include "../inc/debug.h"
#include "../inc/opcodes.h"
#include "../inc/heap.h"
#include "../inc/vm.h"
#include "../inc/utf8.h"
#include "../inc/utils.h"
#include "../inc/lexer.h"
#include "../inc/panics.h"


/* -------------- */
/*  Error Macros  */
/* -------------- */


// Signals a syntax error at the previous token. The argument is the error message specified as
// a printf-style format string along with any values to be interpolated.
#define ERROR_AT_PREVIOUS_TOKEN(...) \
    do { \
        if (!parser->had_syntax_error) { \
            parser->had_syntax_error = true; \
            Token* token = &parser->previous_token; \
            pyro_syntax_error(parser->vm, parser->src_id, token->line, __VA_ARGS__); \
        } \
    } while (false)


// Signals a syntax error at the next token. The argument is the error message specified as
// a printf-style format string along with any values to be interpolated.
#define ERROR_AT_NEXT_TOKEN(...) \
    do { \
        if (!parser->had_syntax_error) { \
            parser->had_syntax_error = true; \
            Token* token = &parser->next_token; \
            pyro_syntax_error(parser->vm, parser->src_id, token->line, __VA_ARGS__); \
        } \
    } while (false)


/* ------- */
/*  Types  */
/* ------- */


typedef struct {
    Token name;             // name of the variable
    int depth;              // scope depth of the block in which the variable was declared
    bool is_initialized;    // true after the variable has been defined
    bool is_captured;       // true if the local is captured by a closure
} Local;


// If [is_local] is true the upvalue is capturing a local variable from the immediately surrounding
// function; [index] is the slot index of this local. If [is_local] is false the upvalue is
// capturing a chained upvalue from the immediately surrounding function referencing a captured
// local from a higher scope; [index] is the upvalue index of the captured upvalue.
typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;


typedef enum {
    TYPE_FUNCTION,
    TYPE_INIT_METHOD,
    TYPE_METHOD,
    TYPE_STATIC_METHOD,
    TYPE_MODULE,
    TYPE_TRY_EXPR,
} FnType;


typedef struct LoopCompiler {
    int start_index;    // bytecode index of the loop's starting point
    int start_depth;    // scope depth of the loop's starting point
    bool had_break;
    struct LoopCompiler* enclosing;
} LoopCompiler;


typedef struct FnCompiler {
    struct FnCompiler* enclosing;
    ObjFn* fn;
    FnType type;
    Local locals[256];      // 256 as we use a single byte argument to index locals
    int local_count;        // how many local variables are in scope
    int scope_depth;        // number of blocks surrounding the curent code
    Upvalue upvalues[256];
    LoopCompiler* loop_compiler;
} FnCompiler;


typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
    bool has_superclass;
} ClassCompiler;


typedef struct {
    Lexer lexer;
    Token previous_token;
    Token next_token;
    bool had_syntax_error;
    bool had_memory_error;
    PyroVM* vm;
    FnCompiler* fn_compiler;
    ClassCompiler* class_compiler;
    const char* src_id;
    size_t num_statements;
    size_t num_expression_statements;
} Parser;


typedef enum Access {
    PRIVATE,
    PUBLIC,
    STATIC
} Access;


/* -------------------- */
/* Forward Declarations */
/* -------------------- */


static void parse_expression(Parser* parser, bool can_assign, bool can_assign_in_parens);
static void parse_statement(Parser* parser);
static void parse_function_definition(Parser* parser, FnType type, Token name);
static void parse_unary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens);
static void parse_type(Parser* parser);


/* ----------------- */
/* Parsing Utilities */
/* ----------------- */


// Make a synthetic token to pass a hardcoded string around.
static Token syntoken(const char* text) {
    return (Token) {
        .type = TOKEN_SYNTHETIC,
        .line = 0,
        .start = text,
        .length = strlen(text)
    };
}


// Make a synthetic token containing the basename from [source_id].
static Token basename_syntoken(const char* source_id) {
    if (strlen(source_id) == 0) {
        return (Token) {
            .type = TOKEN_SYNTHETIC,
            .line = 0,
            .start = "code",
            .length = strlen("code")
        };
    }

    const char* last_char = source_id + strlen(source_id) - 1;

    // Find the first character of the basename.
    const char* start = last_char;
    while (start > source_id) {
        if (*(start - 1) == '/') {
            break;
        }
        start--;
    }

    // Find the last character of the basename.
    const char* end = start;
    while (end < last_char) {
        /* if (*(end + 1) == '.') { */
        /*     break; */
        /* } */
        end++;
    }

    return (Token) {
        .type = TOKEN_SYNTHETIC,
        .line = 0,
        .start = start,
        .length = end - start + 1
    };
}


static bool lexemes_are_equal(Token* a, Token* b) {
    if (a->length == b->length) {
        return memcmp(a->start, b->start, a->length) == 0;
    }
    return false;
}


// Appends a single byte to the current function's bytecode. All writes to the bytecode go through
// this function.
static void emit_byte(Parser* parser, uint8_t byte) {
    if (!ObjFn_write(parser->fn_compiler->fn, byte, parser->previous_token.line, parser->vm)) {
        parser->had_memory_error = true;
    }
}


// Emits two unsigned 8-bit integers.
static void emit_u8_u8(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emit_byte(parser, byte1);
    emit_byte(parser, byte2);
}


// Emits an unsigned 16-bit integer in big-endian format.
static void emit_u16be(Parser* parser, uint16_t value) {
    emit_byte(parser, (value >> 8) & 0xff);
    emit_byte(parser, value & 0xff);
}


// Emits an unsigned 8-bit integer followed by an unsigned 16-bit integer in big-endian format.
// (Typically an opcode and its argument.)
static void emit_u8_u16be(Parser* parser, uint8_t u8_arg, uint16_t u16_arg) {
    emit_byte(parser, u8_arg);
    emit_u16be(parser, u16_arg);
}


// Emits a naked return instruction -- i.e. a return with no value specified. Inside an $init()
// method this returns the object instance being initialized, otherwise it returns null.
static void emit_naked_return(Parser* parser) {
    if (parser->fn_compiler->type == TYPE_INIT_METHOD) {
        emit_u8_u8(parser, OP_GET_LOCAL, 0);
    } else {
        emit_byte(parser, OP_LOAD_NULL);
    }
    emit_byte(parser, OP_RETURN);
}


// Adds the value to the current function's constant table and returns its index.
static uint16_t add_value_to_constant_table(Parser* parser, Value value) {
    int64_t index = ObjFn_add_constant(parser->fn_compiler->fn, value, parser->vm);
    if (index < 0) {
        parser->had_memory_error = true;
        return 0;
    } else if (index > UINT16_MAX) {
        ERROR_AT_PREVIOUS_TOKEN("too many constants in function");
        return 0;
    }
    return (uint16_t)index;
}


// Takes an identifier token and adds its lexeme to the constant table as a string.
// Returns its index in the constant table.
static uint16_t make_string_constant_from_identifier(Parser* parser, Token* name) {
    ObjStr* string = ObjStr_copy_raw(name->start, name->length, parser->vm);
    if (!string) {
        parser->had_memory_error = true;
        return 0;
    }
    return add_value_to_constant_table(parser, pyro_make_obj(string));
}


// Stores [value] in the current function's constant table and emits bytecode to load it onto the
// top of the stack. Uses an optimized instruction set for loading small integer values.
static void emit_load_value_from_constant_table(Parser* parser, Value value) {
    if (IS_I64(value) && value.as.i64 >= 0 && value.as.i64 <= 9) {
        switch (value.as.i64) {
            case 0: emit_byte(parser, OP_LOAD_I64_0); return;
            case 1: emit_byte(parser, OP_LOAD_I64_1); return;
            case 2: emit_byte(parser, OP_LOAD_I64_2); return;
            case 3: emit_byte(parser, OP_LOAD_I64_3); return;
            case 4: emit_byte(parser, OP_LOAD_I64_4); return;
            case 5: emit_byte(parser, OP_LOAD_I64_5); return;
            case 6: emit_byte(parser, OP_LOAD_I64_6); return;
            case 7: emit_byte(parser, OP_LOAD_I64_7); return;
            case 8: emit_byte(parser, OP_LOAD_I64_8); return;
            case 9: emit_byte(parser, OP_LOAD_I64_9); return;
            default: return;
        }
    }

    uint16_t index = add_value_to_constant_table(parser, value);
    emit_byte(parser, OP_LOAD_CONSTANT);
    emit_u16be(parser, index);
}


// Reads the next token from the lexer.
static void advance(Parser* parser) {
    parser->previous_token = parser->next_token;
    parser->next_token = pyro_next_token(&parser->lexer);
    if (parser->next_token.type == TOKEN_ERROR) {
        parser->had_syntax_error = true;
    }
}


// Reads the next token from the lexer and validates that it has the expected type.
static bool consume(Parser* parser, TokenType type, const char* error_message) {
    if (parser->next_token.type == type) {
        advance(parser);
        return true;
    }
    ERROR_AT_NEXT_TOKEN(error_message);
    return false;
}


// Returns true if the next token has type [type].
static bool check(Parser* parser, TokenType type) {
    return parser->next_token.type == type;
}


// Returns true and advances the parser if the next token has type [type].
static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}


// Returns true and advances the parser if the next token has type [type1] or [type2].
static bool match2(Parser* parser, TokenType type1, TokenType type2) {
    if (check(parser, type1) || check(parser, type2)) {
        advance(parser);
        return true;
    }
    return false;
}


// Returns true and advances the parser if the next token has type [type1] or [type2] or [type3].
static bool match3(Parser* parser, TokenType type1, TokenType type2, TokenType type3) {
    if (check(parser, type1) || check(parser, type2) || check(parser, type3)) {
        advance(parser);
        return true;
    }
    return false;
}


static bool match_assignment_token(Parser* parser) {
    if (match3(parser, TOKEN_EQUAL, TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL)) {
        return true;
    }
    return false;
}


// Returns true if initialization succeeded, false if initialization failed because memory
// could not be allocated.
static bool init_fn_compiler(Parser* parser, FnCompiler* fn_compiler, FnType type, Token name) {
    // Set this compiler's enclosing compiler to the parser's current compiler.
    // Set the parser's current compiler to this compiler i.e. the last compiler in the chain.
    fn_compiler->enclosing = parser->fn_compiler;
    parser->fn_compiler = fn_compiler;

    fn_compiler->type = type;
    fn_compiler->local_count = 0;
    fn_compiler->scope_depth = 0;
    fn_compiler->loop_compiler = NULL;

    // Assign to NULL first as the function initializer can trigger the GC.
    fn_compiler->fn = NULL;
    fn_compiler->fn = ObjFn_new(parser->vm);
    if (!fn_compiler->fn) {
        parser->had_memory_error = true;
        parser->fn_compiler = fn_compiler->enclosing;
        return false;
    }

    fn_compiler->fn->name = ObjStr_copy_raw(name.start, name.length, parser->vm);
    if (!fn_compiler->fn->name) {
        parser->had_memory_error = true;
        parser->fn_compiler = fn_compiler->enclosing;
        return false;
    }

    fn_compiler->fn->source_id = ObjStr_copy_raw(parser->src_id, strlen(parser->src_id), parser->vm);
    if (!fn_compiler->fn->source_id) {
        parser->had_memory_error = true;
        parser->fn_compiler = fn_compiler->enclosing;
        return false;
    }

    // Reserve slot zero for the receiver when calling methods.
    Local* local = &fn_compiler->locals[fn_compiler->local_count++];
    local->depth = 0;
    local->is_initialized = true;
    local->is_captured = false;
    if (type == TYPE_METHOD || type == TYPE_INIT_METHOD) {
        local->name.start = "self";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }

    return true;
}


static ObjFn* end_fn_compiler(Parser* parser) {
    emit_naked_return(parser);
    ObjFn* fn = parser->fn_compiler->fn;

    #ifdef PYRO_DEBUG_DUMP_BYTECODE
        if (!parser->had_syntax_error) {
            pyro_disassemble_function(parser->vm, fn);
        }
    #endif

    parser->fn_compiler = parser->fn_compiler->enclosing;
    return fn;
}


static int resolve_local(Parser* parser, FnCompiler* fn_compiler, Token* name) {
    for (int i = fn_compiler->local_count - 1; i >= 0; i--) {
        Local* local = &fn_compiler->locals[i];
        if (lexemes_are_equal(name, &local->name)) {
            if (local->is_initialized) {
                return i;
            }
            ERROR_AT_PREVIOUS_TOKEN("can't read a local variable in its own initializer");
        }
    }
    return -1;
}


// [index] is the closed-over local variable's slot index.
// Returns the index of the newly created upvalue in the function's upvalue list.
static int add_upvalue(Parser* parser, FnCompiler* fn_compiler, uint8_t index, bool is_local) {
    size_t upvalue_count = fn_compiler->fn->upvalue_count;

    // Dont add duplicate upvalues unnecessarily if a closure references the same variable in a
    // surrounding function multiple times.
    for (size_t i = 0; i < upvalue_count; i++) {
        Upvalue* upvalue = &fn_compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count == 256) {
        ERROR_AT_PREVIOUS_TOKEN("too many closure variables in function (max: 256)");
        return 0;
    }

    fn_compiler->upvalues[upvalue_count].is_local = is_local;
    fn_compiler->upvalues[upvalue_count].index = index;
    return fn_compiler->fn->upvalue_count++;
}


// Looks for a local variable declared in any of the surrounding functions.
// If it finds one, it returns the 'upvalue index' for that variable.
// A return value of -1 means the variable is either global or undefined.
static int resolve_upvalue(Parser* parser, FnCompiler* fn_compiler, Token* name) {
    if (fn_compiler->enclosing == NULL) {
        return -1;
    }

    // Look for a matching local variable in the directly enclosing function.
    // If we find one, capture it and return the upvalue index.
    int local = resolve_local(parser, fn_compiler->enclosing, name);
    if (local != -1) {
        fn_compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(parser, fn_compiler, (uint8_t)local, true);
    }

    // Look for a matching local variable beyond the immediately enclosing function by
    // recursively walking the chain of nested compilers. If we find one, the most deeply
    // nested call will capture it and return its upvalue index. As the recursive calls
    // unwind we'll construct a chain of upvalues leading to the captured variable.
    int upvalue = resolve_upvalue(parser, fn_compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(parser, fn_compiler, (uint8_t)upvalue, false);
    }

    // No matching local variable in any enclosing scope.
    return -1;
}


static void add_local(Parser* parser, Token name) {
    if (parser->fn_compiler->local_count == 256) {
        ERROR_AT_PREVIOUS_TOKEN("too many local variables in scope (max 256)");
        return;
    }
    Local* local = &parser->fn_compiler->locals[parser->fn_compiler->local_count++];
    local->name = name;
    local->depth = parser->fn_compiler->scope_depth;
    local->is_initialized = false; // the variable has been declared but not yet defined
    local->is_captured = false;
}


static void mark_initialized(Parser* parser) {
    if (parser->fn_compiler->scope_depth == 0) {
        return;
    }
    parser->fn_compiler->locals[parser->fn_compiler->local_count - 1].is_initialized = true;
}


static void mark_initialized_multi(Parser* parser, size_t count) {
    if (parser->fn_compiler->scope_depth == 0) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        parser->fn_compiler->locals[parser->fn_compiler->local_count - 1 - i].is_initialized = true;
    }
}


static void define_variable(Parser* parser, uint16_t index, Access access) {
    if (parser->fn_compiler->scope_depth > 0) {
        mark_initialized(parser);
        return;
    }
    OpCode opcode = (access == PUBLIC) ? OP_DEFINE_PUB_GLOBAL : OP_DEFINE_PRI_GLOBAL;
    emit_byte(parser, opcode);
    emit_u16be(parser, index);
}


static void define_variables(Parser* parser, uint16_t* indexes, size_t count, Access access) {
    if (parser->fn_compiler->scope_depth > 0) {
        mark_initialized_multi(parser, count);
        return;
    }
    OpCode opcode = (access == PUBLIC) ? OP_DEFINE_PUB_GLOBALS : OP_DEFINE_PRI_GLOBALS;
    emit_u8_u8(parser, opcode, count);
    for (size_t i = 0; i < count; i++) {
        emit_u16be(parser, indexes[i]);
    }
}


// Add a newly declared local variable to the compiler's locals list.
static void declare_variable(Parser* parser, Token name) {
    if (parser->fn_compiler->scope_depth == 0) {
        // Don't do anything for globals.
        return;
    }

    if (name.length == 1 && name.start[0] == '_') {
        add_local(parser, name);
        return;
    }

    for (int i = parser->fn_compiler->local_count - 1; i >= 0; i--) {
        Local* local = &parser->fn_compiler->locals[i];
        if (local->depth < parser->fn_compiler->scope_depth) {
            break;
        }
        if (lexemes_are_equal(&name, &local->name)) {
            ERROR_AT_PREVIOUS_TOKEN("a variable with this name already exists in this scope");
        }
    }

    add_local(parser, name);
}


// Called when declaring a variable or function parameter.
static uint16_t consume_variable_name(Parser* parser, const char* error_message) {
    consume(parser, TOKEN_IDENTIFIER, error_message);
    declare_variable(parser, parser->previous_token);

    // Local variables are referenced by index not by name so we don't need to add the name
    // of a local to the list of constants. This return value will simply be ignored.
    if (parser->fn_compiler->scope_depth > 0) {
        return 0;
    }

    // Global variables are referenced by name.
    return make_string_constant_from_identifier(parser, &parser->previous_token);
}


// Emits bytecode to discard all local variables at scope depth greater than or equal to [depth].
// Returns the number of locals discarded. (This function doesn't decremement the local count as
// it's called directly by break statements.)
static int discard_locals(Parser* parser, int depth) {
    int local_count = parser->fn_compiler->local_count;

    while (local_count > 0 && parser->fn_compiler->locals[local_count - 1].depth >= depth) {
        if (parser->fn_compiler->locals[local_count - 1].is_captured) {
            emit_byte(parser, OP_CLOSE_UPVALUE);
        } else {
            emit_byte(parser, OP_POP);
        }
        local_count--;
    }

    return parser->fn_compiler->local_count - local_count;
}


static void begin_scope(Parser* parser) {
    parser->fn_compiler->scope_depth++;
}


static void end_scope(Parser* parser) {
    int popped = discard_locals(parser, parser->fn_compiler->scope_depth);
    parser->fn_compiler->local_count -= popped;
    parser->fn_compiler->scope_depth--;
}


// Emits a jump instruction and a two-byte placeholder operand. Returns the index of the
// first byte of the operand.
static size_t emit_jump(Parser* parser, OpCode instruction) {
    emit_byte(parser, instruction);
    emit_byte(parser, 0xff);
    emit_byte(parser, 0xff);
    return parser->fn_compiler->fn->code_count - 2;
}


// We call patch_jump() right before emitting the instruction we want the jump to land on.
static void patch_jump(Parser* parser, size_t index) {
    if (parser->had_memory_error) {
        return;
    }

    size_t jump = parser->fn_compiler->fn->code_count - index - 2;
    if (jump > UINT16_MAX) {
        ERROR_AT_PREVIOUS_TOKEN("too much code to jump over");
        return;
    }

    parser->fn_compiler->fn->code[index] = (jump >> 8) & 0xff;
    parser->fn_compiler->fn->code[index + 1] = jump & 0xff;
}


/* ------------------ */
/* Expression Parsers */
/* ------------------ */


static uint8_t parse_argument_list(Parser* parser, bool* unpack_last_argument) {
    uint8_t arg_count = 0;
    bool unpack = false;

    do {
        if (check(parser, TOKEN_RIGHT_PAREN)) {
            break;
        }
        if (unpack) {
            ERROR_AT_NEXT_TOKEN("unpacked argument must be last argument");
        }
        if (match(parser, TOKEN_STAR)) {
            unpack = true;
        }
        parse_expression(parser, false, true);
        if (arg_count == 255) {
            ERROR_AT_PREVIOUS_TOKEN("too many arguments (max: 255)");
        }
        arg_count++;
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after arguments");
    *unpack_last_argument = unpack;
    return arg_count;
}


// Emits bytecode to load the value of the named variable onto the stack.
static void emit_load_named_variable(Parser* parser, Token name) {
    int local_index = resolve_local(parser, parser->fn_compiler, &name);
    if (local_index != -1) {
        emit_u8_u8(parser, OP_GET_LOCAL, (uint8_t)local_index);
        return;
    }

    int upvalue_index = resolve_upvalue(parser, parser->fn_compiler, &name);
    if (upvalue_index != -1) {
        emit_u8_u8(parser, OP_GET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, OP_GET_GLOBAL);
    emit_u16be(parser, const_index);
}


// Emits bytecode to set the named variable to the value on top of the stack.
static void emit_store_named_variable(Parser* parser, Token name) {
    int local_index = resolve_local(parser, parser->fn_compiler, &name);
    if (local_index != -1) {
        emit_u8_u8(parser, OP_SET_LOCAL, (uint8_t)local_index);
        return;
    }

    int upvalue_index = resolve_upvalue(parser, parser->fn_compiler, &name);
    if (upvalue_index != -1) {
        emit_u8_u8(parser, OP_SET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, OP_SET_GLOBAL);
    emit_u16be(parser, const_index);
}


static int64_t parse_hex_literal(Parser* parser) {
    char buffer[16 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 16) {
            ERROR_AT_PREVIOUS_TOKEN("too many digits in hex literal (max: 16)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid hex literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 16);
    if (errno != 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid hex literal (out of range)");
    }

    return value;
}


static int64_t parse_binary_literal(Parser* parser) {
    char buffer[64 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 64) {
            ERROR_AT_PREVIOUS_TOKEN("too many digits in binary literal (max: 64)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid binary literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 2);
    if (errno != 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid binary literal (out of range)");
    }

    return value;
}


static int64_t parse_octal_literal(Parser* parser) {
    char buffer[22 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 22) {
            ERROR_AT_PREVIOUS_TOKEN("too many digits in octal literal (max: 22)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid octal literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 8);
    if (errno != 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid octal literal (out of range)");
    }

    return value;
}


static int64_t parse_int_literal(Parser* parser) {
    char buffer[20 + 1];
    size_t count = 0;

    for (size_t i = 0; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 20) {
            ERROR_AT_PREVIOUS_TOKEN("too many digits in integer literal (max: 20)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 10);
    if (errno != 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid integer literal (out of range)");
    }

    return value;
}


// So, figuring out the maximum possible length of a 64-bit IEEE 754 float literal is complicated.
// Possible answers [here][1] include 24 characters and... 1079 characters! Setting a 24 character
// maximum for now but I may want to revisit this at some point.
//
// [1]: https://stackoverflow.com/questions/1701055/
//
static double parse_float_literal(Parser* parser) {
    char buffer[24 + 1];
    size_t count = 0;

    for (size_t i = 0; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 24) {
            ERROR_AT_PREVIOUS_TOKEN("too many digits in floating-point literal (max: 24)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    double value = strtod(buffer, NULL);
    if (errno != 0) {
        ERROR_AT_PREVIOUS_TOKEN("invalid floating-point literal (out of range)");
    }

    return value;
}


static uint32_t parse_char_literal(Parser* parser) {
    const char* start = parser->previous_token.start + 1;
    size_t length = parser->previous_token.length - 2;

    // The longest valid character literal is a unicode escape sequence of the form: '\UXXXXXXXX'.
    if (length == 0 || length > 10) {
        ERROR_AT_PREVIOUS_TOKEN("invalid character literal");
        return 0;
    }

    char buffer[10];
    size_t count = pyro_unescape_string(start, length, buffer);

    Utf8CodePoint cp;
    if (!pyro_read_utf8_codepoint((uint8_t*)buffer, count, &cp)) {
        ERROR_AT_PREVIOUS_TOKEN("invalid character literal (not utf-8)");
        return 0;
    }

    if (cp.length != count) {
        ERROR_AT_PREVIOUS_TOKEN("invalid character literal (too many bytes)");
        return 0;
    }

    return cp.value;
}


static void parse_map_literal(Parser* parser) {
    uint16_t count = 0;
    do {
        if (check(parser, TOKEN_RIGHT_BRACE)) {
            break;
        }
        parse_expression(parser, false, true);
        consume(parser, TOKEN_EQUAL, "expected '=' after key");
        parse_expression(parser, false, true);
        count++;
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after map literal");
    emit_byte(parser, OP_MAKE_MAP);
    emit_u16be(parser, count);
}


static void parse_vec_literal(Parser* parser) {
    uint16_t count = 0;
    do {
        if (check(parser, TOKEN_RIGHT_BRACKET)) {
            break;
        }
        parse_expression(parser, false, true);
        count++;
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after vector literal");
    emit_byte(parser, OP_MAKE_VEC);
    emit_u16be(parser, count);
}


static void parse_variable_expression(Parser* parser, bool can_assign) {
    Token name = parser->previous_token;

    if (can_assign && match(parser, TOKEN_EQUAL)) {
        parse_expression(parser, true, true);
        emit_store_named_variable(parser, name);

    } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
        emit_load_named_variable(parser, name);
        parse_expression(parser, true, true);
        emit_byte(parser, OP_BINARY_PLUS);
        emit_store_named_variable(parser, name);

    } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
        emit_load_named_variable(parser, name);
        parse_expression(parser, true, true);
        emit_byte(parser, OP_BINARY_MINUS);
        emit_store_named_variable(parser, name);

    } else {
        emit_load_named_variable(parser, name);
    }
}


static void parse_default_value_expression(Parser* parser, const char* value_type) {
    if (match(parser, TOKEN_TRUE)) {
        emit_byte(parser, OP_LOAD_TRUE);
    }

    else if (match(parser, TOKEN_FALSE)) {
        emit_byte(parser, OP_LOAD_FALSE);
    }

    else if (match(parser, TOKEN_NULL)) {
        emit_byte(parser, OP_LOAD_NULL);
    }

    else if (match(parser, TOKEN_INT)) {
        int64_t value = parse_int_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_HEX_INT)) {
        int64_t value = parse_hex_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_BINARY_INT)) {
        int64_t value = parse_binary_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_OCTAL_INT)) {
        int64_t value = parse_octal_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_FLOAT)) {
        double value = parse_float_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_f64(value));
    }

    else if (match(parser, TOKEN_STRING)) {
        const char* start = parser->previous_token.start + 1;
        size_t length = parser->previous_token.length - 2;
        ObjStr* string = ObjStr_copy_raw(start, length, parser->vm);
        emit_load_value_from_constant_table(parser, pyro_make_obj(string));
    }

    else if (match(parser, TOKEN_ESCAPED_STRING)) {
        const char* start = parser->previous_token.start + 1;
        size_t length = parser->previous_token.length - 2;
        ObjStr* string = ObjStr_copy_esc(start, length, parser->vm);
        emit_load_value_from_constant_table(parser, pyro_make_obj(string));
    }

    else if (match(parser, TOKEN_CHAR)) {
        uint32_t codepoint = parse_char_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_char(codepoint));
    }

    else {
        ERROR_AT_NEXT_TOKEN(
            "unexpected token '%.*s', a default %s value must be a simple literal",
            parser->next_token.length,
            parser->next_token.start,
            value_type
        );
    }
}


static TokenType parse_primary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    TokenType token_type = parser->next_token.type;

    if (match(parser, TOKEN_TRUE)) {
        emit_byte(parser, OP_LOAD_TRUE);
    }

    else if (match(parser, TOKEN_FALSE)) {
        emit_byte(parser, OP_LOAD_FALSE);
    }

    else if (match(parser, TOKEN_NULL)) {
        emit_byte(parser, OP_LOAD_NULL);
    }

    else if (match(parser, TOKEN_INT)) {
        int64_t value = parse_int_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_HEX_INT)) {
        int64_t value = parse_hex_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_BINARY_INT)) {
        int64_t value = parse_binary_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_OCTAL_INT)) {
        int64_t value = parse_octal_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_i64(value));
    }

    else if (match(parser, TOKEN_FLOAT)) {
        double value = parse_float_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_f64(value));
    }

    else if (match(parser, TOKEN_LEFT_PAREN)) {
        parse_expression(parser, can_assign_in_parens, can_assign_in_parens);
        if (match_assignment_token(parser) && !can_assign_in_parens) {
            if (parser->fn_compiler->type == TYPE_TRY_EXPR) {
                ERROR_AT_PREVIOUS_TOKEN("assignment is not allowed inside try expressions");
            } else {
                ERROR_AT_PREVIOUS_TOKEN("invalid assignment target");
            }
        }
        consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after expression");
    }

    else if (match(parser, TOKEN_STRING)) {
        const char* start = parser->previous_token.start + 1;
        size_t length = parser->previous_token.length - 2;
        ObjStr* string = ObjStr_copy_raw(start, length, parser->vm);
        emit_load_value_from_constant_table(parser, pyro_make_obj(string));
    }

    else if (match(parser, TOKEN_ESCAPED_STRING)) {
        const char* start = parser->previous_token.start + 1;
        size_t length = parser->previous_token.length - 2;
        ObjStr* string = ObjStr_copy_esc(start, length, parser->vm);
        emit_load_value_from_constant_table(parser, pyro_make_obj(string));
    }

    else if (match(parser, TOKEN_CHAR)) {
        uint32_t codepoint = parse_char_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_make_char(codepoint));
    }

    else if (match(parser, TOKEN_IDENTIFIER)) {
        parse_variable_expression(parser, can_assign);
    }

    else if (match(parser, TOKEN_SELF)) {
        if (parser->class_compiler == NULL) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'self' outside a method declaration");
        } else if (parser->fn_compiler->type == TYPE_STATIC_METHOD) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'self' in a static method");
        }
        emit_load_named_variable(parser, parser->previous_token);
    }

    else if (match(parser, TOKEN_SUPER)) {
        if (parser->class_compiler == NULL) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' outside a class");
        } else if (parser->fn_compiler->type == TYPE_STATIC_METHOD) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' in a static method");
        } else if (!parser->class_compiler->has_superclass) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' in a class with no superclass");
        }

        consume(parser, TOKEN_COLON, "expected ':' after 'super'");
        consume(parser, TOKEN_IDENTIFIER, "expected superclass method name");

        uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
        emit_load_named_variable(parser, syntoken("self"));    // load the instance

        if (match(parser, TOKEN_LEFT_PAREN)) {
            bool unpack_last_argument;
            uint8_t arg_count = parse_argument_list(parser, &unpack_last_argument);
            emit_load_named_variable(parser, syntoken("super"));   // load the superclass
            if (unpack_last_argument) {
                emit_byte(parser, OP_CALL_SUPER_METHOD_UNPACK);
            } else {
                emit_byte(parser, OP_CALL_SUPER_METHOD);
            }
            emit_u16be(parser, index);
            emit_byte(parser, arg_count);
        } else {
            emit_load_named_variable(parser, syntoken("super"));   // load the superclass
            emit_byte(parser, OP_GET_SUPER_METHOD);
            emit_u16be(parser, index);
        }
    }

    else if (match(parser, TOKEN_DEF)) {
        parse_function_definition(parser, TYPE_FUNCTION, syntoken("<lambda>"));
    }

    else if (match(parser, TOKEN_LEFT_BRACKET)) {
        parse_vec_literal(parser);
    }

    else if (match(parser, TOKEN_LEFT_BRACE)) {
        parse_map_literal(parser);
    }

    else {
        ERROR_AT_NEXT_TOKEN(
            "unexpected token '%.*s', expected an expression",
            parser->next_token.length,
            parser->next_token.start
        );
    }

    return token_type;
}


static void parse_call_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    TokenType last_token_type = parse_primary_expr(parser, can_assign, can_assign_in_parens);

    while (true) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            bool unpack_last_argument;
            uint8_t arg_count = parse_argument_list(parser, &unpack_last_argument);
            if (unpack_last_argument) {
                emit_u8_u8(parser, OP_CALL_UNPACK_LAST_ARG, arg_count);
            } else {
                emit_u8_u8(parser, OP_CALL, arg_count);
            }
        }

        else if (match(parser, TOKEN_LEFT_BRACKET)) {
            parse_expression(parser, true, true);
            consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after index");
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true, true);
                emit_byte(parser, OP_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, OP_DUP_2);
                emit_byte(parser, OP_GET_INDEX);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_BINARY_PLUS);
                emit_byte(parser, OP_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, OP_DUP_2);
                emit_byte(parser, OP_GET_INDEX);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_BINARY_MINUS);
                emit_byte(parser, OP_SET_INDEX);
            } else {
                emit_byte(parser, OP_GET_INDEX);
            }
        }

        else if (match(parser, TOKEN_DOT)) {
            OpCode get_opcode = (last_token_type == TOKEN_SELF) ? OP_GET_FIELD : OP_GET_PUB_FIELD;
            OpCode set_opcode = (last_token_type == TOKEN_SELF) ? OP_SET_FIELD : OP_SET_PUB_FIELD;
            consume(parser, TOKEN_IDENTIFIER, "expected a field name after '.'");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true, true);
                emit_u8_u16be(parser, set_opcode, index);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, OP_DUP);
                emit_u8_u16be(parser, get_opcode, index);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_BINARY_PLUS);
                emit_u8_u16be(parser, set_opcode, index);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, OP_DUP);
                emit_u8_u16be(parser, get_opcode, index);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_BINARY_MINUS);
                emit_u8_u16be(parser, set_opcode, index);
            } else {
                emit_u8_u16be(parser, get_opcode, index);
            }
        }

        else if (match(parser, TOKEN_COLON)) {
            consume(parser, TOKEN_IDENTIFIER, "expected a method name after ':'");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
            if (match(parser, TOKEN_LEFT_PAREN)) {
                bool unpack_last_argument;
                uint8_t arg_count = parse_argument_list(parser, &unpack_last_argument);
                if (unpack_last_argument) {
                    OpCode opcode = (last_token_type == TOKEN_SELF) ? OP_CALL_METHOD_UNPACK : OP_CALL_PUB_METHOD_UNPACK;
                    emit_u8_u16be(parser, opcode, index);
                    emit_byte(parser, arg_count);
                } else {
                    OpCode opcode = (last_token_type == TOKEN_SELF) ? OP_CALL_METHOD : OP_CALL_PUB_METHOD;
                    emit_u8_u16be(parser, opcode, index);
                    emit_byte(parser, arg_count);
                }
            } else {
                OpCode opcode = (last_token_type == TOKEN_SELF) ? OP_GET_METHOD : OP_GET_PUB_METHOD;
                emit_u8_u16be(parser, opcode, index);
            }
        }

        else if (match(parser, TOKEN_COLON_COLON)) {
            consume(parser, TOKEN_IDENTIFIER, "expected a member name after '::'");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
            emit_u8_u16be(parser, OP_GET_MEMBER, index);
        }

        else {
            break;
        }

        // Set this to a null value after the first pass through the while-loop as we only care
        // about cases of [self.foo] or [self:foo].
        last_token_type = TOKEN_UNDEFINED;
    }
}


static void parse_try_expr(Parser* parser) {
    FnCompiler fn_compiler;
    if (!init_fn_compiler(parser, &fn_compiler, TYPE_TRY_EXPR, syntoken("try"))) {
        return;
    }

    begin_scope(parser);
    parse_unary_expr(parser, false, false);
    emit_byte(parser, OP_RETURN);

    ObjFn* fn = end_fn_compiler(parser);
    emit_u8_u16be(parser, OP_MAKE_CLOSURE, add_value_to_constant_table(parser, pyro_make_obj(fn)));

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, fn_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, fn_compiler.upvalues[i].index);
    }
}


static void parse_power_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_call_expr(parser, can_assign, can_assign_in_parens);
    if (match(parser, TOKEN_STAR_STAR)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_BINARY_STAR_STAR);
    }
}


static void parse_unary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    if (match(parser, TOKEN_MINUS)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_UNARY_MINUS);
    } else if (match(parser, TOKEN_PLUS)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_UNARY_PLUS);
    } else if (match(parser, TOKEN_BANG)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_UNARY_BANG);
    } else if (match(parser, TOKEN_TRY)) {
        parse_try_expr(parser);
        emit_byte(parser, OP_TRY);
    } else if (match(parser, TOKEN_TILDE)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_UNARY_TILDE);
    } else {
        parse_power_expr(parser, can_assign, can_assign_in_parens);
    }
}


static void parse_bitwise_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_unary_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_CARET)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_CARET);
        } else if (match(parser, TOKEN_AMP)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_AMP);
        } else if (match(parser, TOKEN_BAR)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_BAR);
        } else if (match(parser, TOKEN_LESS_LESS)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_LESS_LESS);
        } else if (match(parser, TOKEN_GREATER_GREATER)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_GREATER_GREATER);
        } else {
            break;
        }
    }
}


static void parse_multiplicative_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_bitwise_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_STAR)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_STAR);
        } else if (match(parser, TOKEN_SLASH)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_SLASH);
        } else if (match(parser, TOKEN_SLASH_SLASH)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_SLASH_SLASH);
        } else if (match(parser, TOKEN_PERCENT)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_PERCENT);
        } else {
            break;
        }
    }
}


static void parse_additive_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_multiplicative_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_PLUS)) {
            parse_multiplicative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_PLUS);
        } else if (match(parser, TOKEN_MINUS)) {
            parse_multiplicative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_MINUS);
        } else {
            break;
        }
    }
}


static void parse_comparative_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_additive_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_GREATER)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_GREATER);
        } else if (match(parser, TOKEN_GREATER_EQUAL)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_GREATER_EQUAL);
        } else if (match(parser, TOKEN_LESS)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_LESS);
        } else if (match(parser, TOKEN_LESS_EQUAL)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_LESS_EQUAL);
        } else if (match(parser, TOKEN_IN)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_IN);
        } else {
            break;
        }
    }
}


static void parse_equality_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_comparative_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_EQUAL_EQUAL)) {
            parse_comparative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_EQUAL_EQUAL);
        } else if (match(parser, TOKEN_BANG_EQUAL)) {
            parse_comparative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BINARY_BANG_EQUAL);
        } else {
            break;
        }
    }
}


// The logical operators && and || are short-circuiting and leave behind a truthy or falsey
// operand to indicate their result.
static void parse_logical_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_equality_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_AMP_AMP)) {
            size_t jump_to_end = emit_jump(parser, OP_JUMP_IF_FALSE);
            emit_byte(parser, OP_POP);
            parse_equality_expr(parser, false, can_assign_in_parens);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_BAR_BAR)) {
            size_t jump_to_end = emit_jump(parser, OP_JUMP_IF_TRUE);
            emit_byte(parser, OP_POP);
            parse_equality_expr(parser, false, can_assign_in_parens);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_HOOK_HOOK)) {
            size_t jump_to_end = emit_jump(parser, OP_JUMP_IF_NOT_NULL);
            emit_byte(parser, OP_POP);
            parse_equality_expr(parser, false, can_assign_in_parens);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_BANG_BANG)) {
            size_t jump_to_end = emit_jump(parser, OP_JUMP_IF_NOT_ERR);
            emit_byte(parser, OP_POP);
            parse_equality_expr(parser, false, can_assign_in_parens);
            patch_jump(parser, jump_to_end);
        } else {
            break;
        }
    }
}


static void parse_conditional_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_logical_expr(parser, can_assign, can_assign_in_parens);
    if (match(parser, TOKEN_HOOK)) {
        size_t jump_to_false_branch = emit_jump(parser, OP_JUMP_IF_FALSE);
        emit_byte(parser, OP_POP);
        parse_logical_expr(parser, false, can_assign_in_parens);
        size_t jump_to_end = emit_jump(parser, OP_JUMP);
        consume(parser, TOKEN_COLON_BAR, "expected ':|' after condition");
        patch_jump(parser, jump_to_false_branch);
        emit_byte(parser, OP_POP);
        parse_logical_expr(parser, false, can_assign_in_parens);
        patch_jump(parser, jump_to_end);
    }
}


static void parse_assignment_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_conditional_expr(parser, can_assign, can_assign_in_parens);

    if (can_assign && match_assignment_token(parser)) {
        ERROR_AT_PREVIOUS_TOKEN("invalid assignment target");
    }
}


static void parse_expression(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_assignment_expr(parser, can_assign, can_assign_in_parens);
}


/* ------------------- */
/*  Type Declarations  */
/* ------------------- */


static void parse_single_type(Parser* parser) {
    // Check for the word "null".
    if (match(parser, TOKEN_NULL)) {
        return;
    }

    // Parse a sequence of identifiers: foo::bar::baz.
    do {
        consume(parser, TOKEN_IDENTIFIER, "expected identifier in type declaration");
    } while (match(parser, TOKEN_COLON_COLON));

    // Parse an optional nullable marker: '?'.
    match(parser, TOKEN_HOOK);

    // Check for a container type declaration: [type, ...].
    if (match(parser, TOKEN_LEFT_BRACKET)) {
        do {
            parse_type(parser);
        } while (match(parser, TOKEN_COMMA));
        consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' in type declaration");
    }

    // Check for a callable type declaration: (type, ...).
    if (match(parser, TOKEN_LEFT_PAREN)) {
        if (!match(parser, TOKEN_RIGHT_PAREN)) {
            do {
                parse_type(parser);
            } while (match(parser, TOKEN_COMMA));
            consume(parser, TOKEN_RIGHT_PAREN, "expected ')' in type declaration");
        }
    }

    // Check for a return type declaration: -> type.
    if (match(parser, TOKEN_RIGHT_ARROW)) {
        parse_type(parser);
    }
}


static void parse_type(Parser* parser) {
    do {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            parse_single_type(parser);
            consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after type declaration");
        } else {
            parse_single_type(parser);
        }
    } while (match(parser, TOKEN_BAR));
}


/* ----------------- */
/* Statement Parsers */
/* ----------------- */


static void parse_echo_stmt(Parser* parser) {
    parse_expression(parser, true, true);
    int count = 1;
    while (match(parser, TOKEN_COMMA)) {
        parse_expression(parser, true, true);
        count++;
    }
    if (count > 255) {
        ERROR_AT_PREVIOUS_TOKEN("too many arguments for 'echo' (max: 255)");
    }
    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_u8_u8(parser, OP_ECHO, count);
}


static void parse_assert_stmt(Parser* parser) {
    parse_expression(parser, false, true);
    if (match_assignment_token(parser)) {
        ERROR_AT_PREVIOUS_TOKEN(
            "invalid assignment: assignment is not allowed inside 'assert' statements: "
            "wrap the assignment in parentheses to enable it"
        );
        return;
    }
    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(parser, OP_ASSERT);
}


static void parse_typedef_stmt(Parser* parser) {
    if (!consume(parser, TOKEN_IDENTIFIER, "expected type name after 'typedef'")) {
        return;
    }
    parse_type(parser);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after typedef statement");
}


static void parse_expression_stmt(Parser* parser) {
    parse_expression(parser, true, true);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(parser, OP_POP);
}


static void parse_unpacking_declaration(Parser* parser, Access access) {
    uint16_t indexes[16];
    size_t count = 0;

    do {
        uint16_t index = consume_variable_name(parser, "expected variable name");
        indexes[count++] = index;
        if (count > 16) {
            ERROR_AT_PREVIOUS_TOKEN("too many variable names in list (max: 16)");
        }
        if (match(parser, TOKEN_COLON)) {
            parse_type(parser);
        }
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after variable names");
    consume(parser, TOKEN_EQUAL, "expected '=' after variable list");

    parse_expression(parser, true, true);
    emit_u8_u8(parser, OP_UNPACK, count);

    define_variables(parser, indexes, count, access);
}


static void parse_var_declaration(Parser* parser, Access access) {
    do {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            parse_unpacking_declaration(parser, access);
        } else {
            uint16_t index = consume_variable_name(parser, "expected variable name");
            if (match(parser, TOKEN_COLON)) {
                parse_type(parser);
            }
            if (match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true, true);
            } else {
                emit_byte(parser, OP_LOAD_NULL);
            }
            define_variable(parser, index, access);
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "expected ';' after variable declaration");
}


static void parse_import_stmt(Parser* parser) {
    // We'll use these arrays if we're explicitly importing named top-level members from the module.
    Token member_names[16];
    uint16_t member_indexes[16];
    int member_count = 0;

    // Read the first module name.
    if (!consume(parser, TOKEN_IDENTIFIER, "expected a module name after 'import'")) return;
    uint16_t module_index = make_string_constant_from_identifier(parser, &parser->previous_token);
    emit_u8_u16be(parser, OP_LOAD_CONSTANT, module_index);
    Token module_name = parser->previous_token;
    int module_count = 1;

    // Read one ::module or ::{...} chunk per iteration.
    while (match(parser, TOKEN_COLON_COLON)) {
        if (match(parser, TOKEN_LEFT_BRACE)) {
            if (match(parser, TOKEN_STAR)) {
                if (!consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after '*' in import statement")) return;
                if (!consume(parser, TOKEN_SEMICOLON, "expected ';' after '{*}' in import statement")) return;
                if (parser->fn_compiler->type != TYPE_MODULE || parser->fn_compiler->scope_depth != 0) {
                    ERROR_AT_PREVIOUS_TOKEN("can only import {*} at global scope");
                    return;
                }
                member_count = -1;
                break;
            }
            do {
                if (!consume(parser, TOKEN_IDENTIFIER, "expected member name in import statement")) return;
                member_indexes[member_count] = make_string_constant_from_identifier(parser, &parser->previous_token);
                emit_u8_u16be(parser, OP_LOAD_CONSTANT, member_indexes[member_count]);
                member_names[member_count] = parser->previous_token;
                member_count++;
                if (member_count > 16) {
                    ERROR_AT_PREVIOUS_TOKEN("too many member names in import statement (max: 16)");
                    return;
                }
            } while (match(parser, TOKEN_COMMA));
            if (!consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after member names in import statement")) return;
            break;
        }
        if (!consume(parser, TOKEN_IDENTIFIER, "expected module name in import statement")) return;
        module_index = make_string_constant_from_identifier(parser, &parser->previous_token);
        emit_u8_u16be(parser, OP_LOAD_CONSTANT, module_index);
        module_name = parser->previous_token;
        module_count++;
    }

    if (module_count > 255) {
        ERROR_AT_PREVIOUS_TOKEN("too many module names in import statement (max: 255)");
        return;
    }

    if (member_count == -1) {
        emit_byte(parser, OP_IMPORT_ALL_MEMBERS);
        emit_byte(parser, module_count);
        return;
    } else if (member_count == 0) {
        emit_byte(parser, OP_IMPORT_MODULE);
        emit_byte(parser, module_count);
    } else {
        emit_byte(parser, OP_IMPORT_NAMED_MEMBERS);
        emit_byte(parser, module_count);
        emit_byte(parser, member_count);
    }

    int alias_count = 0;
    if (match(parser, TOKEN_AS)) {
        do {
            if (!consume(parser, TOKEN_IDENTIFIER, "expected alias name in import statement")) return;
            module_index = make_string_constant_from_identifier(parser, &parser->previous_token);
            module_name = parser->previous_token;
            member_names[alias_count] = parser->previous_token;
            member_indexes[alias_count] = module_index;
            alias_count++;
            if (alias_count > 16) {
                ERROR_AT_PREVIOUS_TOKEN("too many alias names in import statement (max: 16)");
                return;
            }
        } while (match(parser, TOKEN_COMMA));
    }

    if (member_count > 0) {
        if (alias_count > 0 && alias_count != member_count) {
            ERROR_AT_PREVIOUS_TOKEN("alias and member numbers do not match in import statement");
            return;
        }
        for (int i = 0; i < member_count; i++) {
            declare_variable(parser, member_names[i]);
        }
        define_variables(parser, member_indexes, member_count, PRIVATE);
    } else {
        if (alias_count > 1) {
            ERROR_AT_PREVIOUS_TOKEN("too many alias names in import statement");
            return;
        }
        declare_variable(parser, module_name);
        define_variable(parser, module_index, PRIVATE);
    }

    consume(parser, TOKEN_SEMICOLON, "expected ';' after import statement");
}


static void parse_block(Parser* parser) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        parse_statement(parser);
        if (parser->had_syntax_error || parser->had_memory_error) {
            break;
        }
    }
    consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after block");
}


static void parse_if_stmt(Parser* parser) {
    // Parse the condition.
    parse_expression(parser, false, true);
    if (match_assignment_token(parser)) {
        ERROR_AT_PREVIOUS_TOKEN(
            "invalid assignment: assignment is not allowed inside 'if' conditions: "
            "wrap the assignment in parentheses to enable it"
        );
        return;
    }

    // Jump over the 'then' block if the condition is false.
    size_t jump_over_then = emit_jump(parser, OP_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the 'then' block.
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' after condition in 'if' statement");
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // At the end of the 'then' block, unconditionally jump over the 'else' block.
    size_t jump_over_else = emit_jump(parser, OP_JUMP);

    // Backpatch the destination for the jump over the 'then' block.
    patch_jump(parser, jump_over_then);

    // Emit the bytecode for the 'else' block.
    if (match(parser, TOKEN_ELSE)) {
        if (match(parser, TOKEN_IF)) {
            parse_if_stmt(parser);
        } else {
            consume(parser, TOKEN_LEFT_BRACE, "expected '{' after 'else'");
            begin_scope(parser);
            parse_block(parser);
            end_scope(parser);
        }
    }

    // Backpatch the destination for the jump over the 'else' block.
    patch_jump(parser, jump_over_else);
}


// Emits an instruction to jump backwards in the bytecode to the index identified by [start_index].
static void emit_loop(Parser* parser, size_t start_index) {
    emit_byte(parser, OP_JUMP_BACK);

    size_t offset = parser->fn_compiler->fn->code_count - start_index + 2;
    if (offset > UINT16_MAX) {
        ERROR_AT_PREVIOUS_TOKEN("loop body is too large");
    }

    emit_byte(parser, (offset >> 8) & 0xff);
    emit_byte(parser, offset & 0xff);
}


static void parse_for_in_stmt(Parser* parser) {
    // Push a new scope to wrap a dummy local variable pointing to the iterator object.
    begin_scope(parser);

    // Support unpacking syntax for up to 8 variable names: [for (foo, bar, baz) in ...]
    // 8 is an arbitrary restriction. We don't really need the brackets here to recognise unpacking
    // but they match the brackets in variable declarations where they really are required.
    Token loop_variables[8];
    size_t variable_count = 0;

    if (match(parser, TOKEN_LEFT_PAREN)) {
        do {
            consume(parser, TOKEN_IDENTIFIER, "expected loop variable name");
            loop_variables[variable_count++] = parser->previous_token;
            if (variable_count > 8) {
                ERROR_AT_PREVIOUS_TOKEN("too many variable names to unpack (max: 8)");
            }
        } while (match(parser, TOKEN_COMMA));
        consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after variable names");
    } else {
        consume(parser, TOKEN_IDENTIFIER, "expected loop variable name");
        loop_variables[0] = parser->previous_token;
        variable_count++;
    }

    consume(parser, TOKEN_IN, "expected keyword 'in'");

    // This is the object we'll be iterating over.
    parse_expression(parser, true, true);
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before the loop body");

    // Replace the object on top of the stack with the result of calling :$iter() on it.
    emit_byte(parser, OP_GET_ITERATOR);
    add_local(parser, syntoken("*iterator*"));

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_index = parser->fn_compiler->fn->code_count;
    loop.start_depth = parser->fn_compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    emit_byte(parser, OP_GET_NEXT_FROM_ITERATOR);
    size_t exit_jump_index = emit_jump(parser, OP_JUMP_IF_ERR);

    begin_scope(parser);
    if (variable_count > 1) {
        emit_u8_u8(parser, OP_UNPACK, variable_count);
    }
    for (size_t i = 0; i < variable_count; i++) {
        add_local(parser, loop_variables[i]);
        mark_initialized(parser);
    }
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_index);

    // Backpatch the destination for the exit jump.
    patch_jump(parser, exit_jump_index);

    // Pop the error object that caused the loop to exit.
    emit_byte(parser, OP_POP);

    // If there were any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_index;
        ObjFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;

    // Pop the scope containing the dummy iterator local.
    end_scope(parser);
}


static void parse_c_style_loop_stmt(Parser* parser) {
    // Push a new scope to wrap any loop variables declared in the initializer.
    begin_scope(parser);

    // Parse the (optional) initializer.
    if (match(parser, TOKEN_SEMICOLON)) {
        // No initializer clause.
    } else if (match(parser, TOKEN_VAR)) {
        parse_var_declaration(parser, PRIVATE);
    } else {
        parse_expression_stmt(parser);
    }

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_index = parser->fn_compiler->fn->code_count;
    loop.start_depth = parser->fn_compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Parse the (optional) condition.
    bool loop_has_condition = false;
    size_t exit_jump_index = 0;
    if (!match(parser, TOKEN_SEMICOLON)) {
        loop_has_condition = true;
        parse_expression(parser, false, true);
        consume(parser, TOKEN_SEMICOLON, "expected ';' after loop condition");
        exit_jump_index = emit_jump(parser, OP_POP_JUMP_IF_FALSE);
    }

    // Parse the (optional) increment clause.
    if (!match(parser, TOKEN_LEFT_BRACE)) {
        size_t body_jump_index = emit_jump(parser, OP_JUMP);

        size_t increment_index = parser->fn_compiler->fn->code_count;
        parse_expression(parser, true, true);
        emit_byte(parser, OP_POP);
        consume(parser, TOKEN_LEFT_BRACE, "expected '{' before loop body");

        emit_loop(parser, loop.start_index);
        loop.start_index = increment_index;
        patch_jump(parser, body_jump_index);
    }

    // Emit the bytecode for the block.
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_index);

    // Backpatch the destination for the exit jump.
    if (loop_has_condition) {
        patch_jump(parser, exit_jump_index);
    }

    // If there were any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_index;
        ObjFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;

    // Pop the scope surrounding the loop variables.
    end_scope(parser);
}


static void parse_infinite_loop_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_index = parser->fn_compiler->fn->code_count;
    loop.start_depth = parser->fn_compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Emit the bytecode for the block.
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_index);

    // If we found any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_index;
        ObjFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;
}


static void parse_while_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_index = parser->fn_compiler->fn->code_count;
    loop.start_depth = parser->fn_compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Parse the condition.
    parse_expression(parser, false, true);
    if (match_assignment_token(parser)) {
        ERROR_AT_PREVIOUS_TOKEN(
            "invalid assignment: assignment is not allowed inside 'while' conditions: "
            "wrap the assignment in parentheses to enable it"
        );
        return;
    }
    size_t exit_jump_index = emit_jump(parser, OP_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the block.
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before loop body");
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_index);

    // Backpatch the destination for the exit jump.
    patch_jump(parser, exit_jump_index);

    // If we found any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_index;
        ObjFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;
}


static void parse_with_stmt(Parser* parser) {
    if (!consume(parser, TOKEN_IDENTIFIER, "expected a variable name after 'with'")) {
        return;
    }
    Token variable_name = parser->previous_token;

    if (match(parser, TOKEN_COLON)) {
        parse_type(parser);
    }

    if (!consume(parser, TOKEN_EQUAL, "expected '=' after variable name in 'with' statement")) {
        return;
    }

    parse_expression(parser, true, true);
    emit_byte(parser, OP_START_WITH);
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before 'with' statement body");
    begin_scope(parser);
    add_local(parser, variable_name);
    mark_initialized(parser);
    parse_block(parser);
    end_scope(parser);
    emit_byte(parser, OP_END_WITH);
}


// This helper parses a function definition, i.e. the bit after the name that looks like (...){...}.
// It emits the bytecode to create an ObjClosure and leave it on top of the stack.
static void parse_function_definition(Parser* parser, FnType type, Token name) {
    FnCompiler fn_compiler;
    if (!init_fn_compiler(parser, &fn_compiler, type, name)) {
        return;
    }
    begin_scope(parser);
    uint8_t default_value_count = 0;

    // Compile the parameter list.
    consume(parser, TOKEN_LEFT_PAREN, "expected '(' before function parameters");
    do {
        if (check(parser, TOKEN_RIGHT_PAREN)) {
            break;
        }
        if (fn_compiler.fn->arity == 255) {
            ERROR_AT_NEXT_TOKEN("too many parameters (max: 255)");
        }
        if (fn_compiler.fn->is_variadic) {
            ERROR_AT_NEXT_TOKEN("variadic parameter must be final parameter");
        }
        if (match(parser, TOKEN_STAR)) {
            fn_compiler.fn->is_variadic = true;
        }
        uint16_t index = consume_variable_name(parser, "expected parameter name");
        define_variable(parser, index, PRIVATE);
        fn_compiler.fn->arity++;
        if (match(parser, TOKEN_COLON)) {
            parse_type(parser);
        }
        if (default_value_count > 0 && !check(parser, TOKEN_EQUAL)) {
            ERROR_AT_NEXT_TOKEN("missing default value for parameter");
            return;
        }
        if (match(parser, TOKEN_EQUAL)) {
            if (fn_compiler.fn->is_variadic) {
                ERROR_AT_PREVIOUS_TOKEN("a variadic function cannot have default argument values");
            }
            parser->fn_compiler = fn_compiler.enclosing;
            parse_default_value_expression(parser, "argument");
            parser->fn_compiler = &fn_compiler;
            default_value_count += 1;
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after function parameters");

    // Check the arity for known method names.
    if (type == TYPE_METHOD) {
        if (strcmp(fn_compiler.fn->name->bytes, "$fmt") == 0 && fn_compiler.fn->arity != 1) {
            ERROR_AT_PREVIOUS_TOKEN("invalid method definition, $fmt() takes 1 argument");
            return;
        }
        if (strcmp(fn_compiler.fn->name->bytes, "$str") == 0 && fn_compiler.fn->arity != 0) {
            ERROR_AT_PREVIOUS_TOKEN("invalid method definition, $str() takes no arguments");
            return;
        }
    }

    if (match(parser, TOKEN_RIGHT_ARROW)) {
        parse_type(parser);
    }

    // Compile the body.
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before function body");
    parse_block(parser);

    // Create the function object.
    ObjFn* fn = end_fn_compiler(parser);

    // Add the function to the current function's constant table.
    uint16_t index = add_value_to_constant_table(parser, pyro_make_obj(fn));

    // Emit the bytecode to load the function onto the stack as an ObjClosure.
    if (default_value_count > 0) {
        emit_u8_u16be(parser, OP_MAKE_CLOSURE_WITH_DEF_ARGS, index);
        emit_byte(parser, default_value_count);
    } else {
        emit_u8_u16be(parser, OP_MAKE_CLOSURE, index);
    }

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, fn_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, fn_compiler.upvalues[i].index);
    }
}


static void parse_function_declaration(Parser* parser, Access access) {
    uint16_t index = consume_variable_name(parser, "expected a function name");
    mark_initialized(parser);
    parse_function_definition(parser, TYPE_FUNCTION, parser->previous_token);
    define_variable(parser, index, access);
}


static void parse_method_declaration(Parser* parser, Access access) {
    consume(parser, TOKEN_IDENTIFIER, "expected a method name");
    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);

    FnType type;
    if (access == STATIC) {
        type = TYPE_STATIC_METHOD;
    } else if (parser->previous_token.length == 5 && memcmp(parser->previous_token.start, "$init", 5) == 0) {
        type = TYPE_INIT_METHOD;
    } else {
        type = TYPE_METHOD;
    }

    parse_function_definition(parser, type, parser->previous_token);

    if (access == PUBLIC) {
        emit_u8_u16be(parser, OP_DEFINE_PUB_METHOD, index);
    } else if (access == PRIVATE) {
        emit_u8_u16be(parser, OP_DEFINE_PRI_METHOD, index);
    } else {
        emit_u8_u16be(parser, OP_DEFINE_STATIC_METHOD, index);
    }
}


static void parse_field_declaration(Parser* parser, Access access) {
    do {
        consume(parser, TOKEN_IDENTIFIER, "expected field name");
        uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);

        if (match(parser, TOKEN_COLON)) {
            parse_type(parser);
        }

        if (match(parser, TOKEN_EQUAL)) {
            if (access == STATIC) {
                parse_expression(parser, true, true);
            } else {
                parse_default_value_expression(parser, "field");
            }
        } else {
            emit_byte(parser, OP_LOAD_NULL);
        }

        if (access == PUBLIC) {
            emit_u8_u16be(parser, OP_DEFINE_PUB_FIELD, index);
        } else if (access == PRIVATE) {
            emit_u8_u16be(parser, OP_DEFINE_PRI_FIELD, index);
        } else {
            emit_u8_u16be(parser, OP_DEFINE_STATIC_FIELD, index);
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "expected ';' after field declaration");
}


static void parse_class_declaration(Parser* parser, Access access) {
    consume(parser, TOKEN_IDENTIFIER, "expected class name");
    Token class_name = parser->previous_token;

    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
    declare_variable(parser, parser->previous_token);

    emit_u8_u16be(parser, OP_MAKE_CLASS, index);
    define_variable(parser, index, access);

    // Push a new class compiler onto the implicit linked-list stack.
    ClassCompiler class_compiler;
    class_compiler.name = parser->previous_token;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = parser->class_compiler;
    parser->class_compiler = &class_compiler;

    if (match(parser, TOKEN_LESS)) {
        consume(parser, TOKEN_IDENTIFIER, "expected superclass name");
        emit_load_named_variable(parser, parser->previous_token);

        // We declare 'super' as a local variable in a new lexical scope wrapping the method
        // declarations so it can be captured by the upvalue machinery.
        begin_scope(parser);
        add_local(parser, syntoken("super"));
        define_variable(parser, 0, PRIVATE);

        emit_load_named_variable(parser, class_name);
        emit_byte(parser, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    // Load the class object back onto the top of the stack.
    emit_load_named_variable(parser, class_name);

    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before class body");
    while (true) {
        if (match(parser, TOKEN_PUB)) {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, PUBLIC);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, PUBLIC);
            } else {
                ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'pub'");
                return;
            }
        } else if (match(parser, TOKEN_PRI)) {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, PRIVATE);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, PRIVATE);
            } else {
                ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'pri'");
                return;
            }
        } else if (match(parser, TOKEN_STATIC)) {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, STATIC);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, STATIC);
            } else {
                ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'static'");
                return;
            }
        } else {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, PRIVATE);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, PRIVATE);
            } else {
                break;
            }
        }
    }
    consume(parser, TOKEN_RIGHT_BRACE, "unexpected token, expected '}' after class body");
    emit_byte(parser, OP_POP); // pop the class object

    if (class_compiler.has_superclass) {
        end_scope(parser);
    }

    // Pop the class compiler.
    parser->class_compiler = class_compiler.enclosing;
}


static void parse_return_stmt(Parser* parser) {
    if (parser->fn_compiler->type == TYPE_MODULE) {
        ERROR_AT_PREVIOUS_TOKEN("can't return from top-level code");
    }

    if (match(parser, TOKEN_SEMICOLON)) {
        emit_naked_return(parser);
    } else {
        if (parser->fn_compiler->type == TYPE_INIT_METHOD) {
            ERROR_AT_PREVIOUS_TOKEN("can't return a value from an initializer");
        }
        parse_expression(parser, true, true);
        consume(parser, TOKEN_SEMICOLON, "expected ';' after return value");
        emit_byte(parser, OP_RETURN);
    }
}


static void parse_break_stmt(Parser* parser) {
    if (parser->fn_compiler->loop_compiler == NULL) {
        ERROR_AT_PREVIOUS_TOKEN("invalid use of 'break' outside a loop");
        return;
    }
    parser->fn_compiler->loop_compiler->had_break = true;

    discard_locals(parser, parser->fn_compiler->loop_compiler->start_depth + 1);
    emit_jump(parser, OP_BREAK);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after 'break'");
}


static void parse_continue_stmt(Parser* parser) {
    if (parser->fn_compiler->loop_compiler == NULL) {
        ERROR_AT_PREVIOUS_TOKEN("invalid use of 'continue' outside a loop");
        return;
    }

    discard_locals(parser, parser->fn_compiler->loop_compiler->start_depth + 1);
    emit_loop(parser, parser->fn_compiler->loop_compiler->start_index);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after 'continue'");
}


static void parse_statement(Parser* parser) {
    if (match(parser, TOKEN_SEMICOLON)) {
        return;
    }

    if (match(parser, TOKEN_PUB)) {
        if (parser->fn_compiler->type != TYPE_MODULE || parser->fn_compiler->scope_depth > 0) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'pub', only valid at global scope");
            return;
        }
        if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PUBLIC);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PUBLIC);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PUBLIC);
        } else {
            ERROR_AT_NEXT_TOKEN("unexpected token after 'pub'");
            return;
        }
    }

    else if (match(parser, TOKEN_PRI)) {
        if (parser->fn_compiler->type != TYPE_MODULE || parser->fn_compiler->scope_depth > 0) {
            ERROR_AT_PREVIOUS_TOKEN("invalid use of 'pri', only valid at global scope");
            return;
        }
        if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PRIVATE);
        } else {
            ERROR_AT_NEXT_TOKEN("unexpected token after 'pri'");
            return;
        }
    }

    else {
        if (match(parser, TOKEN_LEFT_BRACE)) {
            begin_scope(parser);
            parse_block(parser);
            end_scope(parser);
        } else if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_ECHO)) {
            parse_echo_stmt(parser);
        } else if (match(parser, TOKEN_ASSERT)) {
            parse_assert_stmt(parser);
        } else if (match(parser, TOKEN_IF)) {
            parse_if_stmt(parser);
        } else if (match(parser, TOKEN_WHILE)) {
            parse_while_stmt(parser);
        } else if (match(parser, TOKEN_LOOP)) {
            if (match(parser, TOKEN_LEFT_BRACE)) {
                parse_infinite_loop_stmt(parser);
            } else {
                parse_c_style_loop_stmt(parser);
            }
        } else if (match(parser, TOKEN_FOR)) {
            parse_for_in_stmt(parser);
        } else if (match(parser, TOKEN_RETURN)) {
            parse_return_stmt(parser);
        } else if (match(parser, TOKEN_BREAK)) {
            parse_break_stmt(parser);
        } else if (match(parser, TOKEN_CONTINUE)) {
            parse_continue_stmt(parser);
        } else if (match(parser, TOKEN_IMPORT)) {
            parse_import_stmt(parser);
        } else if (match(parser, TOKEN_TYPEDEF)) {
            parse_typedef_stmt(parser);
        } else if (match(parser, TOKEN_WITH)) {
            parse_with_stmt(parser);
        } else {
            parse_expression_stmt(parser);
            parser->num_expression_statements++;
        }
    }

    parser->num_statements++;
}


/* -------------------- */
/*  Compiler Interface  */
/* -------------------- */


static ObjFn* compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    Parser parser;
    parser.fn_compiler = NULL;
    parser.class_compiler = NULL;
    parser.had_syntax_error = false;
    parser.had_memory_error = false;
    parser.src_id = src_id;
    parser.vm = vm;
    parser.num_statements = 0;
    parser.num_expression_statements = 0;

    // Strip any trailing whitespace before initializing the lexer. This is to ensure we report the
    // correct line number for syntax errors at the end of the input, e.g. a missing trailing
    // semicolon.
    while (src_len > 0 && isspace(src_code[src_len - 1])) {
        src_len--;
    }
    pyro_init_lexer(&parser.lexer, vm, src_code, src_len, src_id);

    FnCompiler fn_compiler;
    if (!init_fn_compiler(&parser, &fn_compiler, TYPE_MODULE, basename_syntoken(src_id))) {
        pyro_panic(vm, "out of memory");
        return NULL;
    }

    // Prime the token pump by reading the first token from the lexer.
    advance(&parser);
    if (parser.had_syntax_error) {
        return NULL;
    }

    // We can get a cascade of syntax errors while parsing a single statement but only the first
    // will be reported. If the [had_syntax_error] flag is set, a panic has already been raised.
    while (!match(&parser, TOKEN_EOF)) {
        parse_statement(&parser);
        if (parser.had_syntax_error) {
            return NULL;
        }
        if (parser.had_memory_error) {
            pyro_panic(vm, "out of memory");
            return NULL;
        }
    }

    ObjFn* fn = end_fn_compiler(&parser);

    // If the code consisted of a single expression statement, we might want to print the value of
    // the expression if we're running inside a REPL.
    if (parser.num_statements == 1 && parser.num_expression_statements == 1) {
        assert(fn->code[fn->code_count - 3] == OP_POP);
        fn->code[fn->code_count - 3] = OP_POP_ECHO_IN_REPL;
    }

    return fn;
}


// This wrappper disables the garbage collector during compilation.
// TODO: this wrapper isn't needed any more.
ObjFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    vm->gc_disallows++;
    ObjFn* fn = compile(vm, src_code, src_len, src_id);
    vm->gc_disallows--;
    return fn;
}
