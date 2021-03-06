#include "compiler.h"
#include "debug.h"
#include "opcodes.h"
#include "heap.h"
#include "vm.h"
#include "utf8.h"
#include "utils.h"


/* ================================== */
/* ~~~~~~~~ Lexing Machinery ~~~~~~~~ */
/* ================================== */


typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_SEMICOLON,
    TOKEN_CARET,
    TOKEN_PERCENT,

    // One or two character tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_BANG_BANG,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_LESS_LESS,
    TOKEN_PLUS, TOKEN_PLUS_EQUAL,
    TOKEN_MINUS, TOKEN_MINUS_EQUAL,
    TOKEN_COLON, TOKEN_COLON_COLON,
    TOKEN_STAR, TOKEN_STAR_STAR,
    TOKEN_BAR, TOKEN_BAR_BAR,
    TOKEN_AMP, TOKEN_AMP_AMP,
    TOKEN_SLASH, TOKEN_SLASH_SLASH,
    TOKEN_HOOK, TOKEN_HOOK_HOOK,

    // Literals.
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_ESCAPED_STRING,
    TOKEN_INT,
    TOKEN_HEX_INT,
    TOKEN_OCTAL_INT,
    TOKEN_BINARY_INT,
    TOKEN_FLOAT,
    TOKEN_CHAR,

    // Keywords.
    TOKEN_AND, TOKEN_AS, TOKEN_ASSERT,
    TOKEN_BREAK,
    TOKEN_CLASS, TOKEN_CONTINUE,
    TOKEN_DEF,
    TOKEN_ECHO, TOKEN_ELSE,
    TOKEN_FALSE, TOKEN_FOR, TOKEN_FROM,
    TOKEN_IF, TOKEN_IMPORT, TOKEN_IN,
    TOKEN_LOOP,
    TOKEN_NOT, TOKEN_NULL,
    TOKEN_OR,
    TOKEN_RETURN,
    TOKEN_SELF, TOKEN_SUPER,
    TOKEN_TRUE, TOKEN_TRY,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_XOR,

    // Synthetic tokens.
    TOKEN_ERROR,
    TOKEN_EOF,
    TOKEN_SYN,
} TokenType;


typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    size_t line;
} Token;


typedef struct {
    const char* start;
    const char* current;
    const char* end;
    size_t line;
    const char* src_id;
    PyroVM* vm;
} Lexer;


static bool is_alpha(char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c == '_') return true;
    return false;
}


static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}


static bool is_digit_or_underscore(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}


static bool is_digit_or_plus_minus(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c == '+' || c == '-') return true;
    return false;
}


static bool is_alpha_or_digit(char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}


static bool is_whitespace(char c) {
    switch (c) {
        case '\n':
        case '\r':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}


static bool is_hex(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'A' && c <= 'F') return true;
    if (c >= 'a' && c <= 'f') return true;
    return false;
}


static bool is_octal(char c) {
    if (c >= '0' && c <= '7') return true;
    return false;
}


static bool is_binary(char c) {
    if (c == '0' || c == '1') return true;
    return false;
}


static bool is_at_end(Lexer* lexer) {
    return lexer->current == lexer->end;
}


static char next_char(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}


static char peek(Lexer* lexer) {
    if (lexer->current == lexer->end) return '\0';
    return *lexer->current;
}


static char peek_next(Lexer* lexer) {
    if (lexer->current == lexer->end) return '\0';
    if (lexer->current + 1 == lexer->end) return '\0';
    return lexer->current[1];
}


static bool match_char(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    return true;
}


static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    return token;
}


static Token make_error_token(Lexer* lexer) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = "error";
    token.length = 5;
    token.line = lexer->line;
    return token;
}


static void skip_whitespace(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                next_char(lexer);
                break;
            case '\n':
                lexer->line++;
                next_char(lexer);
                break;
            case '#':
                next_char(lexer);
                while (!is_at_end(lexer) && peek(lexer) != '\n') {
                    next_char(lexer);
                }
                break;
            default:
                return;
        }
    }
}


static bool check_keyword(Lexer* lexer, const char* keyword) {
    size_t length = strlen(keyword);
    if (lexer->current - lexer->start == (ptrdiff_t)length) {
        if (memcmp(lexer->start, keyword, length) == 0) {
            return true;
        }
    }
    return false;
}


static TokenType get_identifier_type(Lexer* lexer) {
    switch (lexer->start[0]) {
        case 'a':
            if (check_keyword(lexer, "and")) return TOKEN_AND;
            if (check_keyword(lexer, "as")) return TOKEN_AS;
            if (check_keyword(lexer, "assert")) return TOKEN_ASSERT;
            break;

        case 'b':
            if (check_keyword(lexer, "break")) return TOKEN_BREAK;
            break;

        case 'c':
            if (check_keyword(lexer, "class")) return TOKEN_CLASS;
            if (check_keyword(lexer, "continue")) return TOKEN_CONTINUE;
            break;

        case 'd':
            if (check_keyword(lexer, "def")) return TOKEN_DEF;
            break;

        case 'e':
            if (check_keyword(lexer, "echo")) return TOKEN_ECHO;
            if (check_keyword(lexer, "else")) return TOKEN_ELSE;
            break;

        case 'f':
            if (check_keyword(lexer, "false")) return TOKEN_FALSE;
            if (check_keyword(lexer, "for")) return TOKEN_FOR;
            if (check_keyword(lexer, "from")) return TOKEN_FROM;
            break;

        case 'i':
            if (check_keyword(lexer, "if")) return TOKEN_IF;
            if (check_keyword(lexer, "import")) return TOKEN_IMPORT;
            if (check_keyword(lexer, "in")) return TOKEN_IN;
            break;

        case 'l':
            if (check_keyword(lexer, "loop")) return TOKEN_LOOP;
            break;

        case 'n':
            if (check_keyword(lexer, "not")) return TOKEN_NOT;
            if (check_keyword(lexer, "null")) return TOKEN_NULL;
            break;

        case 'o':
            if (check_keyword(lexer, "or")) return TOKEN_OR;
            break;

        case 'r':
            if (check_keyword(lexer, "return")) return TOKEN_RETURN;
            break;

        case 's':
            if (check_keyword(lexer, "self")) return TOKEN_SELF;
            if (check_keyword(lexer, "super")) return TOKEN_SUPER;
            break;

        case 't':
            if (check_keyword(lexer, "true")) return TOKEN_TRUE;
            if (check_keyword(lexer, "try")) return TOKEN_TRY;
            break;

        case 'v':
            if (check_keyword(lexer, "var")) return TOKEN_VAR;
            break;

        case 'w':
            if (check_keyword(lexer, "while")) return TOKEN_WHILE;
            break;

        case 'x':
            if (check_keyword(lexer, "xor")) return TOKEN_XOR;
            break;
    }

    return TOKEN_IDENTIFIER;
}


static Token read_identifier(Lexer* lexer) {
    while (is_alpha_or_digit(peek(lexer))) {
        next_char(lexer);
    }
    return make_token(lexer, get_identifier_type(lexer));
}


static Token read_number(Lexer* lexer) {
    while (is_digit_or_underscore(peek(lexer))) {
        next_char(lexer);
    }

    if (is_at_end(lexer) || (peek(lexer) != '.' && peek(lexer) != 'e')) {
        return make_token(lexer, TOKEN_INT);
    }

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        next_char(lexer);
        while (is_digit_or_underscore(peek(lexer))) {
            next_char(lexer);
        }
    }

    if (peek(lexer) == 'e' && is_digit_or_plus_minus(peek_next(lexer))) {
        next_char(lexer);
        next_char(lexer);
        while (is_digit_or_underscore(peek(lexer))) {
            next_char(lexer);
        }
    }

    return make_token(lexer, TOKEN_FLOAT);
}


static Token read_raw_string(Lexer* lexer) {
    size_t start_line = lexer->line;

    while (!is_at_end(lexer) && peek(lexer) != '`') {
        if (peek(lexer) == '\n') {
            lexer->line++;
        }
        next_char(lexer);
    }

    if (is_at_end(lexer)) {
        pyro_err(lexer->vm,
            "%s:%zu\n  Syntax Error: Unterminated raw string literal, opened in line %zu.\n",
            lexer->src_id,
            start_line,
            start_line
        );
        return make_error_token(lexer);
    }

    next_char(lexer);
    return make_token(lexer, TOKEN_STRING);
}


static Token read_string(Lexer* lexer) {
    bool has_escapes = false;
    size_t start_line = lexer->line;

    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\n') {
            lexer->line++;
        }
        if (peek(lexer) == '\\') {
            has_escapes = true;
            next_char(lexer);
        }
        next_char(lexer);
    }

    if (is_at_end(lexer)) {
        pyro_err(lexer->vm,
            "%s:%zu\n  Syntax Error: Unterminated string literal, opened in line %zu.\n",
            lexer->src_id,
            start_line,
            start_line
        );
        return make_error_token(lexer);
    }

    next_char(lexer);
    return make_token(lexer, has_escapes ? TOKEN_ESCAPED_STRING: TOKEN_STRING);
}


static Token read_char(Lexer* lexer) {
    size_t start_line = lexer->line;

    while (!is_at_end(lexer) && peek(lexer) != '\'') {
        if (peek(lexer) == '\n') {
            lexer->line++;
        }
        if (peek(lexer) == '\\') {
            next_char(lexer);
        }
        next_char(lexer);
    }

    if (is_at_end(lexer)) {
        pyro_err(lexer->vm,
            "%s:%zu\n  Syntax Error: Unterminated character literal, opened in line %zu.\n",
            lexer->src_id,
            start_line,
            start_line
        );
        return make_error_token(lexer);
    }

    next_char(lexer);
    return make_token(lexer, TOKEN_CHAR);
}


Token next_token(Lexer* lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    char c = next_char(lexer);

    if (is_alpha(c) || c == '$') {
        return read_identifier(lexer);
    }

    if (c == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        next_char(lexer);
        while (is_hex(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_HEX_INT);
    }

    if (c == '0' && (peek(lexer) == 'o' || peek(lexer) == 'O')) {
        next_char(lexer);
        while (is_octal(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_OCTAL_INT);
    }

    if (c == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        next_char(lexer);
        while (is_binary(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_BINARY_INT);
    }

    if (is_digit(c)) {
        return read_number(lexer);
    }

    switch (c) {
        case '(': return make_token(lexer, TOKEN_LEFT_PAREN);
        case ')': return make_token(lexer, TOKEN_RIGHT_PAREN);
        case '{': return make_token(lexer, TOKEN_LEFT_BRACE);
        case '}': return make_token(lexer, TOKEN_RIGHT_BRACE);
        case '[': return make_token(lexer, TOKEN_LEFT_BRACKET);
        case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET);
        case ';': return make_token(lexer, TOKEN_SEMICOLON);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '.': return make_token(lexer, TOKEN_DOT);
        case '^': return make_token(lexer, TOKEN_CARET);
        case '%': return make_token(lexer, TOKEN_PERCENT);

        case '-': return make_token(lexer, match_char(lexer, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '+': return make_token(lexer, match_char(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '/': return make_token(lexer, match_char(lexer, '/') ? TOKEN_SLASH_SLASH : TOKEN_SLASH);
        case '=': return make_token(lexer, match_char(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case ':': return make_token(lexer, match_char(lexer, ':') ? TOKEN_COLON_COLON : TOKEN_COLON);
        case '*': return make_token(lexer, match_char(lexer, '*') ? TOKEN_STAR_STAR : TOKEN_STAR);
        case '|': return make_token(lexer, match_char(lexer, '|') ? TOKEN_BAR_BAR : TOKEN_BAR);
        case '?': return make_token(lexer, match_char(lexer, '?') ? TOKEN_HOOK_HOOK: TOKEN_HOOK);

        case '"': return read_string(lexer);
        case '`': return read_raw_string(lexer);
        case '\'': return read_char(lexer);

        case '<':
            if (match_char(lexer, '<')) return make_token(lexer, TOKEN_LESS_LESS);
            if (match_char(lexer, '=')) return make_token(lexer, TOKEN_LESS_EQUAL);
            return make_token(lexer, TOKEN_LESS);

        case '>':
            if (match_char(lexer, '>')) return make_token(lexer, TOKEN_GREATER_GREATER);
            if (match_char(lexer, '=')) return make_token(lexer, TOKEN_GREATER_EQUAL);
            return make_token(lexer, TOKEN_GREATER);

        case '!':
            if (match_char(lexer, '=')) return make_token(lexer, TOKEN_BANG_EQUAL);
            if (match_char(lexer, '!')) return make_token(lexer, TOKEN_BANG_BANG);
            return make_token(lexer, TOKEN_BANG);
    }

    if (c == '&' && match_char(lexer, '&')) {
        return make_token(lexer, TOKEN_AMP_AMP);
    }

    if (isprint(c)) {
        pyro_err(lexer->vm,
            "%s:%zu\n  Syntax Error: Unexpected character '%c' in input.\n",
            lexer->src_id,
            lexer->line,
            c
        );
    } else {
        pyro_err(lexer->vm,
            "%s:%zu\n  Syntax Error: Unexpected byte value (0x%02X) in input.\n",
            lexer->src_id,
            lexer->line,
            c
        );
    }

    return make_error_token(lexer);
}


void init_lexer(Lexer* lexer, PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    lexer->start = src_code;
    lexer->current = src_code;
    lexer->end = src_code + src_len;
    lexer->line = 1;
    lexer->vm = vm;
    lexer->src_id = src_id;
}


/* =================================== */
/* ~~~~~~~~ Parsing Machinery ~~~~~~~~ */
/* =================================== */


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


struct Parser {
    Lexer lexer;
    Token previous;
    Token current;
    bool had_error;
    PyroVM* vm;
    FnCompiler* compiler;
    ClassCompiler* class_compiler;
    ObjModule* module;
    const char* src_id;
};


// -------------------- //
// Forward Declarations //
// -------------------- //


static void parse_expression(Parser* parser, bool can_assign, bool can_assign_in_parens);
static void parse_statement(Parser* parser);
static void parse_function_definition(Parser* parser, FnType type, Token name);
static void parse_unary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens);


// ----------------- //
// Parsing Utilities //
// ----------------- //


static void err_at_token(Parser* parser, Token* token, const char* message) {
    if (parser->had_error) {
        return;
    }
    parser->had_error = true;

    pyro_err(parser->vm, "%s:%zu\n  Syntax Error", parser->src_id, token->line);

    if (token->type == TOKEN_EOF || token->type == TOKEN_ERROR) {
        pyro_err(parser->vm, ": %s\n", message);
    } else if (token->type == TOKEN_STRING || token->type == TOKEN_ESCAPED_STRING) {
        pyro_err(parser->vm, ": %s\n", message);
    } else if (token->type == TOKEN_CHAR) {
        pyro_err(parser->vm, " at %.*s: %s\n", token->length, token->start, message);
    } else {
        pyro_err(parser->vm, " at '%.*s': %s\n", token->length, token->start, message);
    }
}


static void err_at_curr(Parser* parser, const char* message) {
    err_at_token(parser, &parser->current, message);
}


static void err_at_prev(Parser* parser, const char* message) {
    err_at_token(parser, &parser->previous, message);
}


// Make a synthetic token to pass a hardcoded string around.
static Token syntoken(const char* text) {
    return (Token) {
        .type = TOKEN_SYN,
        .line = 0,
        .start = text,
        .length = strlen(text)
    };
}


static bool lexemes_are_equal(Token* a, Token* b) {
    if (a->length == b->length) {
        return memcmp(a->start, b->start, a->length) == 0;
    }
    return false;
}


static void emit_byte(Parser* parser, uint8_t byte) {
    ObjFn_write(parser->compiler->fn, byte, parser->previous.line, parser->vm);
}


static void emit_bytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emit_byte(parser, byte1);
    emit_byte(parser, byte2);
}


// Emit a 16-bit unsigned integer in big-endian format.
static void emit_u16(Parser* parser, uint16_t value) {
    emit_byte(parser, (value >> 8) & 0xff);
    emit_byte(parser, value & 0xff);
}


// Emit an opcode followed by a 16-bit argument.
static void emit_op_u16(Parser* parser, uint8_t opcode, uint16_t arg) {
    emit_byte(parser, opcode);
    emit_u16(parser, arg);
}


static void emit_return(Parser* parser) {
    if (parser->compiler->type == TYPE_INIT_METHOD) {
        emit_bytes(parser, OP_GET_LOCAL, 0);
    } else {
        emit_byte(parser, OP_LOAD_NULL);
    }
    emit_byte(parser, OP_RETURN);
}


static uint16_t make_constant(Parser* parser, Value value) {
    size_t index = ObjFn_add_constant(parser->compiler->fn, value, parser->vm);
    if (index > UINT16_MAX) {
        err_at_prev(parser, "Too many constants in function.");
        return 0;
    }
    return (uint16_t)index;
}


// Takes an identifier token and adds its lexeme to the constant table as a string.
static uint16_t make_string_constant_from_identifier(Parser* parser, Token* name) {
    ObjStr* string = ObjStr_copy_raw(name->start, name->length, parser->vm);
    return make_constant(parser, OBJ_VAL(string));
}


static void emit_constant(Parser* parser, Value value) {
    uint16_t index = make_constant(parser, value);
    emit_byte(parser, OP_LOAD_CONSTANT);
    emit_u16(parser, index);
}


// Reads the next token from the lexer.
static void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = next_token(&parser->lexer);
    if (parser->current.type == TOKEN_ERROR) {
        parser->had_error = true;
    }
}


// Reads the next token from the lexer and validates that it has the expected type.
static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    err_at_curr(parser, message);
}


static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}


static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}


static bool match2(Parser* parser, TokenType type1, TokenType type2) {
    if (check(parser, type1) || check(parser, type2)) {
        advance(parser);
        return true;
    }
    return false;
}


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


static void init_fn_compiler(Parser* parser, FnCompiler* compiler, FnType type, Token name) {
    compiler->enclosing = parser->compiler;
    parser->compiler = compiler;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->loop_compiler = NULL;

    // Assign to NULL first as the function initializer can trigger the GC.
    compiler->fn = NULL;
    compiler->fn = ObjFn_new(parser->vm);
    compiler->fn->module = parser->module;
    compiler->fn->name = ObjStr_copy_raw(name.start, name.length, parser->vm);
    compiler->fn->source = ObjStr_copy_raw(parser->src_id, strlen(parser->src_id), parser->vm);

    // Reserve slot zero for the receiver when calling methods.
    Local* local = &compiler->locals[compiler->local_count++];
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
}


static ObjFn* end_fn_compiler(Parser* parser) {
    emit_return(parser);

    ObjFn* fn = parser->compiler->fn;

    #ifdef PYRO_DEBUG_DUMP_BYTECODE
        if (!parser->had_error) {
            pyro_disassemble_function(parser->vm, fn);
        }
    #endif

    parser->compiler = parser->compiler->enclosing;
    return fn;
}


static int resolve_local(Parser* parser, FnCompiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (lexemes_are_equal(name, &local->name)) {
            if (local->is_initialized) {
                return i;
            }
            err_at_prev(parser, "Can't read a local variable in its own initializer.");
        }
    }
    return -1;
}


// [index] is the closed-over local variable's slot index.
// Returns the index of the newly created upvalue in the function's upvalue list.
static int add_upvalue(Parser* parser, FnCompiler* compiler, uint8_t index, bool is_local) {
    size_t upvalue_count = compiler->fn->upvalue_count;

    // Dont add duplicate upvalues unnecessarily if a closure references the same variable in a
    // surrounding function multiple times.
    for (size_t i = 0; i < upvalue_count; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count == 256) {
        err_at_prev(parser, "Too many closure variables in function (max: 256).");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->fn->upvalue_count++;
}


// Looks for a local variable declared in any of the surrounding functions.
// If it finds one, it returns the 'upvalue index' for that variable.
// A return value of -1 means the variable is either global or undefined.
static int resolve_upvalue(Parser* parser, FnCompiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) {
        return -1;
    }

    // Look for a matching local variable in the directly enclosing function.
    // If we find one, capture it and return the upvalue index.
    int local = resolve_local(parser, compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(parser, compiler, (uint8_t)local, true);
    }

    // Look for a matching local variable beyond the immediately enclosing function by
    // recursively walking the chain of nested compilers. If we find one, the most deeply
    // nested call will capture it and return its upvalue index. As the recursive calls
    // unwind we'll construct a chain of upvalues leading to the captured variable.
    int upvalue = resolve_upvalue(parser, compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(parser, compiler, (uint8_t)upvalue, false);
    }

    // No matching local variable in any enclosing scope.
    return -1;
}


static void add_local(Parser* parser, Token name) {
    if (parser->compiler->local_count == 256) {
        err_at_prev(parser, "Too many local variables in scope (max 256).");
        return;
    }
    Local* local = &parser->compiler->locals[parser->compiler->local_count++];
    local->name = name;
    local->depth = parser->compiler->scope_depth;
    local->is_initialized = false; // the variable has been declared but not yet defined
    local->is_captured = false;
}


static void mark_initialized(Parser* parser) {
    if (parser->compiler->scope_depth == 0) {
        return;
    }
    parser->compiler->locals[parser->compiler->local_count - 1].is_initialized = true;
}


static void define_variable(Parser* parser, uint16_t index) {
    if (parser->compiler->scope_depth > 0) {
        mark_initialized(parser);
        return;
    }
    emit_byte(parser, OP_DEFINE_GLOBAL);
    emit_u16(parser, index);
}


// Add a newly declared local variable to the compiler's locals list.
static void declare_variable(Parser* parser, Token name) {
    if (parser->compiler->scope_depth == 0) {
        // Don't do anything for globals.
        return;
    }

    for (int i = parser->compiler->local_count - 1; i >= 0; i--) {
        Local* local = &parser->compiler->locals[i];
        if (local->depth != -1 && local->depth < parser->compiler->scope_depth) {
            break;
        }
        if (lexemes_are_equal(&name, &local->name)) {
            err_at_prev(parser, "A variable with this name already exists in this scope.");
        }
    }

    add_local(parser, name);
}


// Called when declaring a variable or function parameter.
static uint16_t consume_variable_name(Parser* parser, const char* error_message) {
    consume(parser, TOKEN_IDENTIFIER, error_message);
    declare_variable(parser, parser->previous);

    // Local variables are referenced by index not by name so we don't need to add the name
    // of a local to the list of constants. This return value will simply be ignored.
    if (parser->compiler->scope_depth > 0) {
        return 0;
    }

    // Global variables are referenced by name.
    return make_string_constant_from_identifier(parser, &parser->previous);
}


// Emits bytecode to discard all local variables at scope depth greater than or equal to [depth].
// Returns the number of locals discarded. (This function doesn't decremement the local count as
// it's called directly by break statements.)
static int discard_locals(Parser* parser, int depth) {
    int local_count = parser->compiler->local_count;

    while (local_count > 0 && parser->compiler->locals[local_count - 1].depth >= depth) {
        if (parser->compiler->locals[local_count - 1].is_captured) {
            emit_byte(parser, OP_CLOSE_UPVALUE);
        } else {
            emit_byte(parser, OP_POP);
        }
        local_count--;
    }

    return parser->compiler->local_count - local_count;
}


static void begin_scope(Parser* parser) {
    parser->compiler->scope_depth++;
}


static void end_scope(Parser* parser) {
    int popped = discard_locals(parser, parser->compiler->scope_depth);
    parser->compiler->local_count -= popped;
    parser->compiler->scope_depth--;
}


// Emits a jump instruction and a two-byte placeholder operand. Returns the index of the
// first byte of the operand.
static size_t emit_jump(Parser* parser, OpCode instruction) {
    emit_byte(parser, instruction);
    emit_byte(parser, 0xff);
    emit_byte(parser, 0xff);
    return parser->compiler->fn->code_count - 2;
}


// We call patch_jump() right before emitting the instruction we want the jump to land on.
static void patch_jump(Parser* parser, size_t index) {
    size_t jump = parser->compiler->fn->code_count - index - 2;
    if (jump > UINT16_MAX) {
        err_at_prev(parser, "Too much code to jump over.");
    }
    parser->compiler->fn->code[index] = (jump >> 8) & 0xff;
    parser->compiler->fn->code[index + 1] = jump & 0xff;
}


// ------------------ //
// Expression Parsers //
// ------------------ //


static uint8_t parse_argument_list(Parser* parser) {
    uint8_t arg_count = 0;
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            parse_expression(parser, false, true);
            if (arg_count == 255) {
                err_at_prev(parser, "Too many arguments (max: 255).");
            }
            arg_count++;
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
    return arg_count;
}


// Emits bytecode to load the value of the named variable onto the stack.
static void get_named_variable(Parser* parser, Token name) {
    int local_index = resolve_local(parser, parser->compiler, &name);
    if (local_index != -1) {
        emit_bytes(parser, OP_GET_LOCAL, (uint8_t)local_index);
        return;
    }

    int upvalue_index = resolve_upvalue(parser, parser->compiler, &name);
    if (upvalue_index != -1) {
        emit_bytes(parser, OP_GET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, OP_GET_GLOBAL);
    emit_u16(parser, const_index);
}


// Emits bytecode to set the named variable to the value on top of the stack.
static void set_named_variable(Parser* parser, Token name) {
    int local_index = resolve_local(parser, parser->compiler, &name);
    if (local_index != -1) {
        emit_bytes(parser, OP_SET_LOCAL, (uint8_t)local_index);
        return;
    }

    int upvalue_index = resolve_upvalue(parser, parser->compiler, &name);
    if (upvalue_index != -1) {
        emit_bytes(parser, OP_SET_UPVALUE, (uint8_t)upvalue_index);
        return;
    }

    uint16_t const_index = make_string_constant_from_identifier(parser, &name);
    emit_byte(parser, OP_SET_GLOBAL);
    emit_u16(parser, const_index);
}


static void parse_variable(Parser* parser, bool can_assign) {
    Token name = parser->previous;

    if (can_assign && match(parser, TOKEN_EQUAL)) {
        parse_expression(parser, true, true);
        set_named_variable(parser, name);

    } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
        get_named_variable(parser, name);
        parse_expression(parser, true, true);
        emit_byte(parser, OP_ADD);
        set_named_variable(parser, name);

    } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
        get_named_variable(parser, name);
        parse_expression(parser, true, true);
        emit_byte(parser, OP_SUBTRACT);
        set_named_variable(parser, name);

    } else {
        get_named_variable(parser, name);
    }
}


static int64_t parse_hex_literal(Parser* parser) {
    char buffer[16 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous.length; i++) {
        if (parser->previous.start[i] == '_') {
            continue;
        }
        if (count == 16) {
            err_at_prev(parser, "Too many digits in hex literal (max: 16).");
            return 0;
        }
        buffer[count++] = parser->previous.start[i];
    }

    if (count == 0) {
        err_at_prev(parser, "Invalid hex literal (zero digits).");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 16);
    if (errno != 0) {
        err_at_prev(parser, "Invalid hex literal (out of range).");
    }

    return value;
}


static int64_t parse_binary_literal(Parser* parser) {
    char buffer[64 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous.length; i++) {
        if (parser->previous.start[i] == '_') {
            continue;
        }
        if (count == 64) {
            err_at_prev(parser, "Too many digits in binary literal (max: 64).");
            return 0;
        }
        buffer[count++] = parser->previous.start[i];
    }

    if (count == 0) {
        err_at_prev(parser, "Invalid binary literal (zero digits).");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 2);
    if (errno != 0) {
        err_at_prev(parser, "Invalid binary literal (out of range).");
    }

    return value;
}


static int64_t parse_octal_literal(Parser* parser) {
    char buffer[22 + 1];
    size_t count = 0;

    for (size_t i = 2; i < parser->previous.length; i++) {
        if (parser->previous.start[i] == '_') {
            continue;
        }
        if (count == 22) {
            err_at_prev(parser, "Too many digits in octal literal (max: 22).");
            return 0;
        }
        buffer[count++] = parser->previous.start[i];
    }

    if (count == 0) {
        err_at_prev(parser, "Invalid octal literal (zero digits).");
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 8);
    if (errno != 0) {
        err_at_prev(parser, "Invalid octal literal (out of range).");
    }

    return value;
}


static int64_t parse_int_literal(Parser* parser) {
    char buffer[20 + 1];
    size_t count = 0;

    for (size_t i = 0; i < parser->previous.length; i++) {
        if (parser->previous.start[i] == '_') {
            continue;
        }
        if (count == 20) {
            err_at_prev(parser, "Too many digits in integer literal (max: 20).");
            return 0;
        }
        buffer[count++] = parser->previous.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    int64_t value = strtoll(buffer, NULL, 10);
    if (errno != 0) {
        err_at_prev(parser, "Invalid integer literal (out of range).");
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

    for (size_t i = 0; i < parser->previous.length; i++) {
        if (parser->previous.start[i] == '_') {
            continue;
        }
        if (count == 24) {
            err_at_prev(parser, "Too many digits in floating-point literal (max: 24).");
            return 0;
        }
        buffer[count++] = parser->previous.start[i];
    }

    buffer[count] = '\0';
    errno = 0;
    double value = strtod(buffer, NULL);
    if (errno != 0) {
        err_at_prev(parser, "Invalid floating-point literal (out of range).");
    }

    return value;
}


static uint32_t parse_char_literal(Parser* parser) {
    const char* start = parser->previous.start + 1;
    size_t length = parser->previous.length - 2;

    // The longest valid character literal is a unicode escape sequence of the form: '\UXXXXXXXX'.
    if (length == 0 || length > 10) {
        err_at_prev(parser, "Invalid character literal.");
        return 0;
    }

    char buffer[10];
    size_t count = pyro_unescape_string(start, length, buffer);

    Utf8CodePoint cp;
    if (!pyro_read_utf8_codepoint((uint8_t*)buffer, count, &cp)) {
        err_at_prev(parser, "Invalid character literal (not utf-8).");
        return 0;
    }

    if (cp.length != count) {
        err_at_prev(parser, "Invalid character literal (too many bytes).");
        return 0;
    }

    return cp.codepoint;
}


static void parse_map_literal(Parser* parser) {
    uint16_t count = 0;
    do {
        if (check(parser, TOKEN_RIGHT_BRACE)) {
            break;
        }
        parse_expression(parser, false, true);
        consume(parser, TOKEN_EQUAL, "Expected '=' after key.");
        parse_expression(parser, false, true);
        count++;
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after map literal.");
    emit_byte(parser, OP_MAKE_MAP);
    emit_u16(parser, count);
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
    consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after vector literal.");
    emit_byte(parser, OP_MAKE_VEC);
    emit_u16(parser, count);
}


static void parse_primary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
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
        emit_constant(parser, I64_VAL(value));
    }

    else if (match(parser, TOKEN_HEX_INT)) {
        int64_t value = parse_hex_literal(parser);
        emit_constant(parser, I64_VAL(value));
    }

    else if (match(parser, TOKEN_BINARY_INT)) {
        int64_t value = parse_binary_literal(parser);
        emit_constant(parser, I64_VAL(value));
    }

    else if (match(parser, TOKEN_OCTAL_INT)) {
        int64_t value = parse_octal_literal(parser);
        emit_constant(parser, I64_VAL(value));
    }

    else if (match(parser, TOKEN_FLOAT)) {
        double value = parse_float_literal(parser);
        emit_constant(parser, F64_VAL(value));
    }

    else if (match(parser, TOKEN_LEFT_PAREN)) {
        parse_expression(parser, can_assign_in_parens, can_assign_in_parens);
        consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
    }

    else if (match(parser, TOKEN_STRING)) {
        const char* start = parser->previous.start + 1;
        size_t length = parser->previous.length - 2;
        ObjStr* string = ObjStr_copy_raw(start, length, parser->vm);
        emit_constant(parser, OBJ_VAL(string));
    }

    else if (match(parser, TOKEN_ESCAPED_STRING)) {
        const char* start = parser->previous.start + 1;
        size_t length = parser->previous.length - 2;
        ObjStr* string = ObjStr_copy_esc(start, length, parser->vm);
        emit_constant(parser, OBJ_VAL(string));
    }

    else if (match(parser, TOKEN_CHAR)) {
        uint32_t codepoint = parse_char_literal(parser);
        emit_constant(parser, CHAR_VAL(codepoint));
    }

    else if (match(parser, TOKEN_IDENTIFIER)) {
        parse_variable(parser, can_assign);
    }

    else if (match(parser, TOKEN_SELF)) {
        if (parser->class_compiler == NULL) {
            err_at_prev(parser, "Invalid use of 'self' outside a method declaration.");
        }
        get_named_variable(parser, parser->previous);
    }

    else if (match(parser, TOKEN_SUPER)) {
        if (parser->class_compiler == NULL) {
            err_at_prev(parser, "Invalid use of 'super' outside a class.");
            return;
        } else if (!parser->class_compiler->has_superclass) {
            err_at_prev(parser, "Invalid use of 'super' in a class with no superclass.");
        }

        consume(parser, TOKEN_COLON, "Expected ':' after 'super'.");
        consume(parser, TOKEN_IDENTIFIER, "Expected superclass method name.");

        uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
        get_named_variable(parser, syntoken("self"));    // load the instance

        if (match(parser, TOKEN_LEFT_PAREN)) {
            uint8_t arg_count = parse_argument_list(parser);
            get_named_variable(parser, syntoken("super"));   // load the superclass
            emit_byte(parser, OP_INVOKE_SUPER_METHOD);
            emit_u16(parser, index);
            emit_byte(parser, arg_count);
        } else {
            get_named_variable(parser, syntoken("super"));   // load the superclass
            emit_byte(parser, OP_GET_SUPER_METHOD);
            emit_u16(parser, index);
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
        err_at_curr(parser, "Invalid token. Expected an expression.");
    }
}


static void parse_call_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_primary_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_LEFT_PAREN)) {
            uint8_t arg_count = parse_argument_list(parser);
            emit_bytes(parser, OP_CALL, arg_count);
        }

        else if (match(parser, TOKEN_LEFT_BRACKET)) {
            parse_expression(parser, true, true);
            consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after index.");
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true, true);
                emit_byte(parser, OP_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, OP_DUP2);
                emit_byte(parser, OP_GET_INDEX);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_ADD);
                emit_byte(parser, OP_SET_INDEX);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, OP_DUP2);
                emit_byte(parser, OP_GET_INDEX);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_SUBTRACT);
                emit_byte(parser, OP_SET_INDEX);
            } else {
                emit_byte(parser, OP_GET_INDEX);
            }
        }

        else if (match(parser, TOKEN_DOT)) {
            consume(parser, TOKEN_IDENTIFIER, "Expected a field name after '.'.");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
            if (can_assign && match(parser, TOKEN_EQUAL)) {
                parse_expression(parser, true, true);
                emit_op_u16(parser, OP_SET_FIELD, index);
            } else if (can_assign && match(parser, TOKEN_PLUS_EQUAL)) {
                emit_byte(parser, OP_DUP);
                emit_op_u16(parser, OP_GET_FIELD, index);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_ADD);
                emit_op_u16(parser, OP_SET_FIELD, index);
            } else if (can_assign && match(parser, TOKEN_MINUS_EQUAL)) {
                emit_byte(parser, OP_DUP);
                emit_op_u16(parser, OP_GET_FIELD, index);
                parse_expression(parser, true, true);
                emit_byte(parser, OP_SUBTRACT);
                emit_op_u16(parser, OP_SET_FIELD, index);
            } else {
                emit_op_u16(parser, OP_GET_FIELD, index);
            }
        }

        else if (match(parser, TOKEN_COLON)) {
            consume(parser, TOKEN_IDENTIFIER, "Expected a method name after ':'.");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
            if (match(parser, TOKEN_LEFT_PAREN)) {
                uint8_t arg_count = parse_argument_list(parser);
                emit_op_u16(parser, OP_INVOKE_METHOD, index);
                emit_byte(parser, arg_count);
            } else {
                emit_op_u16(parser, OP_GET_METHOD, index);
            }
        }

        else if (match(parser, TOKEN_COLON_COLON)) {
            consume(parser, TOKEN_IDENTIFIER, "Expected a module name after '::'.");
            uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
            emit_op_u16(parser, OP_GET_MEMBER, index);
        }

        else {
            break;
        }
    }
}


static void parse_try_expr(Parser* parser) {
    FnCompiler compiler;
    init_fn_compiler(parser, &compiler, TYPE_TRY_EXPR, syntoken("try"));
    begin_scope(parser);

    parse_unary_expr(parser, false, false);
    emit_byte(parser, OP_RETURN);

    ObjFn* fn = end_fn_compiler(parser);
    emit_op_u16(parser, OP_CLOSURE, make_constant(parser, OBJ_VAL(fn)));

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, compiler.upvalues[i].index);
    }
}


static void parse_power_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_call_expr(parser, can_assign, can_assign_in_parens);
    if (match(parser, TOKEN_CARET)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_POWER);
    }
}


static void parse_unary_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    if (match(parser, TOKEN_MINUS)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_NEGATE);
    } else if (match(parser, TOKEN_BANG)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_LOGICAL_NOT);
    } else if (match(parser, TOKEN_TRY)) {
        parse_try_expr(parser);
        emit_byte(parser, OP_TRY);
    } else if (match(parser, TOKEN_NOT)) {
        parse_unary_expr(parser, false, can_assign_in_parens);
        emit_byte(parser, OP_BITWISE_NOT);
    } else {
        parse_power_expr(parser, can_assign, can_assign_in_parens);
    }
}


static void parse_bitwise_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_unary_expr(parser, can_assign, can_assign_in_parens);
    while (true) {
        if (match(parser, TOKEN_XOR)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BITWISE_XOR);
        } else if (match(parser, TOKEN_AND)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BITWISE_AND);
        } else if (match(parser, TOKEN_OR)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_BITWISE_OR);
        } else if (match(parser, TOKEN_LESS_LESS)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_LSHIFT);
        } else if (match(parser, TOKEN_GREATER_GREATER)) {
            parse_unary_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_RSHIFT);
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
            emit_byte(parser, OP_MULTIPLY);
        } else if (match(parser, TOKEN_SLASH)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_FLOAT_DIV);
        } else if (match(parser, TOKEN_SLASH_SLASH)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_TRUNC_DIV);
        } else if (match(parser, TOKEN_PERCENT)) {
            parse_bitwise_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_MODULO);
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
            emit_byte(parser, OP_ADD);
        } else if (match(parser, TOKEN_MINUS)) {
            parse_multiplicative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_SUBTRACT);
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
            emit_byte(parser, OP_GREATER);
        } else if (match(parser, TOKEN_GREATER_EQUAL)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_GREATER_EQUAL);
        } else if (match(parser, TOKEN_LESS)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_LESS);
        } else if (match(parser, TOKEN_LESS_EQUAL)) {
            parse_additive_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_LESS_EQUAL);
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
            emit_byte(parser, OP_EQUAL);
        } else if (match(parser, TOKEN_BANG_EQUAL)) {
            parse_comparative_expr(parser, false, can_assign_in_parens);
            emit_byte(parser, OP_NOT_EQUAL);
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
        consume(parser, TOKEN_BAR, "Expected '|' after condition.");
        patch_jump(parser, jump_to_false_branch);
        emit_byte(parser, OP_POP);
        parse_logical_expr(parser, false, can_assign_in_parens);
        patch_jump(parser, jump_to_end);
    }
}


static void parse_assignment_expr(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_conditional_expr(parser, can_assign, can_assign_in_parens);

    if (can_assign && match_assignment_token(parser)) {
        err_at_prev(parser, "Invalid assignment target.");
    }
}


static void parse_expression(Parser* parser, bool can_assign, bool can_assign_in_parens) {
    parse_assignment_expr(parser, can_assign, can_assign_in_parens);
}


// ----------------- //
// Statement Parsers //
// ----------------- //


static void parse_echo_stmt(Parser* parser) {
    parse_expression(parser, true, true);
    int count = 1;
    while (match(parser, TOKEN_COMMA)) {
        parse_expression(parser, true, true);
        count++;
    }
    if (count > 255) {
        err_at_prev(parser, "Too many arguments for 'echo' (max: 255).");
    }
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.");
    emit_bytes(parser, OP_ECHO, count);
}


static void parse_assert_stmt(Parser* parser) {
    parse_expression(parser, true, true);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.");
    emit_byte(parser, OP_ASSERT);
}


static void parse_expression_stmt(Parser* parser) {
    parse_expression(parser, true, true);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.");
    emit_byte(parser, OP_POP);
}


static void parse_var_declaration(Parser* parser) {
    do {
        uint8_t index = consume_variable_name(parser, "Expected variable name.");
        if (match(parser, TOKEN_EQUAL)) {
            parse_expression(parser, true, true);
        } else {
            emit_byte(parser, OP_LOAD_NULL);
        }
        define_variable(parser, index);
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after variable declaration.");
}


static void parse_import_stmt(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expected module name after 'import'.");
    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
    emit_op_u16(parser, OP_LOAD_CONSTANT, index);
    Token import_name = parser->previous;
    int count = 1;

    while (match(parser, TOKEN_COLON_COLON)) {
        consume(parser, TOKEN_IDENTIFIER, "Expected module name after '::'.");
        index = make_string_constant_from_identifier(parser, &parser->previous);
        emit_op_u16(parser, OP_LOAD_CONSTANT, index);
        import_name = parser->previous;
        count++;
    }

    if (count > 255) {
        err_at_prev(parser, "Too many module names in import statement.");
    }
    emit_bytes(parser, OP_IMPORT, count);

    if (match(parser, TOKEN_AS)) {
        consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'as'.");
        index = make_string_constant_from_identifier(parser, &parser->previous);
        import_name = parser->previous;
    }

    consume(parser, TOKEN_SEMICOLON, "Expected ';' after import statement.");
    declare_variable(parser, import_name);
    define_variable(parser, index);
}


static void parse_block(Parser* parser) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        parse_statement(parser);
    }
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}


static void parse_if_stmt(Parser* parser) {
    // Parse the condition.
    parse_expression(parser, false, true);
    if (match_assignment_token(parser)) {
        err_at_prev(
            parser,
            "Assignment is disabled inside 'if' conditions. "
            "(Wrap the assignment in parentheses to enable it.)"
        );
        return;
    }

    // Jump over the 'then' block if the condition is false.
    size_t jump_over_then = emit_jump(parser, OP_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the 'then' block.
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' after condition.");
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
            consume(parser, TOKEN_LEFT_BRACE, "Expected '{' after else.");
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
    emit_byte(parser, OP_LOOP);

    size_t offset = parser->compiler->fn->code_count - start_index + 2;
    if (offset > UINT16_MAX) {
        err_at_prev(parser, "Loop body is too large.");
    }

    emit_byte(parser, (offset >> 8) & 0xff);
    emit_byte(parser, offset & 0xff);
}


static void parse_for_in_stmt(Parser* parser) {
    // Push a new scope to wrap a dummy local variable pointing to the iterator object.
    begin_scope(parser);

    consume(parser, TOKEN_IDENTIFIER, "Expected loop variable name.");
    Token loop_var_name = parser->previous;
    consume(parser, TOKEN_IN, "Expected keyword 'in' after loop variable.");

    // This is the object we'll be iterating over.
    parse_expression(parser, true, true);
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before the loop body.");

    // Replace the object on top of the stack with the result of calling :$iter() on it.
    emit_byte(parser, OP_ITER);
    add_local(parser, syntoken("*iterator*"));

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_index = parser->compiler->fn->code_count;
    loop.start_depth = parser->compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->compiler->loop_compiler;
    parser->compiler->loop_compiler = &loop;

    emit_byte(parser, OP_ITER_NEXT);
    size_t exit_jump_index = emit_jump(parser, OP_JUMP_IF_ERR);

    begin_scope(parser);
    add_local(parser, loop_var_name);
    mark_initialized(parser);
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
        ObjFn* fn = parser->compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->compiler->loop_compiler = loop.enclosing;

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
        parse_var_declaration(parser);
    } else {
        parse_expression_stmt(parser);
    }

    // This is the point in the bytecode the loop will jump back to.
    LoopCompiler loop;
    loop.start_index = parser->compiler->fn->code_count;
    loop.start_depth = parser->compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->compiler->loop_compiler;
    parser->compiler->loop_compiler = &loop;

    // Parse the (optional) condition.
    bool loop_has_condition = false;
    size_t exit_jump_index = 0;
    if (!match(parser, TOKEN_SEMICOLON)) {
        loop_has_condition = true;
        parse_expression(parser, false, true);
        consume(parser, TOKEN_SEMICOLON, "Expected ';' after loop condition.");
        exit_jump_index = emit_jump(parser, OP_POP_JUMP_IF_FALSE);
    }

    // Parse the (optional) increment clause.
    if (!match(parser, TOKEN_LEFT_BRACE)) {
        size_t body_jump_index = emit_jump(parser, OP_JUMP);

        size_t increment_index = parser->compiler->fn->code_count;
        parse_expression(parser, true, true);
        emit_byte(parser, OP_POP);
        consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before loop body.");

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
        ObjFn* fn = parser->compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->compiler->loop_compiler = loop.enclosing;

    // Pop the scope surrounding the loop variables.
    end_scope(parser);
}


static void parse_infinite_loop_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_index = parser->compiler->fn->code_count;
    loop.start_depth = parser->compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->compiler->loop_compiler;
    parser->compiler->loop_compiler = &loop;

    // Emit the bytecode for the block.
    begin_scope(parser);
    parse_block(parser);
    end_scope(parser);

    // Jump back to the beginning of the loop.
    emit_loop(parser, loop.start_index);

    // If we found any break statements in the loop, backpatch their destinations.
    if (loop.had_break) {
        size_t ip = loop.start_index;
        ObjFn* fn = parser->compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->compiler->loop_compiler = loop.enclosing;
}


static void parse_while_stmt(Parser* parser) {
    LoopCompiler loop;
    loop.start_index = parser->compiler->fn->code_count;
    loop.start_depth = parser->compiler->scope_depth;
    loop.had_break = false;
    loop.enclosing = parser->compiler->loop_compiler;
    parser->compiler->loop_compiler = &loop;

    // Parse the condition.
    parse_expression(parser, false, true);
    size_t exit_jump_index = emit_jump(parser, OP_POP_JUMP_IF_FALSE);

    // Emit the bytecode for the block.
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before loop body.");
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
        ObjFn* fn = parser->compiler->fn;
        while (ip < fn->code_count) {
            if (fn->code[ip] == OP_BREAK) {
                fn->code[ip] = OP_JUMP;
                patch_jump(parser, ip + 1);
            } else {
                ip += 1 + ObjFn_opcode_argcount(fn, ip);
            }
        }
    }

    parser->compiler->loop_compiler = loop.enclosing;
}


// This helper parses a function definition, i.e. the bit that looks like (...){...}.
// It emits the bytecode to create a ObjClosure and leave it on top of the stack.
static void parse_function_definition(Parser* parser, FnType type, Token name) {
    FnCompiler compiler;
    init_fn_compiler(parser, &compiler, type, name);
    begin_scope(parser);

    // Compile the parameter list.
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' before function parameters.");
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            if (compiler.fn->arity == 255) {
                err_at_curr(parser, "Too many parameters (max: 255).");
            }
            compiler.fn->arity++;
            uint16_t index = consume_variable_name(parser, "Expected parameter name.");
            define_variable(parser, index);
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after function parameters.");

    if (type == TYPE_METHOD) {
        if (strcmp(compiler.fn->name->bytes, "$fmt") == 0 && compiler.fn->arity != 1) {
            err_at_prev(parser, "Invalid method definition, $fmt() takes 1 argument.");
            return;
        }
        if (strcmp(compiler.fn->name->bytes, "$str") == 0 && compiler.fn->arity != 0) {
            err_at_prev(parser, "Invalid method definition, $str() takes no arguments.");
            return;
        }
    }

    // Compile the body.
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before function body.");
    parse_block(parser);

    // Create the function object.
    ObjFn* fn = end_fn_compiler(parser);
    emit_op_u16(parser, OP_CLOSURE, make_constant(parser, OBJ_VAL(fn)));

    for (size_t i = 0; i < fn->upvalue_count; i++) {
        emit_byte(parser, compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(parser, compiler.upvalues[i].index);
    }
}


static void parse_function_declaration(Parser* parser) {
    uint16_t index = consume_variable_name(parser, "Expected function name.");
    mark_initialized(parser);
    parse_function_definition(parser, TYPE_FUNCTION, parser->previous);
    define_variable(parser, index);
}


static void parse_method_declaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expected method name.");
    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);

    FnType type = TYPE_METHOD;
    if (parser->previous.length == 5 && memcmp(parser->previous.start, "$init", 5) == 0) {
        type = TYPE_INIT_METHOD;
    }
    parse_function_definition(parser, type, parser->previous);

    emit_op_u16(parser, OP_METHOD, index);
}


static void parse_field_declaration(Parser* parser) {
    do {
        consume(parser, TOKEN_IDENTIFIER, "Expected field name.");
        uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);

        if (match(parser, TOKEN_EQUAL)) {
            parse_expression(parser, true, true);
        } else {
            emit_byte(parser, OP_LOAD_NULL);
        }

        emit_op_u16(parser, OP_FIELD, index);
    } while (match(parser, TOKEN_COMMA));
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after field declaration.");
}


static void parse_class_declaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expected class name.");
    Token class_name = parser->previous;

    uint16_t index = make_string_constant_from_identifier(parser, &parser->previous);
    declare_variable(parser, parser->previous);

    emit_op_u16(parser, OP_CLASS, index);
    define_variable(parser, index);

    // Push a new class compiler onto the implicit linked-list stack.
    ClassCompiler class_compiler;
    class_compiler.name = parser->previous;
    class_compiler.has_superclass = false;
    class_compiler.enclosing = parser->class_compiler;
    parser->class_compiler = &class_compiler;

    if (match(parser, TOKEN_LESS)) {
        consume(parser, TOKEN_IDENTIFIER, "Expected superclass name.");
        get_named_variable(parser, parser->previous);

        // We declare 'super' as a local variable in a new lexical scope wrapping the method
        // declarations so it can be captured by the upvalue machinery.
        begin_scope(parser);
        add_local(parser, syntoken("super"));
        define_variable(parser, 0);

        get_named_variable(parser, class_name);
        emit_byte(parser, OP_INHERIT);
        class_compiler.has_superclass = true;
    }

    // Load the class object back onto the top of the stack.
    get_named_variable(parser, class_name);

    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before class body.");
    while (true) {
        if (match(parser, TOKEN_DEF)) {
            parse_method_declaration(parser);
        } else if (match(parser, TOKEN_VAR)) {
            parse_field_declaration(parser);
        } else {
            break;
        }
    }
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after class body.");
    emit_byte(parser, OP_POP); // pop the class object

    if (class_compiler.has_superclass) {
        end_scope(parser);
    }

    // Pop the class compiler.
    parser->class_compiler = class_compiler.enclosing;
}


static void parse_return_stmt(Parser* parser) {
    if (parser->compiler->type == TYPE_MODULE) {
        err_at_prev(parser, "Can't return from top-level code.");
    }

    if (match(parser, TOKEN_SEMICOLON)) {
        emit_return(parser);
    } else {
        if (parser->compiler->type == TYPE_INIT_METHOD) {
            err_at_prev(parser, "Can't return a value from an initializer.");
        }
        parse_expression(parser, true, true);
        consume(parser, TOKEN_SEMICOLON, "Expected ';' after return value.");
        emit_byte(parser, OP_RETURN);
    }
}


static void parse_break_stmt(Parser* parser) {
    if (parser->compiler->loop_compiler == NULL) {
        err_at_prev(parser, "Invalid use of 'break' outside a loop.");
        return;
    }
    parser->compiler->loop_compiler->had_break = true;

    discard_locals(parser, parser->compiler->loop_compiler->start_depth + 1);
    emit_jump(parser, OP_BREAK);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'break'.");
}


static void parse_continue_stmt(Parser* parser) {
    if (parser->compiler->loop_compiler == NULL) {
        err_at_prev(parser, "Invalid use of 'continue' outside a loop.");
        return;
    }

    discard_locals(parser, parser->compiler->loop_compiler->start_depth + 1);
    emit_loop(parser, parser->compiler->loop_compiler->start_index);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'continue'.");
}


static void parse_statement(Parser* parser) {
    if (match(parser, TOKEN_LEFT_BRACE)) {
        begin_scope(parser);
        parse_block(parser);
        end_scope(parser);
    } else if (match(parser, TOKEN_VAR)) {
        parse_var_declaration(parser);
    } else if (match(parser, TOKEN_DEF)) {
        parse_function_declaration(parser);
    } else if (match(parser, TOKEN_CLASS)) {
        parse_class_declaration(parser);
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
    } else {
        parse_expression_stmt(parser);
    }
}


/* ==================================== */
/* ~~~~~~~~ Compiler Interface ~~~~~~~~ */
/* ==================================== */


ObjFn* pyro_compile(PyroVM* vm, const char* src_code, size_t src_len, const char* src_id, ObjModule* module) {
    Parser parser;
    parser.module = module;
    parser.compiler = NULL;
    parser.class_compiler = NULL;
    parser.had_error = false;
    parser.src_id = src_id;
    parser.vm = vm;
    vm->parser = &parser;

    // Strip any trailing whitespace. This ensures we report the correct line number for syntax
    // errors at the end of the input, e.g. a missing trailing semicolon.
    while (src_len > 0 && is_whitespace(src_code[src_len - 1])) {
        src_len--;
    }

    init_lexer(&parser.lexer, vm, src_code, src_len, src_id);

    FnCompiler compiler;
    init_fn_compiler(&parser, &compiler, TYPE_MODULE, syntoken("<module>"));

    // Prime the token pump.
    advance(&parser);

    while (!match(&parser, TOKEN_EOF)) {
        parse_statement(&parser);
        if (parser.had_error) {
            vm->parser = NULL;
            return NULL;
        }
    }

    ObjFn* fn = end_fn_compiler(&parser);
    vm->parser = NULL;
    return fn;
}


// This function is called by the garbage collector. If we're in the middle of compiling when
// a collection triggers, we mark as reachable every object that can be directly accessed by
// the compiler machinery.
void pyro_mark_compiler_roots(PyroVM* vm) {
    if (vm->parser != NULL) {
        FnCompiler* compiler = vm->parser->compiler;
        while (compiler != NULL) {
            pyro_mark_object(vm, (Obj*)compiler->fn);
            compiler = compiler->enclosing;
        }
    }
}

