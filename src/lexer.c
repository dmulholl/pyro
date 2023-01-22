#include "../inc/pyro.h"


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


static bool is_digit_or_plus_or_minus(char c) {
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


static bool is_hex_digit(char c) {
    if (c >= '0' && c <= '9') return true;
    if (c >= 'A' && c <= 'F') return true;
    if (c >= 'a' && c <= 'f') return true;
    return false;
}


static bool is_octal_digit(char c) {
    if (c >= '0' && c <= '7') return true;
    return false;
}


static bool is_binary_digit(char c) {
    if (c == '0' || c == '1') return true;
    return false;
}


static inline bool is_at_end(Lexer* lexer) {
    return lexer->current == lexer->end;
}


static inline char next_char(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}


static char peek(Lexer* lexer) {
    if (lexer->current == lexer->end) return '\0';
    return *lexer->current;
}


static char peek_next(Lexer* lexer) {
    if (lexer->current + 1 >= lexer->end) return '\0';
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
    token.start = "";
    token.length = 0;
    token.line = lexer->line;
    return token;
}


static void skip_whitespace_and_comments(Lexer* lexer) {
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
            if (check_keyword(lexer, "null")) return TOKEN_NULL;
            break;

        case 'p':
            if (check_keyword(lexer, "pri")) return TOKEN_PRI;
            if (check_keyword(lexer, "pub")) return TOKEN_PUB;
            break;

        case 'r':
            if (check_keyword(lexer, "return")) return TOKEN_RETURN;
            break;

        case 's':
            if (check_keyword(lexer, "self")) return TOKEN_SELF;
            if (check_keyword(lexer, "static")) return TOKEN_STATIC;
            if (check_keyword(lexer, "super")) return TOKEN_SUPER;
            break;

        case 't':
            if (check_keyword(lexer, "true")) return TOKEN_TRUE;
            if (check_keyword(lexer, "try")) return TOKEN_TRY;
            if (check_keyword(lexer, "typedef")) return TOKEN_TYPEDEF;
            break;

        case 'v':
            if (check_keyword(lexer, "var")) return TOKEN_VAR;
            break;

        case 'w':
            if (check_keyword(lexer, "with")) return TOKEN_WITH;
            if (check_keyword(lexer, "while")) return TOKEN_WHILE;
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

    if (peek(lexer) == 'e' && is_digit_or_plus_or_minus(peek_next(lexer))) {
        next_char(lexer);
        next_char(lexer);
        while (is_digit_or_underscore(peek(lexer))) {
            next_char(lexer);
        }
    }

    return make_token(lexer, TOKEN_FLOAT);
}


static Token read_backtick_string(Lexer* lexer) {
    size_t start_line = lexer->line;

    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '`') {
            Token token = make_token(lexer, TOKEN_RAW_STRING);
            token.start += 1;
            token.length -= 2;
            return token;
        }
    }

    pyro_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated raw string literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


static Token read_double_quoted_string(Lexer* lexer) {
    bool contains_escapes = false;
    size_t start_line = lexer->line;

    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '\\') {
            if (!is_at_end(lexer)) {
                next_char(lexer);
            }
            contains_escapes = true;
            continue;
        }

        if (c == '$') {
            if (peek(lexer) == '{') {
                next_char(lexer);
                Token token = make_token(lexer, TOKEN_STRING_FRAGMENT);
                token.start += 1;
                token.length -= 3;
                return token;
            }
            continue;
        }

        if (c == '"') {
            TokenType type = contains_escapes ? TOKEN_ESCAPED_STRING : TOKEN_RAW_STRING;
            Token token = make_token(lexer, type);
            token.start += 1;
            token.length -= 2;
            return token;
        }
    }

    pyro_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated string literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


static Token read_char_literal(Lexer* lexer) {
    size_t start_line = lexer->line;

    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '\\') {
            if (!is_at_end(lexer)) {
                next_char(lexer);
            }
            continue;
        }

        if (c == '\'') {
            return make_token(lexer, TOKEN_CHAR);
        }
    }

    pyro_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated character literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


// We're already inside the string. The last token to be parsed was '}', closing an
// ${interpolation} element. There are only three possible values we can return:
// - TOKEN_STRING_FRAGMENT if we meet a ${
// - TOKEN_STRING_FRAGMENT_FINAL if we meet a "
// - TOKEN_ERROR if the string is unterminated
static Token next_interpolated_string_token(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '\\') {
            if (!is_at_end(lexer)) {
                next_char(lexer);
            }
            continue;
        }

        if (c == '$') {
            if (peek(lexer) == '{') {
                next_char(lexer);
                Token token = make_token(lexer, TOKEN_STRING_FRAGMENT);
                token.length -= 2;
                return token;
            }
            continue;
        }

        if (c == '"') {
            Token token = make_token(lexer, TOKEN_STRING_FRAGMENT_FINAL);
            token.length -= 1;
            return token;
        }
    }

    pyro_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated string literal"
    );

    return make_error_token(lexer);
}


Token pyro_next_token(Lexer* lexer) {
    lexer->start = lexer->current;
    if (lexer->interpolated_string_mode) {
        return next_interpolated_string_token(lexer);
    }

    skip_whitespace_and_comments(lexer);
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
        while (is_hex_digit(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_HEX_INT);
    }

    if (c == '0' && (peek(lexer) == 'o' || peek(lexer) == 'O')) {
        next_char(lexer);
        while (is_octal_digit(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_OCTAL_INT);
    }

    if (c == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        next_char(lexer);
        while (is_binary_digit(peek(lexer)) || peek(lexer) == '_') {
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
        case '~': return make_token(lexer, TOKEN_TILDE);

        case '+': return make_token(lexer, match_char(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '/': return make_token(lexer, match_char(lexer, '/') ? TOKEN_SLASH_SLASH : TOKEN_SLASH);
        case '=': return make_token(lexer, match_char(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '*': return make_token(lexer, match_char(lexer, '*') ? TOKEN_STAR_STAR : TOKEN_STAR);
        case '|': return make_token(lexer, match_char(lexer, '|') ? TOKEN_BAR_BAR : TOKEN_BAR);
        case '?': return make_token(lexer, match_char(lexer, '?') ? TOKEN_HOOK_HOOK: TOKEN_HOOK);
        case '&': return make_token(lexer, match_char(lexer, '&') ? TOKEN_AMP_AMP: TOKEN_AMP);

        case '"': return read_double_quoted_string(lexer);
        case '`': return read_backtick_string(lexer);
        case '\'': return read_char_literal(lexer);

        case ':':
            if (match_char(lexer, ':')) return make_token(lexer, TOKEN_COLON_COLON);
            if (match_char(lexer, '|')) return make_token(lexer, TOKEN_COLON_BAR);
            return make_token(lexer, TOKEN_COLON);

        case '-':
            if (match_char(lexer, '=')) return make_token(lexer, TOKEN_MINUS_EQUAL);
            if (match_char(lexer, '>')) return make_token(lexer, TOKEN_RIGHT_ARROW);
            return make_token(lexer, TOKEN_MINUS);

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

    const char* error_message = "unexpected byte value (0x%02X) in input";
    if (isprint(c)) {
        error_message = "unexpected character '%c' in input";
    }

    pyro_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        error_message,
        c
    );

    return make_error_token(lexer);
}


void pyro_init_lexer(Lexer* lexer, PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    lexer->start = src_code;
    lexer->current = src_code;
    lexer->end = src_code + src_len;
    lexer->line = 1;
    lexer->vm = vm;
    lexer->src_id = src_id;
    lexer->interpolated_string_mode = false;
}