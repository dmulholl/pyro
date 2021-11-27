#ifndef pyro_lexer_h
#define pyro_lexer_h

#include "common.h"

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

    // Two-character tokens.
    TOKEN_RIGHT_ARROW,

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
    TOKEN_FALSE, TOKEN_FOR,
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
    TOKEN_SYNTHETIC,
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    size_t length;
    size_t line;
} Token;

typedef struct {
    const char* start;      // Points to the first character of the current token.
    const char* current;    // Points to the next character to be read.
    const char* end;        // Points to one past the end of the input array.
    size_t line;
    const char* src_id;
    PyroVM* vm;
} Lexer;

void pyro_init_lexer(Lexer* lexer, PyroVM* vm, const char* src_code, size_t src_len, const char* src_id);
Token pyro_next_token(Lexer* lexer);

#endif
