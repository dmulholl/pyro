#include "../includes/pyro.h"


/* -------------- */
/*  Error Macros  */
/* -------------- */


// Signals a syntax error at the previous token. The argument is the error message specified as
// a printf-style format string along with any values to be interpolated.
#define SYNTAX_ERROR_AT_PREVIOUS_TOKEN(...) \
    do { \
        if (!parser->vm->halt_flag) { \
            Token* token = &parser->previous_token; \
            pyro_panic_with_syntax_error(parser->vm, parser->src_id, token->line, __VA_ARGS__); \
        } \
    } while (false)


// Signals a syntax error at the next token. The argument is the error message specified as
// a printf-style format string along with any values to be interpolated.
#define SYNTAX_ERROR_AT_NEXT_TOKEN(...) \
    do { \
        if (!parser->vm->halt_flag) { \
            Token* token = &parser->next_token; \
            pyro_panic_with_syntax_error(parser->vm, parser->src_id, token->line, __VA_ARGS__); \
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
    bool is_constant;       // true if the local is a constant
} Local;


// If [is_local] is true the upvalue is capturing a local variable from the immediately
// surrounding function; [index] is the slot index of this local. If [is_local] is false the
// upvalue is capturing a chained upvalue from the immediately surrounding function referencing
// a captured local from a higher scope; [index] is the upvalue index of the captured upvalue.
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
    size_t start_bytecode_count;    // bytecode index of the loop's starting point
    size_t start_scope_depth;       // scope depth of the loop's starting point
    size_t start_with_block_depth;  // with-block depth of the loop's starting point
    bool had_break;
    struct LoopCompiler* enclosing;
} LoopCompiler;


typedef struct FnCompiler {
    struct FnCompiler* enclosing;
    PyroFn* fn;
    FnType type;
    Local locals[256];      // 256 as we use a single byte argument to index locals
    int local_count;        // how many local variables are in scope
    int scope_depth;        // number of blocks surrounding the curent code
    Upvalue upvalues[256];
    LoopCompiler* loop_compiler;
    size_t with_block_depth;
} FnCompiler;


typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
    bool has_superclass;
} ClassCompiler;


typedef struct GlobalConstant GlobalConstant;
struct GlobalConstant {
    Token name;
    GlobalConstant* next;
};


typedef struct GlobalAssignment GlobalAssignment;
struct GlobalAssignment {
    Token name;
    GlobalAssignment* next;
};


typedef struct {
    Lexer lexer;
    Token previous_token;
    Token next_token;
    PyroVM* vm;
    FnCompiler* fn_compiler;
    ClassCompiler* class_compiler;
    const char* src_id;
    size_t num_statements;
    size_t num_expression_statements;
    bool dump_bytecode;
    GlobalConstant* global_constants;
    GlobalAssignment* global_assignments;
} Parser;


typedef enum Access {
    PRIVATE,
    PUBLIC,
    STATIC
} Access;


/* -------------------- */
/* Forward Declarations */
/* -------------------- */


static void parse_expression(Parser* parser, bool can_assign);
static void parse_unary_expr(Parser* parser, bool can_assign);
static void parse_statement(Parser* parser);
static void parse_function_definition(Parser* parser, FnType type, Token name);
static void parse_type(Parser* parser);
static void log_global_constant(Parser* parser, Token name);
static void log_global_assignment(Parser* parser, Token name);


/* ----------------- */
/* Parsing Utilities */
/* ----------------- */


// Make a synthetic token to pass a hardcoded string around.
static Token make_syntoken(const char* text) {
    return (Token) {
        .type = TOKEN_SYNTHETIC,
        .line = 0,
        .start = text,
        .length = strlen(text)
    };
}


// Make a synthetic token containing the basename from [source_id].
static Token make_syntoken_from_basename(const char* source_id) {
    size_t length = strlen(source_id);

    if (length == 0) {
        return (Token) {
            .type = TOKEN_SYNTHETIC,
            .line = 0,
            .start = "code",
            .length = strlen("code")
        };
    }

    // Find the first character of the basename.
    const char* start = source_id + length;
    while (start > source_id) {
        start--;
        if (*start == '/') {
            start++;
            break;
        }
    }

    return (Token) {
        .type = TOKEN_SYNTHETIC,
        .line = 0,
        .start = start,
        .length = (source_id + length) - start,
    };
}


static bool lexemes_are_equal(Token* a, Token* b) {
    if (a->length == b->length) {
        return memcmp(a->start, b->start, a->length) == 0;
    }
    return false;
}


// Appends a single byte to the current function's bytecode. All writes to the bytecode go
// through this function.
static void emit_byte(Parser* parser, uint8_t byte) {
    if (!PyroFn_write(parser->fn_compiler->fn, byte, parser->previous_token.line, parser->vm)) {
        pyro_panic(parser->vm, "out of memory");
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
        emit_byte(parser, PYRO_OPCODE_GET_LOCAL_0);
    } else {
        emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
    }
    emit_byte(parser, PYRO_OPCODE_RETURN);
}


// Adds the value to the current function's constant table and returns its index.
static uint16_t add_value_to_constant_table(Parser* parser, PyroValue value) {
    int64_t index = PyroFn_add_constant(parser->fn_compiler->fn, value, parser->vm);
    if (index < 0) {
        pyro_panic(parser->vm, "out of memory");
        return 0;
    } else if (index > UINT16_MAX) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("the function's constant table is full (max: %d values)", UINT16_MAX);
        return 0;
    }
    return (uint16_t)index;
}


// Takes an identifier token and adds its lexeme to the constant table as a string.
// Returns its index in the constant table.
static uint16_t make_string_constant_from_identifier(Parser* parser, Token* name) {
    PyroStr* string = PyroStr_copy(name->start, name->length, false, parser->vm);
    if (!string) {
        pyro_panic(parser->vm, "out of memory");
        return 0;
    }
    return add_value_to_constant_table(parser, pyro_obj(string));
}


// Stores [value] in the current function's constant table and emits bytecode to load it onto
// the top of the stack.
// - Uses an optimized instruction set for loading small integer values.
// - Uses an optimized instruction set for loading constants with small indexes.
static void emit_load_value_from_constant_table(Parser* parser, PyroValue value) {
    if (PYRO_IS_I64(value) && value.as.i64 >= 0 && value.as.i64 <= 9) {
        switch (value.as.i64) {
            case 0: emit_byte(parser, PYRO_OPCODE_LOAD_I64_0); return;
            case 1: emit_byte(parser, PYRO_OPCODE_LOAD_I64_1); return;
            case 2: emit_byte(parser, PYRO_OPCODE_LOAD_I64_2); return;
            case 3: emit_byte(parser, PYRO_OPCODE_LOAD_I64_3); return;
            case 4: emit_byte(parser, PYRO_OPCODE_LOAD_I64_4); return;
            case 5: emit_byte(parser, PYRO_OPCODE_LOAD_I64_5); return;
            case 6: emit_byte(parser, PYRO_OPCODE_LOAD_I64_6); return;
            case 7: emit_byte(parser, PYRO_OPCODE_LOAD_I64_7); return;
            case 8: emit_byte(parser, PYRO_OPCODE_LOAD_I64_8); return;
            case 9: emit_byte(parser, PYRO_OPCODE_LOAD_I64_9); return;
            default: return;
        }
    }

    uint16_t index = add_value_to_constant_table(parser, value);

    switch (index) {
        case 0: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_0); break;
        case 1: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_1); break;
        case 2: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_2); break;
        case 3: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_3); break;
        case 4: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_4); break;
        case 5: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_5); break;
        case 6: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_6); break;
        case 7: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_7); break;
        case 8: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_8); break;
        case 9: emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT_9); break;
        default:
            emit_byte(parser, PYRO_OPCODE_LOAD_CONSTANT);
            emit_u16be(parser, index);
            break;
    }
}


static void emit_load_value_from_constant_table_at_index(Parser* parser, uint16_t index) {
    emit_u8_u16be(parser, PYRO_OPCODE_LOAD_CONSTANT, index);
}


// Reads the next token from the lexer.
static void advance(Parser* parser) {
    parser->previous_token = parser->next_token;
    parser->next_token = pyro_next_token(&parser->lexer);
}


// Reads the next token from the lexer and validates that it has the expected type.
static bool consume(Parser* parser, TokenType type, const char* error_message) {
    if (parser->next_token.type == type) {
        advance(parser);
        return true;
    }
    SYNTAX_ERROR_AT_NEXT_TOKEN(error_message);
    return false;
}


// Returns true if the next token has type [type]. Does not advance the parser.
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


// Returns true and advances the parser if the next token has type [type1] or [type2] or
// [type3].
static bool match3(Parser* parser, TokenType type1, TokenType type2, TokenType type3) {
    if (check(parser, type1) || check(parser, type2) || check(parser, type3)) {
        advance(parser);
        return true;
    }
    return false;
}


// Returns true and advances the parser if the next token is one of: =, +=, -=.
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
    fn_compiler->with_block_depth = 0;

    fn_compiler->fn = PyroFn_new(parser->vm);
    if (!fn_compiler->fn) {
        parser->fn_compiler = fn_compiler->enclosing;
        pyro_panic(parser->vm, "out of memory");
        return false;
    }

    fn_compiler->fn->name = PyroStr_copy(name.start, name.length, false, parser->vm);
    if (!fn_compiler->fn->name) {
        parser->fn_compiler = fn_compiler->enclosing;
        pyro_panic(parser->vm, "out of memory");
        return false;
    }

    fn_compiler->fn->source_id = PyroStr_copy(parser->src_id, strlen(parser->src_id), false, parser->vm);
    if (!fn_compiler->fn->source_id) {
        parser->fn_compiler = fn_compiler->enclosing;
        pyro_panic(parser->vm, "out of memory");
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


static PyroFn* end_fn_compiler(Parser* parser) {
    emit_naked_return(parser);
    PyroFn* fn = parser->fn_compiler->fn;

    if (parser->dump_bytecode) {
        if (!parser->vm->halt_flag) {
            pyro_disassemble_function(parser->vm, fn, parser->src_id);
        }
    }

    parser->fn_compiler = parser->fn_compiler->enclosing;
    return fn;
}


static bool resolve_local(Parser* parser, FnCompiler* fn_compiler, Token* name, int* local_index, bool* is_constant) {
    for (int i = fn_compiler->local_count - 1; i >= 0; i--) {
        Local* local = &fn_compiler->locals[i];
        if (lexemes_are_equal(name, &local->name)) {
            if (local->is_initialized) {
                *local_index = i;
                *is_constant = local->is_constant;
                return true;
            }
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("can't access local '%.*s' in initializing expression", name->length, name->start);
            return true;
        }
    }
    return false;
}


// If the upvalue is local, [index] is the closed-over local variable's slot index.
// If the upvalue is not local, [index] is an upvalue index in the enclosing function scope.
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

    // TODO: investigate whether this low limit is really necessary.
    if (upvalue_count == 256) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many closure variables in function (max: 256)");
        return 0;
    }

    fn_compiler->upvalues[upvalue_count].is_local = is_local;
    fn_compiler->upvalues[upvalue_count].index = index;
    return fn_compiler->fn->upvalue_count++;
}


// Looks for a local variable declared in any of the surrounding functions.
// If it finds one, it returns true and the 'upvalue index' for that variable.
// A return value of false means the variable is either global or undefined.
static bool resolve_upvalue(Parser* parser, FnCompiler* fn_compiler, Token* name, int* upvalue_index, bool* is_constant) {
    if (fn_compiler->enclosing == NULL) {
        return false;
    }

    // Look for a matching local variable in the directly enclosing function.
    // If we find one, capture it and return the upvalue index.
    int resolved_local_index = 0;
    if (resolve_local(parser, fn_compiler->enclosing, name, &resolved_local_index, is_constant)) {
        fn_compiler->enclosing->locals[resolved_local_index].is_captured = true;
        *upvalue_index = add_upvalue(parser, fn_compiler, (uint8_t)resolved_local_index, true);
        *is_constant = fn_compiler->enclosing->locals[resolved_local_index].is_constant;
        return true;
    }

    // Look for a matching local variable beyond the immediately enclosing function by
    // recursively walking the chain of nested compilers. If we find one, the most deeply
    // nested call will capture it and return its upvalue index. As the recursive calls
    // unwind we'll construct a chain of upvalues leading to the captured variable.
    int resolved_upvalue_index = 0;
    if (resolve_upvalue(parser, fn_compiler->enclosing, name, &resolved_upvalue_index, is_constant)) {
        *upvalue_index = add_upvalue(parser, fn_compiler, (uint8_t)resolved_upvalue_index, false);
        return true;
    }

    // No matching local variable in any enclosing scope.
    return false;
}


static void add_local(Parser* parser, Token name) {
    if (parser->fn_compiler->local_count == 256) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many local variables in scope (max: 256)");
        return;
    }
    Local* local = &parser->fn_compiler->locals[parser->fn_compiler->local_count++];
    local->name = name;
    local->depth = parser->fn_compiler->scope_depth;
    local->is_initialized = false; // the variable has been declared but not yet defined
    local->is_captured = false;
}


// Sets the [is_initialized] flag on the last [count] local variables.
static void mark_locals_as_initialized(Parser* parser, int count, bool is_constant) {
    if (parser->fn_compiler->scope_depth > 0) {
        for (int i = 0; i < count; i++) {
            int index = parser->fn_compiler->local_count - 1 - i;
            parser->fn_compiler->locals[index].is_initialized = true;
            parser->fn_compiler->locals[index].is_constant = is_constant;
        }
    }
}


// Sets the [is_initialized] flag on the last local variable.
static void mark_local_as_initialized(Parser* parser, bool is_constant) {
    mark_locals_as_initialized(parser, 1, is_constant);
}


static void define_variable(Parser* parser, uint16_t index, Access access, bool is_constant) {
    if (parser->fn_compiler->scope_depth > 0) {
        mark_local_as_initialized(parser, is_constant);
        return;
    }

    PyroOpcode opcode = (access == PUBLIC) ? PYRO_OPCODE_DEFINE_PUB_GLOBAL : PYRO_OPCODE_DEFINE_PRI_GLOBAL;
    emit_byte(parser, opcode);
    emit_u16be(parser, index);
}


static void define_variables(Parser* parser, uint16_t* indexes, int count, Access access, bool is_constant) {
    if (parser->fn_compiler->scope_depth > 0) {
        mark_locals_as_initialized(parser, count, is_constant);
        return;
    }

    PyroOpcode opcode = (access == PUBLIC) ? PYRO_OPCODE_DEFINE_PUB_GLOBALS : PYRO_OPCODE_DEFINE_PRI_GLOBALS;
    emit_u8_u8(parser, opcode, count);
    for (int i = 0; i < count; i++) {
        emit_u16be(parser, indexes[i]);
    }
}


static void declare_variable(Parser* parser, Token name, bool is_constant) {
    if (parser->fn_compiler->scope_depth == 0) {
        if (is_constant) {
            log_global_constant(parser, name);
        }
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
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("'%.*s' has already been declared in this scope", name.length, name.start);
        }
    }

    add_local(parser, name);
}


// Called when declaring a variable or function parameter.
static uint16_t consume_variable_name(Parser* parser, const char* error_message, bool is_constant) {
    consume(parser, TOKEN_IDENTIFIER, error_message);
    declare_variable(parser, parser->previous_token, is_constant);

    // Local variables are referenced by index not by name so we don't need to add the name
    // of a local to the list of constants. This return value will simply be ignored.
    if (parser->fn_compiler->scope_depth > 0) {
        return 0;
    }

    // Global variables are referenced by name.
    return make_string_constant_from_identifier(parser, &parser->previous_token);
}


// Emits bytecode to discard all local variables at scope depth greater than or equal to
// [depth]. Returns the number of locals discarded. (This function doesn't decrement the local
// count as it's called directly by break statements.)
static int discard_locals(Parser* parser, int depth) {
    int local_count = parser->fn_compiler->local_count;

    while (local_count > 0 && parser->fn_compiler->locals[local_count - 1].depth >= depth) {
        if (parser->fn_compiler->locals[local_count - 1].is_captured) {
            emit_byte(parser, PYRO_OPCODE_CLOSE_UPVALUE);
        } else {
            emit_byte(parser, PYRO_OPCODE_POP);
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
static size_t emit_jump(Parser* parser, PyroOpcode instruction) {
    emit_byte(parser, instruction);
    emit_byte(parser, 0xff);
    emit_byte(parser, 0xff);
    return parser->fn_compiler->fn->code_count - 2;
}


// We call patch_jump() right before emitting the instruction we want the jump to land on.
static void patch_jump(Parser* parser, size_t index) {
    if (parser->vm->halt_flag) {
        return;
    }

    size_t jump = parser->fn_compiler->fn->code_count - index - 2;
    if (jump > UINT16_MAX) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("jump length exceeds maximum limit (max: %d bytes of bytecode)", UINT16_MAX);
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
            SYNTAX_ERROR_AT_NEXT_TOKEN("unpacked argument must be last argument");
        }
        if (match(parser, TOKEN_STAR)) {
            unpack = true;
        }
        parse_expression(parser, false);
        if (arg_count == 255) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many arguments (max: 255)");
        }
        arg_count++;
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after arguments");
    *unpack_last_argument = unpack;
    return arg_count;
}


// Emits bytecode to load the value of the named variable onto the stack.
static void emit_load_named_variable(Parser* parser, Token name) {
    bool is_constant;

    // Load a local variable.
    int local_index = 0;
    if (resolve_local(parser, parser->fn_compiler, &name, &local_index, &is_constant)) {
        switch (local_index) {
            case 0: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_0); break;
            case 1: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_1); break;
            case 2: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_2); break;
            case 3: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_3); break;
            case 4: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_4); break;
            case 5: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_5); break;
            case 6: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_6); break;
            case 7: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_7); break;
            case 8: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_8); break;
            case 9: emit_byte(parser, PYRO_OPCODE_GET_LOCAL_9); break;
            default:
                emit_u8_u8(parser, PYRO_OPCODE_GET_LOCAL, (uint8_t)local_index);
                break;
        }
        return;
    }

    // Load an upvalue.
    int upvalue_index;
    if (resolve_upvalue(parser, parser->fn_compiler, &name, &upvalue_index, &is_constant)) {
        emit_u8_u8(parser, PYRO_OPCODE_GET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    // Load a global variable.
    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, PYRO_OPCODE_GET_GLOBAL);
    emit_u16be(parser, const_index);
}


// Emits bytecode to set the named variable to the value on top of the stack.
static void emit_store_named_variable(Parser* parser, Token name) {
    bool is_constant = false;

    // Set a local variable.
    int local_index = 0;
    if (resolve_local(parser, parser->fn_compiler, &name, &local_index, &is_constant)) {
        if (is_constant) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid assignment to constant '%.*s'", name.length, name.start);
            return;
        }

        switch (local_index) {
            case 0: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_0); break;
            case 1: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_1); break;
            case 2: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_2); break;
            case 3: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_3); break;
            case 4: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_4); break;
            case 5: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_5); break;
            case 6: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_6); break;
            case 7: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_7); break;
            case 8: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_8); break;
            case 9: emit_byte(parser, PYRO_OPCODE_SET_LOCAL_9); break;
            default:
                emit_u8_u8(parser, PYRO_OPCODE_SET_LOCAL, (uint8_t)local_index);
                break;
        }

        return;
    }

    // Set an upvalue.
    int upvalue_index;
    if (resolve_upvalue(parser, parser->fn_compiler, &name, &upvalue_index, &is_constant)) {
        if (is_constant) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid assignment to constant '%.*s'", name.length, name.start);
            return;
        }

        emit_u8_u8(parser, PYRO_OPCODE_SET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    // Set a global variable.
    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, PYRO_OPCODE_SET_GLOBAL);
    emit_u16be(parser, const_index);
    log_global_assignment(parser, name);
}


static int64_t parse_hex_literal(Parser* parser) {
    char buffer[16 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 16) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many digits in hex literal (max: 16)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid hex literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 16);
    if (errno != 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid hex literal (out of range)");
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
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many digits in binary literal (max: 64)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid binary literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 2);
    if (errno != 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid binary literal (out of range)");
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
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many digits in octal literal (max: 22)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    if (count == 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid octal literal (zero digits)");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 8);
    if (errno != 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid octal literal (out of range)");
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
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many digits in integer literal (max: 20)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 10);
    if (errno != 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid integer literal (out of range)");
    }

    return value;
}


// So, figuring out the maximum possible length of a 64-bit IEEE 754 float literal is
// complicated. Possible answers [here][1] include 24 characters and... 1079 characters!
// Setting a 64 character maximum for now but I may want to revisit this at some point.
//
// [1]: https://stackoverflow.com/questions/1701055/
//
static double parse_float_literal(Parser* parser) {
    char buffer[64 + 1];
    size_t count = 0;

    for (size_t i = 0; i < parser->previous_token.length; i++) {
        if (parser->previous_token.start[i] == '_') {
            continue;
        }
        if (count == 64) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many digits in floating-point literal (max: 64)");
            return 0;
        }
        buffer[count++] = parser->previous_token.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    double value = strtod(buffer, NULL);
    if (errno != 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid floating-point literal (out of range)");
    }

    return value;
}


static uint32_t parse_rune_literal(Parser* parser) {
    // The longest valid character literal is a unicode escape sequence of the form:
    // '\UXXXXXXXX'
    const size_t max_length = 10;

    const char* start = parser->previous_token.start;
    size_t length = parser->previous_token.length;

    if (length == 0 || length > max_length) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid character literal");
        return 0;
    }

    char buffer[16];
    size_t count = pyro_process_backslashed_escapes(start, length, buffer);

    Utf8CodePoint cp;
    if (!pyro_read_utf8_codepoint((uint8_t*)buffer, count, &cp)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid character literal (not utf-8)");
        return 0;
    }

    if (cp.length != count) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid character literal (too many bytes)");
        return 0;
    }

    return cp.value;
}


static void parse_map_or_set_literal(Parser* parser) {
    bool is_map = true;
    uint16_t count = 0;

    do {
        if (check(parser, TOKEN_RIGHT_BRACE)) {
            break;
        }
        parse_expression(parser, false);
        if (count == 0) {
            if (match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, false);
            } else {
                is_map = false;
            }
        } else if (is_map) {
            consume(parser, TOKEN_EQUAL, "expected '=' after key");
            parse_expression(parser, false);
        }
        count++;
    } while (match(parser, TOKEN_COMMA));

    if (is_map) {
        consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after map literal");
        emit_byte(parser, PYRO_OPCODE_MAKE_MAP);
    } else {
        consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after set literal");
        emit_byte(parser, PYRO_OPCODE_MAKE_SET);
    }

    emit_u16be(parser, count);
}


static void parse_vec_literal(Parser* parser) {
    uint16_t count = 0;
    do {
        if (check(parser, TOKEN_RIGHT_BRACKET)) {
            break;
        }
        parse_expression(parser, true);
        count++;
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after vector literal");
    emit_byte(parser, PYRO_OPCODE_MAKE_VEC);
    emit_u16be(parser, count);
}


static void parse_variable_expression(Parser* parser, bool can_assign) {
    Token name = parser->previous_token;

    if (can_assign && match(parser, TOKEN_EQUAL)) {
        parse_expression(parser, true);
        emit_store_named_variable(parser, name);
    }

    else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
        emit_load_named_variable(parser, name);
        parse_expression(parser, true);
        emit_byte(parser, PYRO_OPCODE_BINARY_PLUS);
        emit_store_named_variable(parser, name);
    }

    else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
        emit_load_named_variable(parser, name);
        parse_expression(parser, true);
        emit_byte(parser, PYRO_OPCODE_BINARY_MINUS);
        emit_store_named_variable(parser, name);
    }

    else {
        emit_load_named_variable(parser, name);
    }
}


static void parse_default_value_expression(Parser* parser, const char* value_type) {
    if (match(parser, TOKEN_TRUE)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_TRUE);
    }

    else if (match(parser, TOKEN_FALSE)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_FALSE);
    }

    else if (match(parser, TOKEN_NULL)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
    }

    else if (match(parser, TOKEN_INT)) {
        int64_t value = parse_int_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_HEX_INT)) {
        int64_t value = parse_hex_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_BINARY_INT)) {
        int64_t value = parse_binary_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_OCTAL_INT)) {
        int64_t value = parse_octal_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_FLOAT)) {
        double value = parse_float_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_f64(value));
    }

    else if (match(parser, TOKEN_RAW_STRING)) {
        PyroStr* string = PyroStr_copy(
            parser->previous_token.start,
            parser->previous_token.length,
            false,
            parser->vm
        );
        emit_load_value_from_constant_table(parser, pyro_obj(string));
    }

    else if (match(parser, TOKEN_ESCAPED_STRING)) {
        PyroStr* string = PyroStr_copy(
            parser->previous_token.start,
            parser->previous_token.length,
            true,
            parser->vm
        );
        emit_load_value_from_constant_table(parser, pyro_obj(string));
    }

    else if (match(parser, TOKEN_RUNE)) {
        uint32_t codepoint = parse_rune_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_rune(codepoint));
    }

    else {
        SYNTAX_ERROR_AT_NEXT_TOKEN(
            "unexpected token '%.*s', a default %s value must be a simple literal",
            parser->next_token.length,
            parser->next_token.start,
            value_type
        );
    }
}


static void parse_if_expr(Parser* parser, bool can_assign) {
    parse_expression(parser, false);
    if (match_assignment_token(parser)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN(
            "invalid assignment in 'if' condition, wrap the assignment in '()' to allow"
        );
        return;
    }

    if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' after condition in 'if' expression")) {
        return;
    }

    size_t jump_to_false_branch = emit_jump(parser, PYRO_OPCODE_JUMP_IF_FALSE);
    emit_byte(parser, PYRO_OPCODE_POP);

    parse_expression(parser, true);
    size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP);

    if (!consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after then-clause in 'if' expression")) {
        return;
    }

    if (!consume(parser, TOKEN_ELSE, "expected 'else' in 'if' expression")) {
        return;
    }

    patch_jump(parser, jump_to_false_branch);
    emit_byte(parser, PYRO_OPCODE_POP);

    if (match(parser, TOKEN_IF)) {
        parse_if_expr(parser, false);
    } else {
        if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' after 'else' in 'if' expression")) {
            return;
        }
        parse_expression(parser, true);
        if (!consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after 'else' clause in 'if' expression")) {
            return;
        }
    }

    patch_jump(parser, jump_to_end);
}


static TokenType parse_primary_expr(Parser* parser, bool can_assign) {
    TokenType token_type = parser->next_token.type;

    if (match(parser, TOKEN_TRUE)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_TRUE);
    }

    else if (match(parser, TOKEN_FALSE)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_FALSE);
    }

    else if (match(parser, TOKEN_NULL)) {
        emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
    }

    else if (match(parser, TOKEN_INT)) {
        int64_t value = parse_int_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_HEX_INT)) {
        int64_t value = parse_hex_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_BINARY_INT)) {
        int64_t value = parse_binary_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_OCTAL_INT)) {
        int64_t value = parse_octal_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_i64(value));
    }

    else if (match(parser, TOKEN_FLOAT)) {
        double value = parse_float_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_f64(value));
    }

    else if (match(parser, TOKEN_LEFT_PAREN)) {
        if (match(parser, TOKEN_RIGHT_PAREN)) {
            emit_byte(parser, PYRO_OPCODE_MAKE_TUP);
            emit_u16be(parser, 0);
            return token_type;
        }

        parse_expression(parser, true);

        if (match(parser, TOKEN_COMMA)) {
            uint16_t count = 1;
            do {
                if (check(parser, TOKEN_RIGHT_PAREN)) {
                    break;
                }
                parse_expression(parser, true);
                count++;
            } while (match(parser, TOKEN_COMMA));
            consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after tuple literal");
            emit_byte(parser, PYRO_OPCODE_MAKE_TUP);
            emit_u16be(parser, count);
            return token_type;
        }

        consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after expression");
    }

    else if (match(parser, TOKEN_RAW_STRING)) {
        PyroStr* string = PyroStr_copy(
            parser->previous_token.start,
            parser->previous_token.length,
            false,
            parser->vm
        );
        emit_load_value_from_constant_table(parser, pyro_obj(string));
    }

    else if (match(parser, TOKEN_ESCAPED_STRING)) {
        PyroStr* string = PyroStr_copy(
            parser->previous_token.start,
            parser->previous_token.length,
            true,
            parser->vm
        );
        emit_load_value_from_constant_table(parser, pyro_obj(string));
    }

    else if (match(parser, TOKEN_STRING_FRAGMENT)) {
        PyroStr* string = PyroStr_copy(
            parser->previous_token.start,
            parser->previous_token.length,
            true,
            parser->vm
        );

        emit_load_value_from_constant_table(parser, pyro_obj(string));
        uint16_t string_count = 1;

        while (true) {
            parse_expression(parser, false);
            if (parser->vm->halt_flag) {
                return TOKEN_UNDEFINED;
            }

            PyroOpcode stringify_opcode = PYRO_OPCODE_STRINGIFY;

            if (check(parser, TOKEN_SEMICOLON)) {
                parser->lexer.format_specifier_mode = true;
                advance(parser);
                parser->lexer.format_specifier_mode = false;

                // The next token will be one of: TOKEN_FORMAT_SPECIFIER, TOKEN_ERROR.
                if (!consume(parser, TOKEN_FORMAT_SPECIFIER, "expected format specifier after ';'")) {
                    return TOKEN_UNDEFINED;
                }

                PyroStr* string = PyroStr_copy(
                    parser->previous_token.start,
                    parser->previous_token.length,
                    false,
                    parser->vm
                );

                emit_load_value_from_constant_table(parser, pyro_obj(string));
                stringify_opcode = PYRO_OPCODE_FORMAT;
            }

            emit_byte(parser, stringify_opcode);
            string_count++;

            parser->lexer.interpolated_string_mode = true;
            if (!consume(parser, TOKEN_RIGHT_BRACE, "expected closing '}' for interpolation element")) {
                return TOKEN_UNDEFINED;
            }
            parser->lexer.interpolated_string_mode = false;

            if (match(parser, TOKEN_STRING_FRAGMENT)) {
                PyroStr* string = PyroStr_copy(
                    parser->previous_token.start,
                    parser->previous_token.length,
                    true,
                    parser->vm
                );
                emit_load_value_from_constant_table(parser, pyro_obj(string));
                string_count++;
                continue;
            }

            if (match(parser, TOKEN_STRING_FRAGMENT_FINAL)) {
                PyroStr* string = PyroStr_copy(
                    parser->previous_token.start,
                    parser->previous_token.length,
                    true,
                    parser->vm
                );
                emit_load_value_from_constant_table(parser, pyro_obj(string));
                string_count++;
                break;
            }

            // We must have an unterminated string literal. The lexer will have returned
            // TOKEN_ERROR and the advance() function will have registered a syntax error.
            return TOKEN_UNDEFINED;
        }

        emit_u8_u16be(parser, PYRO_OPCODE_CONCAT_STRINGS, string_count);
    }

    else if (match(parser, TOKEN_RUNE)) {
        uint32_t codepoint = parse_rune_literal(parser);
        emit_load_value_from_constant_table(parser, pyro_rune(codepoint));
    }

    else if (match(parser, TOKEN_IDENTIFIER)) {
        parse_variable_expression(parser, can_assign);
    }

    else if (match(parser, TOKEN_SELF)) {
        if (parser->class_compiler == NULL) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'self' outside a method declaration");
        } else if (parser->fn_compiler->type == TYPE_STATIC_METHOD) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'self' in a static method");
        }
        emit_load_named_variable(parser, parser->previous_token);
    }

    else if (match(parser, TOKEN_SUPER)) {
        if (parser->class_compiler == NULL) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' outside a class");
        } else if (parser->fn_compiler->type == TYPE_STATIC_METHOD) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' in a static method");
        } else if (!parser->class_compiler->has_superclass) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'super' in a class with no superclass");
        }

        consume(parser, TOKEN_COLON, "expected ':' after 'super'");
        consume(parser, TOKEN_IDENTIFIER, "expected superclass method name");

        uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
        emit_load_named_variable(parser, make_syntoken("self"));    // load the instance

        if (match(parser, TOKEN_LEFT_PAREN)) {
            bool unpack_last_argument;
            uint8_t arg_count = parse_argument_list(parser, &unpack_last_argument);
            emit_load_named_variable(parser, make_syntoken("super"));   // load the superclass
            if (unpack_last_argument) {
                emit_byte(parser, PYRO_OPCODE_CALL_SUPER_METHOD_WITH_UNPACK);
            } else {
                emit_byte(parser, PYRO_OPCODE_CALL_SUPER_METHOD);
            }
            emit_u16be(parser, index);
            emit_byte(parser, arg_count);
        } else {
            emit_load_named_variable(parser, make_syntoken("super"));   // load the superclass
            emit_byte(parser, PYRO_OPCODE_GET_SUPER_METHOD);
            emit_u16be(parser, index);
        }
    }

    else if (match(parser, TOKEN_DEF)) {
        parse_function_definition(parser, TYPE_FUNCTION, make_syntoken("<lambda>"));
    }

    else if (match(parser, TOKEN_LEFT_BRACKET)) {
        parse_vec_literal(parser);
    }

    else if (match(parser, TOKEN_LEFT_BRACE)) {
        parse_map_or_set_literal(parser);
    }

    else if (match(parser, TOKEN_IF)) {
        parse_if_expr(parser, can_assign);
    }

    else if (match(parser, TOKEN_EOF)) {
        SYNTAX_ERROR_AT_NEXT_TOKEN("unexpected end of input, expected an expression");
    }

    else {
        SYNTAX_ERROR_AT_NEXT_TOKEN(
            "unexpected token '%.*s', expected an expression",
            parser->next_token.length,
            parser->next_token.start
        );
    }

    return token_type;
}


static void parse_call_expr(Parser* parser, bool can_assign) {
    TokenType last_token_type = parse_primary_expr(parser, can_assign);

    while (true) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            bool unpack_last_argument;
            uint8_t arg_count = parse_argument_list(parser, &unpack_last_argument);
            if (unpack_last_argument) {
                emit_u8_u8(parser, PYRO_OPCODE_CALL_VALUE_WITH_UNPACK, arg_count);
            } else {
                switch (arg_count) {
                    case 0: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_0); break;
                    case 1: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_1); break;
                    case 2: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_2); break;
                    case 3: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_3); break;
                    case 4: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_4); break;
                    case 5: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_5); break;
                    case 6: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_6); break;
                    case 7: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_7); break;
                    case 8: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_8); break;
                    case 9: emit_byte(parser, PYRO_OPCODE_CALL_VALUE_9); break;
                    default:
                        emit_u8_u8(parser, PYRO_OPCODE_CALL_VALUE, arg_count);
                        break;
                }
            }
        }

        else if (match(parser, TOKEN_LEFT_BRACKET)) {
            parse_expression(parser, true);
            consume(parser, TOKEN_RIGHT_BRACKET, "expected ']' after index");
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true);
                emit_byte(parser, PYRO_OPCODE_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, PYRO_OPCODE_DUP_2);
                emit_byte(parser, PYRO_OPCODE_GET_INDEX);
                parse_expression(parser, true);
                emit_byte(parser, PYRO_OPCODE_BINARY_PLUS);
                emit_byte(parser, PYRO_OPCODE_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, PYRO_OPCODE_DUP_2);
                emit_byte(parser, PYRO_OPCODE_GET_INDEX);
                parse_expression(parser, true);
                emit_byte(parser, PYRO_OPCODE_BINARY_MINUS);
                emit_byte(parser, PYRO_OPCODE_SET_INDEX);
            } else {
                emit_byte(parser, PYRO_OPCODE_GET_INDEX);
            }
        }

        else if (match(parser, TOKEN_DOT)) {
            PyroOpcode get_opcode = (last_token_type == TOKEN_SELF) ? PYRO_OPCODE_GET_FIELD : PYRO_OPCODE_GET_PUB_FIELD;
            PyroOpcode set_opcode = (last_token_type == TOKEN_SELF) ? PYRO_OPCODE_SET_FIELD : PYRO_OPCODE_SET_PUB_FIELD;
            consume(parser, TOKEN_IDENTIFIER, "expected a field name after '.'");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true);
                emit_u8_u16be(parser, set_opcode, index);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, PYRO_OPCODE_DUP);
                emit_u8_u16be(parser, get_opcode, index);
                parse_expression(parser, true);
                emit_byte(parser, PYRO_OPCODE_BINARY_PLUS);
                emit_u8_u16be(parser, set_opcode, index);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, PYRO_OPCODE_DUP);
                emit_u8_u16be(parser, get_opcode, index);
                parse_expression(parser, true);
                emit_byte(parser, PYRO_OPCODE_BINARY_MINUS);
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
                    PyroOpcode opcode = (last_token_type == TOKEN_SELF) ? PYRO_OPCODE_CALL_METHOD_WITH_UNPACK : PYRO_OPCODE_CALL_PUB_METHOD_WITH_UNPACK;
                    emit_u8_u16be(parser, opcode, index);
                    emit_byte(parser, arg_count);
                } else {
                    PyroOpcode opcode = (last_token_type == TOKEN_SELF) ? PYRO_OPCODE_CALL_METHOD : PYRO_OPCODE_CALL_PUB_METHOD;
                    emit_u8_u16be(parser, opcode, index);
                    emit_byte(parser, arg_count);
                }
            } else {
                PyroOpcode opcode = (last_token_type == TOKEN_SELF) ? PYRO_OPCODE_GET_METHOD : PYRO_OPCODE_GET_PUB_METHOD;
                emit_u8_u16be(parser, opcode, index);
            }
        }

        else if (match(parser, TOKEN_COLON_COLON)) {
            if (!consume(parser, TOKEN_IDENTIFIER, "expected a member name after '::'")) {
                break;
            }
            Token name = parser->previous_token;
            uint16_t index = make_string_constant_from_identifier(parser, &name);
            emit_u8_u16be(parser, PYRO_OPCODE_GET_MEMBER, index);
            if (match_assignment_token(parser)) {
                SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid assignment to '%.*s', cannot assign to module members", name.length, name.start);
            }
        }

        else {
            break;
        }

        // Set this to a null value after the first pass through the while-loop as we only
        // care about cases of [self.foo] or [self:foo].
        last_token_type = TOKEN_UNDEFINED;
    }
}


static void parse_try_expr(Parser* parser) {
    FnCompiler fn_compiler;
    if (!init_fn_compiler(parser, &fn_compiler, TYPE_TRY_EXPR, make_syntoken("try"))) {
        return;
    }

    begin_scope(parser);
    parse_unary_expr(parser, false);
    emit_byte(parser, PYRO_OPCODE_RETURN);

    PyroFn* fn = end_fn_compiler(parser);
    emit_u8_u16be(parser, PYRO_OPCODE_MAKE_CLOSURE, add_value_to_constant_table(parser, pyro_obj(fn)));

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, fn_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, fn_compiler.upvalues[i].index);
    }
}


static void parse_power_expr(Parser* parser, bool can_assign) {
    parse_call_expr(parser, can_assign);
    if (match(parser, TOKEN_STAR_STAR)) {
        parse_unary_expr(parser, false);
        emit_byte(parser, PYRO_OPCODE_BINARY_STAR_STAR);
    }
}


static void parse_unary_expr(Parser* parser, bool can_assign) {
    if (match(parser, TOKEN_MINUS)) {
        parse_unary_expr(parser, false);
        emit_byte(parser, PYRO_OPCODE_UNARY_MINUS);
    } else if (match(parser, TOKEN_PLUS)) {
        parse_unary_expr(parser, false);
        emit_byte(parser, PYRO_OPCODE_UNARY_PLUS);
    } else if (match(parser, TOKEN_BANG)) {
        parse_unary_expr(parser, false);
        emit_byte(parser, PYRO_OPCODE_UNARY_BANG);
    } else if (match(parser, TOKEN_TRY)) {
        parse_try_expr(parser);
        emit_byte(parser, PYRO_OPCODE_TRY);
    } else if (match(parser, TOKEN_TILDE)) {
        parse_unary_expr(parser, false);
        emit_byte(parser, PYRO_OPCODE_UNARY_TILDE);
    } else {
        parse_power_expr(parser, can_assign);
    }
}


static void parse_bitwise_expr(Parser* parser, bool can_assign) {
    parse_unary_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_CARET)) {
            parse_unary_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_CARET);
        } else if (match(parser, TOKEN_AMP)) {
            parse_unary_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_AMP);
        } else if (match(parser, TOKEN_BAR)) {
            parse_unary_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_BAR);
        } else if (match(parser, TOKEN_LESS_LESS)) {
            parse_unary_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_LESS_LESS);
        } else if (match(parser, TOKEN_GREATER_GREATER)) {
            parse_unary_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_GREATER_GREATER);
        } else {
            break;
        }
    }
}


static void parse_multiplicative_expr(Parser* parser, bool can_assign) {
    parse_bitwise_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_STAR)) {
            parse_bitwise_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_STAR);
        } else if (match(parser, TOKEN_SLASH)) {
            parse_bitwise_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_SLASH);
        } else if (match(parser, TOKEN_SLASH_SLASH)) {
            parse_bitwise_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_SLASH_SLASH);
        } else if (match(parser, TOKEN_PERCENT)) {
            parse_bitwise_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_PERCENT);
        } else {
            break;
        }
    }
}


static void parse_additive_expr(Parser* parser, bool can_assign) {
    parse_multiplicative_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_PLUS)) {
            parse_multiplicative_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_PLUS);
        } else if (match(parser, TOKEN_MINUS)) {
            parse_multiplicative_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_MINUS);
        } else if (match(parser, TOKEN_I64_ADD)) {
            parse_multiplicative_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_I64_ADD);
        } else {
            break;
        }
    }
}


static void parse_comparative_expr(Parser* parser, bool can_assign) {
    parse_additive_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_GREATER)) {
            parse_additive_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_GREATER);
        } else if (match(parser, TOKEN_GREATER_EQUAL)) {
            parse_additive_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_GREATER_EQUAL);
        } else if (match(parser, TOKEN_LESS)) {
            parse_additive_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_LESS);
        } else if (match(parser, TOKEN_LESS_EQUAL)) {
            parse_additive_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_LESS_EQUAL);
        } else if (match(parser, TOKEN_IN)) {
            parse_additive_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_IN);
        } else {
            break;
        }
    }
}


static void parse_equality_expr(Parser* parser, bool can_assign) {
    parse_comparative_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_EQUAL_EQUAL)) {
            parse_comparative_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_EQUAL_EQUAL);
        } else if (match(parser, TOKEN_BANG_EQUAL)) {
            parse_comparative_expr(parser, false);
            emit_byte(parser, PYRO_OPCODE_BINARY_BANG_EQUAL);
        } else {
            break;
        }
    }
}


// The logical operators && and || are short-circuiting and leave behind a truthy or falsey
// operand to indicate their result.
static void parse_logical_expr(Parser* parser, bool can_assign) {
    parse_equality_expr(parser, can_assign);
    while (true) {
        if (match(parser, TOKEN_AMP_AMP)) {
            size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP_IF_FALSE);
            emit_byte(parser, PYRO_OPCODE_POP);
            parse_equality_expr(parser, false);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_BAR_BAR)) {
            size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP_IF_TRUE);
            emit_byte(parser, PYRO_OPCODE_POP);
            parse_equality_expr(parser, false);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_HOOK_HOOK)) {
            size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP_IF_NOT_NULL);
            emit_byte(parser, PYRO_OPCODE_POP);
            parse_equality_expr(parser, false);
            patch_jump(parser, jump_to_end);
        } else if (match(parser, TOKEN_BANG_BANG)) {
            size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP_IF_NOT_ERR);
            emit_byte(parser, PYRO_OPCODE_POP);
            parse_equality_expr(parser, false);
            patch_jump(parser, jump_to_end);
        } else {
            break;
        }
    }
}


static void parse_conditional_expr(Parser* parser, bool can_assign) {
    parse_logical_expr(parser, can_assign);
    if (match(parser, TOKEN_HOOK)) {
        size_t jump_to_false_branch = emit_jump(parser, PYRO_OPCODE_JUMP_IF_FALSE);
        emit_byte(parser, PYRO_OPCODE_POP);
        parse_logical_expr(parser, false);
        size_t jump_to_end = emit_jump(parser, PYRO_OPCODE_JUMP);
        consume(parser, TOKEN_COLON_BAR, "expected ':|' after condition");
        patch_jump(parser, jump_to_false_branch);
        emit_byte(parser, PYRO_OPCODE_POP);
        parse_logical_expr(parser, false);
        patch_jump(parser, jump_to_end);
    }
}


static void parse_assignment_expr(Parser* parser, bool can_assign) {
    parse_conditional_expr(parser, can_assign);

    if (can_assign && match_assignment_token(parser)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid assignment target");
    }
}


static void parse_expression(Parser* parser, bool can_assign) {
    parse_assignment_expr(parser, can_assign);
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
    int count = 0;
    if (match(parser, TOKEN_SEMICOLON)) {
        emit_u8_u8(parser, PYRO_OPCODE_ECHO, count);
        return;
    }

    parse_expression(parser, true);
    count++;

    while (match(parser, TOKEN_COMMA)) {
        if (count == 255) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many arguments for 'echo' (max: 255)");
            return;
        }
        parse_expression(parser, true);
        count++;
    }

    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_u8_u8(parser, PYRO_OPCODE_ECHO, count);
}


static void parse_assert_stmt(Parser* parser) {
    parse_expression(parser, false);
    if (match_assignment_token(parser)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN(
            "invalid assignment in 'assert' statement, wrap the assignment in '()' to allow"
        );
        return;
    }
    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(parser, PYRO_OPCODE_ASSERT);
}


static void parse_typedef_stmt(Parser* parser) {
    if (!consume(parser, TOKEN_IDENTIFIER, "expected type name after 'typedef'")) {
        return;
    }
    parse_type(parser);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after typedef statement");
}


static void parse_expression_stmt(Parser* parser) {
    parse_expression(parser, true);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    emit_byte(parser, PYRO_OPCODE_POP);
}


static void parse_unpacking_declaration(Parser* parser, Access access, bool is_constant) {
    const int var_name_capacity = 16;
    uint16_t var_name_indexes[16];
    int var_name_count = 0;

    do {
        if (var_name_count == var_name_capacity) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many variable names to unpack (max: %d)", var_name_capacity);
            return;
        }
        var_name_indexes[var_name_count++] = consume_variable_name(parser, "expected variable name", is_constant);
        if (match(parser, TOKEN_COLON)) {
            parse_type(parser);
        }
    } while (match(parser, TOKEN_COMMA));

    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after variable names");
    consume(parser, TOKEN_EQUAL, "expected '=' after variable list");

    parse_expression(parser, true);
    emit_u8_u8(parser, PYRO_OPCODE_UNPACK, var_name_count);

    define_variables(parser, var_name_indexes, var_name_count, access, is_constant);
}


static void parse_var_declaration(Parser* parser, Access access, bool is_constant) {
    do {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            parse_unpacking_declaration(parser, access, is_constant);
        } else {
            uint16_t index = consume_variable_name(parser, "expected variable name", is_constant);
            if (match(parser, TOKEN_COLON)) {
                parse_type(parser);
            }
            if (match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true);
            } else {
                emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
            }
            define_variable(parser, index, access, is_constant);
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "expected ';' after variable declaration");
}


static void parse_import_stmt(Parser* parser) {
    // We'll use these arrays if we're explicitly importing named top-level members.
    Token member_names[64];
    uint16_t member_name_indexes[64];
    int member_name_count = 0;

    // Read the first module name.
    if (!consume(parser, TOKEN_IDENTIFIER, "expected a module name after 'import'")) return;
    uint16_t module_name_index = make_string_constant_from_identifier(parser, &parser->previous_token);
    emit_u8_u16be(parser, PYRO_OPCODE_LOAD_CONSTANT, module_name_index);
    Token module_name = parser->previous_token;
    int module_count = 1;

    // Read one ::module or ::{...} chunk per iteration.
    while (match(parser, TOKEN_COLON_COLON)) {
        if (match(parser, TOKEN_LEFT_BRACE)) {
            do {
                if (!consume(parser, TOKEN_IDENTIFIER, "expected member name in import statement")) return;
                member_name_indexes[member_name_count] = make_string_constant_from_identifier(parser, &parser->previous_token);
                emit_u8_u16be(parser, PYRO_OPCODE_LOAD_CONSTANT, member_name_indexes[member_name_count]);
                member_names[member_name_count] = parser->previous_token;
                member_name_count++;
                if (member_name_count > 64) {
                    SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many member names in import statement (max: 64)");
                    return;
                }
            } while (match(parser, TOKEN_COMMA));
            if (!consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after member names in import statement")) return;
            break;
        }
        if (!consume(parser, TOKEN_IDENTIFIER, "expected module name in import statement")) return;
        module_name_index = make_string_constant_from_identifier(parser, &parser->previous_token);
        emit_u8_u16be(parser, PYRO_OPCODE_LOAD_CONSTANT, module_name_index);
        module_name = parser->previous_token;
        module_count++;
    }

    if (module_count > 255) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many module names in import statement (max: 255)");
        return;
    }

    if (member_name_count == 0) {
        emit_byte(parser, PYRO_OPCODE_IMPORT_MODULE);
        emit_byte(parser, module_count);
    } else {
        emit_byte(parser, PYRO_OPCODE_IMPORT_NAMED_MEMBERS);
        emit_byte(parser, module_count);
        emit_byte(parser, member_name_count);
    }

    int alias_count = 0;
    if (match(parser, TOKEN_AS)) {
        do {
            if (!consume(parser, TOKEN_IDENTIFIER, "expected alias name in import statement")) return;
            module_name_index = make_string_constant_from_identifier(parser, &parser->previous_token);
            module_name = parser->previous_token;
            member_names[alias_count] = parser->previous_token;
            member_name_indexes[alias_count] = module_name_index;
            alias_count++;
            if (alias_count > 16) {
                SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many alias names in import statement (max: 16)");
                return;
            }
        } while (match(parser, TOKEN_COMMA));
    }

    if (member_name_count > 0) {
        if (alias_count > 0 && alias_count != member_name_count) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("alias and member numbers do not match in import statement");
            return;
        }
        for (int i = 0; i < member_name_count; i++) {
            declare_variable(parser, member_names[i], false);
        }
        define_variables(parser, member_name_indexes, member_name_count, PRIVATE, false);
    } else {
        if (alias_count > 1) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many alias names in import statement");
            return;
        }
        declare_variable(parser, module_name, false);
        define_variable(parser, module_name_index, PRIVATE, false);
    }

    consume(parser, TOKEN_SEMICOLON, "expected ';' after import statement");
}


static void parse_block(Parser* parser) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        parse_statement(parser);
        if (parser->vm->halt_flag) {
            break;
        }
    }
    consume(parser, TOKEN_RIGHT_BRACE, "expected '}' after block");
}


static void parse_if_stmt(Parser* parser) {
    // Push a new scope for condition-scoped variables.
    begin_scope(parser);

    // Parse condition-scoped variables as locals.
    if (match(parser, TOKEN_VAR)) {
        parse_var_declaration(parser, PRIVATE, false);
    } else if (match(parser, TOKEN_LET)) {
        parse_var_declaration(parser, PRIVATE, true);
    }

    // Parse the condition.
    parse_expression(parser, false);
    if (match_assignment_token(parser)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN(
            "assignment is not allowed in 'if' conditions, wrap in '()' to enable"
        );
        return;
    }

    // Jump over the 'then' block if the condition is false.
    size_t jump_over_then = emit_jump(parser, PYRO_OPCODE_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the 'then' block.
    if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' after condition in 'if' statement")) {
        return;
    }

    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // At the end of the 'then' block, unconditionally jump over the 'else' block.
    size_t jump_over_else = emit_jump(parser, PYRO_OPCODE_JUMP);

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

    // Pop the scope surrounding any condition-scoped variables.
    end_scope(parser);
}


// Emits an instruction to jump backwards in the bytecode to the index identified by
// [start_bytecode_count].
static void emit_loop(Parser* parser, size_t start_bytecode_count) {
    emit_byte(parser, PYRO_OPCODE_JUMP_BACK);

    size_t offset = parser->fn_compiler->fn->code_count - start_bytecode_count + 2;
    if (offset > UINT16_MAX) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("loop body is too large (max: %d bytes of bytecode)", UINT16_MAX);
    }

    emit_byte(parser, (offset >> 8) & 0xff);
    emit_byte(parser, offset & 0xff);
}


static void parse_for_in_stmt(Parser* parser) {
    // Push a new scope to wrap a dummy local variable pointing to the iterator object.
    begin_scope(parser);

    // Support unpacking syntax for up to [loop_vars_capacity] names.
    const size_t loop_vars_capacity = 12;
    Token loop_vars[12];
    size_t loop_vars_count = 0;
    bool unpack_vars = false;

    if (match(parser, TOKEN_LEFT_PAREN)) {
        unpack_vars = true;
        do {
            if (loop_vars_count == loop_vars_capacity) {
                SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many variable names to unpack (max: %zu)", loop_vars_capacity);
                return;
            }
            consume(parser, TOKEN_IDENTIFIER, "expected loop variable name");
            loop_vars[loop_vars_count++] = parser->previous_token;
        } while (match(parser, TOKEN_COMMA));
        consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after variable names");
    } else {
        consume(parser, TOKEN_IDENTIFIER, "expected loop variable name");
        loop_vars[0] = parser->previous_token;
        loop_vars_count++;
    }

    consume(parser, TOKEN_IN, "expected keyword 'in'");

    // This is the object we'll be iterating over.
    parse_expression(parser, true);
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before the loop body");

    // Replace the object on top of the stack with the result of calling :$iter() on it.
    emit_byte(parser, PYRO_OPCODE_GET_ITERATOR);
    add_local(parser, make_syntoken("*iterator*"));

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_bytecode_count = parser->fn_compiler->fn->code_count;
    loop.start_scope_depth = parser->fn_compiler->scope_depth;
    loop.start_with_block_depth = parser->fn_compiler->with_block_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    emit_byte(parser, PYRO_OPCODE_GET_NEXT_FROM_ITERATOR);
    size_t exit_jump_index = emit_jump(parser, PYRO_OPCODE_JUMP_IF_ERR);

    begin_scope(parser);
    if (unpack_vars) {
        emit_u8_u8(parser, PYRO_OPCODE_UNPACK, loop_vars_count);
    }
    for (size_t i = 0; i < loop_vars_count; i++) {
        add_local(parser, loop_vars[i]);
        mark_local_as_initialized(parser, false);
    }
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_bytecode_count);

    // Backpatch the destination for the exit jump.
    patch_jump(parser, exit_jump_index);

    // Pop the error object that caused the loop to exit.
    emit_byte(parser, PYRO_OPCODE_POP);

    // If there were any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_bytecode_count;
        PyroFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == PYRO_OPCODE_BREAK) {
                fn->code[ip] = PYRO_OPCODE_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + PyroFn_opcode_argcount(fn, ip);
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
        parse_var_declaration(parser, PRIVATE, false);
    } else if (match(parser, TOKEN_LET)) {
        parse_var_declaration(parser, PRIVATE, true);
    } else {
        parse_expression_stmt(parser);
    }

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_bytecode_count = parser->fn_compiler->fn->code_count;
    loop.start_scope_depth = parser->fn_compiler->scope_depth;
    loop.start_with_block_depth = parser->fn_compiler->with_block_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Parse the (optional) condition.
    bool loop_has_condition = false;
    size_t exit_jump_index = 0;
    if (!match(parser, TOKEN_SEMICOLON)) {
        loop_has_condition = true;
        parse_expression(parser, false);
        if (match_assignment_token(parser)) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN(
                "assignment is not allowed in loop conditions, wrap in '()' to enable"
            );
            return;
        }
        consume(parser, TOKEN_SEMICOLON, "expected ';' after loop condition");
        exit_jump_index = emit_jump(parser, PYRO_OPCODE_POP_JUMP_IF_FALSE);
    }

    // Parse the (optional) increment clause.
    if (!match(parser, TOKEN_LEFT_BRACE)) {
        size_t body_jump_index = emit_jump(parser, PYRO_OPCODE_JUMP);

        size_t increment_index = parser->fn_compiler->fn->code_count;
        parse_expression(parser, true);
        emit_byte(parser, PYRO_OPCODE_POP);
        consume(parser, TOKEN_LEFT_BRACE, "expected '{' before loop body");

        emit_loop(parser, loop.start_bytecode_count);
        loop.start_bytecode_count = increment_index;
        patch_jump(parser, body_jump_index);
    }

    // Emit the bytecode for the block.
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_bytecode_count);

    // Backpatch the destination for the exit jump.
    if (loop_has_condition) {
        patch_jump(parser, exit_jump_index);
    }

    // If there were any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_bytecode_count;
        PyroFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == PYRO_OPCODE_BREAK) {
                fn->code[ip] = PYRO_OPCODE_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + PyroFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;

    // Pop the scope surrounding the loop variables.
    end_scope(parser);
}


static void parse_infinite_loop_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_bytecode_count = parser->fn_compiler->fn->code_count;
    loop.start_scope_depth = parser->fn_compiler->scope_depth;
    loop.start_with_block_depth = parser->fn_compiler->with_block_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Emit the bytecode for the block.
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_bytecode_count);

    // If we found any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_bytecode_count;
        PyroFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == PYRO_OPCODE_BREAK) {
                fn->code[ip] = PYRO_OPCODE_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + PyroFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;
}


static void parse_while_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_bytecode_count = parser->fn_compiler->fn->code_count;
    loop.start_scope_depth = parser->fn_compiler->scope_depth;
    loop.start_with_block_depth = parser->fn_compiler->with_block_depth;
    loop.had_break = false;
    loop.enclosing = parser->fn_compiler->loop_compiler;
    parser->fn_compiler->loop_compiler = &loop;

    // Parse the condition.
    parse_expression(parser, false);
    if (match_assignment_token(parser)) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN(
            "assignment is not allowed in while conditions, wrap in '()' to enable"
        );
        return;
    }
    size_t exit_jump_index = emit_jump(parser, PYRO_OPCODE_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the block.
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before loop body");
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_bytecode_count);

    // Backpatch the destination for the exit jump.
    patch_jump(parser, exit_jump_index);

    // If we found any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_bytecode_count;
        PyroFn* fn = parser->fn_compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == PYRO_OPCODE_BREAK) {
                fn->code[ip] = PYRO_OPCODE_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + PyroFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->fn_compiler->loop_compiler = loop.enclosing;
}


static void parse_with_stmt(Parser* parser) {
    // Support unpacking syntax for up to [var_names_capacity] names.
    const size_t var_names_capacity = 12;
    Token var_names[12];
    size_t var_names_count = 0;
    bool unpack_vars = false;

    if (match(parser, TOKEN_LEFT_PAREN)) {
        unpack_vars = true;
        do {
            if (var_names_count == var_names_capacity) {
                SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many variable names to unpack (max: %zu)", var_names_capacity);
                return;
            }
            consume(parser, TOKEN_IDENTIFIER, "expected a variable name");
            var_names[var_names_count++] = parser->previous_token;
        } while (match(parser, TOKEN_COMMA));
        consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after variable names");
    } else {
        consume(parser, TOKEN_IDENTIFIER, "expected a variable name after 'with'");
        var_names[0] = parser->previous_token;
        var_names_count++;
    }

    if (!consume(parser, TOKEN_EQUAL, "expected '=' in 'with' statement")) {
        return;
    }

    parse_expression(parser, true);
    emit_byte(parser, PYRO_OPCODE_START_WITH);
    parser->fn_compiler->with_block_depth++;
    if (unpack_vars) {
        emit_u8_u8(parser, PYRO_OPCODE_UNPACK, var_names_count);
    }

    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before 'with' statement body");
    begin_scope(parser);
    for (size_t i = 0; i < var_names_count; i++) {
        add_local(parser, var_names[i]);
        mark_local_as_initialized(parser, false);
    }
    parse_block(parser);
    end_scope(parser);

    emit_byte(parser, PYRO_OPCODE_END_WITH);
    parser->fn_compiler->with_block_depth--;
}


// This helper parses a function definition, i.e. the bit after the name that looks like
// (...){...}. It emits the bytecode to create an PyroClosure and leave it on top of the stack.
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
            SYNTAX_ERROR_AT_NEXT_TOKEN("too many parameters (max: 255)");
        }
        if (fn_compiler.fn->is_variadic) {
            SYNTAX_ERROR_AT_NEXT_TOKEN("variadic parameter must be final parameter");
        }
        if (match(parser, TOKEN_STAR)) {
            fn_compiler.fn->is_variadic = true;
        }
        uint16_t index = consume_variable_name(parser, "expected parameter name", false);
        define_variable(parser, index, PRIVATE, false);
        fn_compiler.fn->arity++;
        if (match(parser, TOKEN_COLON)) {
            parse_type(parser);
        }
        if (default_value_count > 0 && !check(parser, TOKEN_EQUAL)) {
            SYNTAX_ERROR_AT_NEXT_TOKEN("missing default value for parameter");
            return;
        }
        if (match(parser, TOKEN_EQUAL)) {
            if (fn_compiler.fn->is_variadic) {
                SYNTAX_ERROR_AT_PREVIOUS_TOKEN("a variadic function cannot have default argument values");
            }
            parser->fn_compiler = fn_compiler.enclosing;
            parse_default_value_expression(parser, "argument");
            parser->fn_compiler = &fn_compiler;
            default_value_count += 1;
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_PAREN, "expected ')' after function parameters");

    if (match(parser, TOKEN_RIGHT_ARROW)) {
        parse_type(parser);
    }

    // Compile the body.
    consume(parser, TOKEN_LEFT_BRACE, "expected '{' before function body");
    parse_block(parser);

    // Create the function object.
    PyroFn* fn = end_fn_compiler(parser);

    // Add the function to the current function's constant table.
    uint16_t index = add_value_to_constant_table(parser, pyro_obj(fn));

    // Emit the bytecode to load the function onto the stack as an PyroClosure.
    if (default_value_count > 0) {
        emit_u8_u16be(parser, PYRO_OPCODE_MAKE_CLOSURE_WITH_DEF_ARGS, index);
        emit_byte(parser, default_value_count);
    } else {
        emit_u8_u16be(parser, PYRO_OPCODE_MAKE_CLOSURE, index);
    }

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, fn_compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, fn_compiler.upvalues[i].index);
    }
}


static void parse_function_declaration(Parser* parser, Access access) {
    uint16_t index = consume_variable_name(parser, "expected a function name", false);
    mark_local_as_initialized(parser, false);
    parse_function_definition(parser, TYPE_FUNCTION, parser->previous_token);
    define_variable(parser, index, access, false);
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
        emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_PUB_METHOD, index);
    } else if (access == PRIVATE) {
        emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_PRI_METHOD, index);
    } else {
        emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_STATIC_METHOD, index);
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
                parse_expression(parser, true);
            } else {
                parse_default_value_expression(parser, "field");
            }
        } else {
            emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
        }

        if (access == PUBLIC) {
            emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_PUB_FIELD, index);
        } else if (access == PRIVATE) {
            emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_PRI_FIELD, index);
        } else {
            emit_u8_u16be(parser, PYRO_OPCODE_DEFINE_STATIC_FIELD, index);
        }
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "expected ';' after field declaration");
}


static void parse_enum_declaration(Parser* parser, Access access) {
    if (!consume(parser, TOKEN_IDENTIFIER, "expected enum name")) {
        return;
    }

    // Token enum_name = parser->previous_token;

    uint16_t enum_name_index = make_string_constant_from_identifier(parser, &parser->previous_token);
    declare_variable(parser, parser->previous_token, false);

    if (!consume(parser, TOKEN_LEFT_BRACE, "expected '{' before enum body")) {
        return;
    }

    uint16_t value_count = 0;

    while (true) {
        if (match(parser, TOKEN_RIGHT_BRACE)) {
            break;
        }

        if (!consume(parser, TOKEN_IDENTIFIER, "expected enum value name")) {
            return;
        }

        uint16_t value_name_index = make_string_constant_from_identifier(parser, &parser->previous_token);
        if (parser->vm->panic_flag) {
            return;
        }

        emit_load_value_from_constant_table_at_index(parser, value_name_index);

        if (match(parser, TOKEN_EQUAL)) {
            parse_expression(parser, false);
        } else {
            emit_byte(parser, PYRO_OPCODE_LOAD_NULL);
        }

        value_count++;

        if (match(parser, TOKEN_RIGHT_BRACE)) {
            break;
        }

        if (!consume(parser, TOKEN_COMMA, "expected ',' after enum value")) {
            return;
        }
    }

    if (value_count == 0) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid enum: zero values");
        return;
    }

    emit_u8_u16be(parser, PYRO_OPCODE_MAKE_ENUM, value_count);
    define_variable(parser, enum_name_index, access, false);
}


static void parse_class_declaration(Parser* parser, Access access) {
    consume(parser, TOKEN_IDENTIFIER, "expected class name");
    Token class_name = parser->previous_token;

    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous_token);
    declare_variable(parser, parser->previous_token, false);

    emit_u8_u16be(parser, PYRO_OPCODE_MAKE_CLASS, index);
    define_variable(parser, index, access, false);

    // Push a new class compiler onto the implicit linked-list stack.
    ClassCompiler class_compiler;
    class_compiler.name = parser->previous_token;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = parser->class_compiler;
    parser->class_compiler = &class_compiler;

    if (match(parser, TOKEN_EXTENDS) || match(parser, TOKEN_LESS)) {
        consume(parser, TOKEN_IDENTIFIER, "expected superclass name");
        emit_load_named_variable(parser, parser->previous_token);

        // We declare 'super' as a local variable in a new lexical scope wrapping the method
        // declarations so it can be captured by the upvalue machinery.
        begin_scope(parser);
        add_local(parser, make_syntoken("super"));
        define_variable(parser, 0, PRIVATE, false);

        emit_load_named_variable(parser, class_name);
        emit_byte(parser, PYRO_OPCODE_INHERIT);
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
                SYNTAX_ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'pub'");
                return;
            }
        } else if (match(parser, TOKEN_PRI)) {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, PRIVATE);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, PRIVATE);
            } else {
                SYNTAX_ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'pri'");
                return;
            }
        } else if (match(parser, TOKEN_STATIC)) {
            if (match(parser, TOKEN_DEF)) {
                parse_method_declaration(parser, STATIC);
            } else if (match(parser, TOKEN_VAR)) {
                parse_field_declaration(parser, STATIC);
            } else {
                SYNTAX_ERROR_AT_NEXT_TOKEN("expected field or method declaration after 'static'");
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
    emit_byte(parser, PYRO_OPCODE_POP); // pop the class object

    if (class_compiler.has_superclass) {
        end_scope(parser);
    }

    // Pop the class compiler.
    parser->class_compiler = class_compiler.enclosing;
}


static void parse_return_stmt(Parser* parser) {
    if (parser->fn_compiler->type == TYPE_MODULE) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("can't return from module-level code");
        return;
    }

    if (match(parser, TOKEN_SEMICOLON)) {
        emit_naked_return(parser);
        return;
    }

    if (parser->fn_compiler->type == TYPE_INIT_METHOD) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("can't return a value from an initializer");
        return;
    }

    parse_expression(parser, true);
    size_t count = 1;

    while (match(parser, TOKEN_COMMA)) {
        parse_expression(parser, true);
        count++;
    }

    if (count > 255) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("too many arguments for 'return' (max: 255)");
    }

    consume(parser, TOKEN_SEMICOLON, "expected ';' after return statement");

    if (count == 1) {
        emit_byte(parser, PYRO_OPCODE_RETURN);
    } else {
        emit_u8_u8(parser, PYRO_OPCODE_RETURN_TUPLE, (uint8_t)count);
    }
}


static void parse_break_stmt(Parser* parser) {
    if (parser->fn_compiler->loop_compiler == NULL) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'break' outside a loop");
        return;
    }
    parser->fn_compiler->loop_compiler->had_break = true;

    discard_locals(parser, parser->fn_compiler->loop_compiler->start_scope_depth + 1);

    size_t with_block_depth = parser->fn_compiler->with_block_depth;
    while (with_block_depth > parser->fn_compiler->loop_compiler->start_with_block_depth) {
        emit_byte(parser, PYRO_OPCODE_END_WITH);
        with_block_depth--;
    }

    emit_jump(parser, PYRO_OPCODE_BREAK);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after 'break'");
}


static void parse_continue_stmt(Parser* parser) {
    if (parser->fn_compiler->loop_compiler == NULL) {
        SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'continue' outside a loop");
        return;
    }

    discard_locals(parser, parser->fn_compiler->loop_compiler->start_scope_depth + 1);

    size_t with_block_depth = parser->fn_compiler->with_block_depth;
    while (with_block_depth > parser->fn_compiler->loop_compiler->start_with_block_depth) {
        emit_byte(parser, PYRO_OPCODE_END_WITH);
        with_block_depth--;
    }

    emit_loop(parser, parser->fn_compiler->loop_compiler->start_bytecode_count);
    consume(parser, TOKEN_SEMICOLON, "expected ';' after 'continue'");
}


static void parse_statement(Parser* parser) {
    if (match(parser, TOKEN_SEMICOLON)) {
        return;
    }

    if (match(parser, TOKEN_PUB)) {
        if (parser->fn_compiler->type != TYPE_MODULE || parser->fn_compiler->scope_depth > 0) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'pub', only valid at global scope");
            return;
        }
        if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PUBLIC, false);
        } else if (match(parser, TOKEN_LET)) {
            parse_var_declaration(parser, PUBLIC, true);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PUBLIC);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PUBLIC);
        } else if (match(parser, TOKEN_ENUM)) {
            parse_enum_declaration(parser, PUBLIC);
        } else {
            SYNTAX_ERROR_AT_NEXT_TOKEN("unexpected token after 'pub'");
            return;
        }
    }

    else if (match(parser, TOKEN_PRI)) {
        if (parser->fn_compiler->type != TYPE_MODULE || parser->fn_compiler->scope_depth > 0) {
            SYNTAX_ERROR_AT_PREVIOUS_TOKEN("invalid use of 'pri', only valid at global scope");
            return;
        }
        if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PRIVATE, false);
        } else if (match(parser, TOKEN_LET)) {
            parse_var_declaration(parser, PRIVATE, true);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_ENUM)) {
            parse_enum_declaration(parser, PRIVATE);
        } else {
            SYNTAX_ERROR_AT_NEXT_TOKEN("unexpected token after 'pri'");
            return;
        }
    }

    else {
        if (match(parser, TOKEN_LEFT_BRACE)) {
            begin_scope(parser);
            parse_block(parser);
            end_scope(parser);
        } else if (match(parser, TOKEN_VAR)) {
            parse_var_declaration(parser, PRIVATE, false);
        } else if (match(parser, TOKEN_LET)) {
            parse_var_declaration(parser, PRIVATE, true);
        } else if (match(parser, TOKEN_DEF)) {
            parse_function_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_CLASS)) {
            parse_class_declaration(parser, PRIVATE);
        } else if (match(parser, TOKEN_ENUM)) {
            parse_enum_declaration(parser, PRIVATE);
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


/* ------------------------------------- */
/*  Log Global Declarations/Assignments  */
/* ------------------------------------- */


static void log_global_constant(Parser* parser, Token name) {
    GlobalConstant* global_constant = pyro_realloc(parser->vm, NULL, 0, sizeof(GlobalConstant));
    if (!global_constant) {
        pyro_panic(parser->vm, "out of memory");
        return;
    }

    global_constant->name = name;
    global_constant->next = NULL;

    if (parser->global_constants) {
        global_constant->next = parser->global_constants;
        parser->global_constants = global_constant;
    } else {
        parser->global_constants = global_constant;
    }
}


static void free_global_constants(Parser* parser) {
    while (parser->global_constants) {
        GlobalConstant* next = parser->global_constants->next;
        pyro_realloc(parser->vm, parser->global_constants, sizeof(GlobalConstant), 0);
        parser->global_constants = next;
    }
}


static void log_global_assignment(Parser* parser, Token name) {
    GlobalAssignment* global_asignment = pyro_realloc(parser->vm, NULL, 0, sizeof(GlobalAssignment));
    if (!global_asignment) {
        pyro_panic(parser->vm, "out of memory");
        return;
    }

    global_asignment->name = name;
    global_asignment->next = NULL;

    if (parser->global_assignments) {
        global_asignment->next = parser->global_assignments;
        parser->global_assignments = global_asignment;
    } else {
        parser->global_assignments = global_asignment;
    }
}


static void free_global_assignments(Parser* parser) {
    while (parser->global_assignments) {
        GlobalAssignment* next = parser->global_assignments->next;
        pyro_realloc(parser->vm, parser->global_assignments, sizeof(GlobalAssignment), 0);
        parser->global_assignments = next;
    }
}


static void check_global_constants(Parser* parser) {
    GlobalAssignment* global_assignment = parser->global_assignments;

    while (global_assignment) {
        GlobalConstant* global_constant = parser->global_constants;

        while (global_constant) {
            if (lexemes_are_equal(&global_assignment->name, &global_constant->name)) {
                pyro_panic_with_syntax_error(
                    parser->vm,
                    parser->src_id,
                    global_assignment->name.line,
                    "invalid assignment to global constant '%.*s'",
                    global_assignment->name.length,
                    global_assignment->name.start
                );
                return;
            }
            global_constant = global_constant->next;
        }

        global_assignment = global_assignment->next;
    }
}

/* -------------------- */
/*  Compiler Interface  */
/* -------------------- */


PyroFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, bool dump_bytecode) {
    Parser parser;
    parser.fn_compiler = NULL;
    parser.class_compiler = NULL;
    parser.src_id = src_id;
    parser.vm = vm;
    parser.num_statements = 0;
    parser.num_expression_statements = 0;
    parser.dump_bytecode = dump_bytecode;
    parser.global_constants = NULL;
    parser.global_assignments = NULL;

    // Strip any trailing whitespace before initializing the lexer. This is to ensure we
    // report the correct line number for syntax errors at the end of the input.
    while (src_len > 0 && pyro_is_ascii_ws(src_code[src_len - 1])) {
        src_len--;
    }
    pyro_init_lexer(&parser.lexer, vm, src_code, src_len, src_id);

    FnCompiler fn_compiler;
    if (!init_fn_compiler(&parser, &fn_compiler, TYPE_MODULE, make_syntoken_from_basename(src_id))) {
        return NULL;
    }

    // Prime the token pump by reading the first token from the lexer.
    advance(&parser);

    while (!match(&parser, TOKEN_EOF)) {
        if (vm->halt_flag) {
            break;
        }
        parse_statement(&parser);
    }

    PyroFn* fn = end_fn_compiler(&parser);

    // If we're running in a REPL and the code consisted of a single expression statement,
    // we may want to print the value of the expression.
    if (vm->in_repl && parser.num_statements == 1 && parser.num_expression_statements == 1) {
        if (fn->code_count >= 3 && fn->code[fn->code_count - 3] == PYRO_OPCODE_POP) {
            fn->code[fn->code_count - 3] = PYRO_OPCODE_POP_ECHO_IN_REPL;
        }
    }

    check_global_constants(&parser);
    free_global_constants(&parser);
    free_global_assignments(&parser);

    return fn;
}


PyroFn* pyro_compile_expression(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    Parser parser;
    parser.fn_compiler = NULL;
    parser.class_compiler = NULL;
    parser.src_id = src_id;
    parser.vm = vm;
    parser.num_statements = 0;
    parser.num_expression_statements = 0;
    parser.dump_bytecode = false;
    parser.global_constants = NULL;
    parser.global_assignments = NULL;

    // Strip any trailing whitespace before initializing the lexer. This is to ensure we
    // report the correct line number for syntax errors at the end of the input.
    while (src_len > 0 && pyro_is_ascii_ws(src_code[src_len - 1])) {
        src_len--;
    }
    pyro_init_lexer(&parser.lexer, vm, src_code, src_len, src_id);

    FnCompiler fn_compiler;
    if (!init_fn_compiler(&parser, &fn_compiler, TYPE_MODULE, make_syntoken_from_basename(src_id))) {
        return NULL;
    }

    // Prime the token pump by reading the first token from the lexer.
    advance(&parser);

    parse_expression(&parser, false);
    emit_byte(&parser, PYRO_OPCODE_RETURN);

    PyroFn* fn = end_fn_compiler(&parser);

    check_global_constants(&parser);
    free_global_constants(&parser);
    free_global_assignments(&parser);

    return fn;
}
