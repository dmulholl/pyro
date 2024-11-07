#include "../includes/pyro.h"


static bool is_alpha_or_underscore(char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c == '_') return true;
    return false;
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


static bool is_alpha_or_digit_or_underscore(char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}


static inline bool is_at_end(Lexer* lexer) {
    return lexer->next == lexer->end;
}


static inline char next_char(Lexer* lexer) {
    lexer->next++;
    return lexer->next[-1];
}


static char peek(Lexer* lexer) {
    if (lexer->next == lexer->end) return '\0';
    return *lexer->next;
}


static char peek2(Lexer* lexer) {
    if (lexer->next + 1 >= lexer->end) return '\0';
    return lexer->next[1];
}


static bool match_char(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->next != expected) return false;
    lexer->next++;
    return true;
}


static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->next - lexer->start);
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
    if (lexer->next - lexer->start == (ptrdiff_t)length) {
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
            if (check_keyword(lexer, "enum")) return TOKEN_ENUM;
            if (check_keyword(lexer, "extends")) return TOKEN_EXTENDS;
            break;

        case 'f':
            if (check_keyword(lexer, "false")) return TOKEN_FALSE;
            if (check_keyword(lexer, "for")) return TOKEN_FOR;
            break;

        case 'i':
            if (check_keyword(lexer, "i64_add")) return TOKEN_I64_ADD;
            if (check_keyword(lexer, "if")) return TOKEN_IF;
            if (check_keyword(lexer, "import")) return TOKEN_IMPORT;
            if (check_keyword(lexer, "in")) return TOKEN_IN;
            break;

        case 'l':
            if (check_keyword(lexer, "let")) return TOKEN_LET;
            if (check_keyword(lexer, "loop")) return TOKEN_LOOP;
            break;

        case 'm':
            if (check_keyword(lexer, "mod")) return TOKEN_MOD;
            break;

        case 'n':
            if (check_keyword(lexer, "null")) return TOKEN_NULL;
            break;

        case 'p':
            if (check_keyword(lexer, "pri")) return TOKEN_PRI;
            if (check_keyword(lexer, "pub")) return TOKEN_PUB;
            break;

        case 'r':
            if (check_keyword(lexer, "rem")) return TOKEN_REM;
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
    while (is_alpha_or_digit_or_underscore(peek(lexer))) {
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

    if (peek(lexer) == '.' && pyro_is_ascii_decimal_digit(peek2(lexer))) {
        next_char(lexer);
        while (is_digit_or_underscore(peek(lexer))) {
            next_char(lexer);
        }
    }

    if (peek(lexer) == 'e' && is_digit_or_plus_or_minus(peek2(lexer))) {
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

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated raw string literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


// Are we at the beginning of a valid backslashed escape sequence? We've already parsed the
// backslash and the input contains at least one more character.
static bool is_valid_backslashed_escape(Lexer* lexer) {
    size_t chars_remaining = lexer->end - lexer->next;

    switch (peek(lexer)) {
        case '\\':
        case '0':
        case '"':
        case '\'':
        case '$':
        case '{':
        case 'b':
        case 'e':
        case 'n':
        case 'r':
        case 't':
            return true;

        case 'x': {
            if (chars_remaining >= 3 &&
                pyro_is_ascii_hex_digit(lexer->next[1]) &&
                pyro_is_ascii_hex_digit(lexer->next[2])
            ) {
                return true;
            }

            pyro_panic_with_syntax_error(
                lexer->vm,
                lexer->src_id,
                lexer->line,
                "invalid escape sequence, \\x must be followed by 2 hexadecimal digits",
                lexer->next
            );
            return false;
        }

        case 'u': {
            if (chars_remaining >= 5 &&
                pyro_is_ascii_hex_digit(lexer->next[1]) &&
                pyro_is_ascii_hex_digit(lexer->next[2]) &&
                pyro_is_ascii_hex_digit(lexer->next[3]) &&
                pyro_is_ascii_hex_digit(lexer->next[4])
            ) {
                return true;
            }

            pyro_panic_with_syntax_error(
                lexer->vm,
                lexer->src_id,
                lexer->line,
                "invalid escape sequence, \\u must be followed by 4 hexadecimal digits",
                lexer->next
            );
            return false;
        }

        case 'U': {
            if (chars_remaining >= 9 &&
                pyro_is_ascii_hex_digit(lexer->next[1]) &&
                pyro_is_ascii_hex_digit(lexer->next[2]) &&
                pyro_is_ascii_hex_digit(lexer->next[3]) &&
                pyro_is_ascii_hex_digit(lexer->next[4]) &&
                pyro_is_ascii_hex_digit(lexer->next[5]) &&
                pyro_is_ascii_hex_digit(lexer->next[6]) &&
                pyro_is_ascii_hex_digit(lexer->next[7]) &&
                pyro_is_ascii_hex_digit(lexer->next[8])
            ) {
                return true;
            }

            pyro_panic_with_syntax_error(
                lexer->vm,
                lexer->src_id,
                lexer->line,
                "invalid escape sequence, \\U must be followed by 8 hexadecimal digits",
                lexer->next
            );
            return false;
        }

        default: {
            const char* error_message = "invalid escape sequence \\(0x%02X)";
            if (pyro_is_ascii_printable(lexer->next[0])) {
                error_message = "invalid escape sequence \\%c";
            }

            pyro_panic_with_syntax_error(
                lexer->vm,
                lexer->src_id,
                lexer->line,
                error_message,
                lexer->next[0]
            );
            return false;
        }
    }
}


static Token read_double_quote_string(Lexer* lexer) {
    bool contains_escapes = false;
    size_t start_line = lexer->line;

    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '\\') {
            if (is_at_end(lexer)) {
                break;
            }
            if (is_valid_backslashed_escape(lexer)) {
                contains_escapes = true;
                next_char(lexer);
                continue;
            }
            return make_error_token(lexer);
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

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated string literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


static Token read_rune_literal(Lexer* lexer) {
    size_t start_line = lexer->line;

    while (!is_at_end(lexer)) {
        char c = next_char(lexer);

        if (c == '\n') {
            lexer->line++;
            continue;
        }

        if (c == '\\') {
            if (is_at_end(lexer)) {
                break;
            }
            if (is_valid_backslashed_escape(lexer)) {
                next_char(lexer);
                continue;
            }
            return make_error_token(lexer);
        }

        if (c == '\'') {
            Token token = make_token(lexer, TOKEN_RUNE);
            token.start += 1;
            token.length -= 2;
            return token;
        }
    }

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated character literal, opened in line %zu",
        start_line
    );

    return make_error_token(lexer);
}


// If this function is called, we're inside a double-quoted string containing ${} elements. The last
// token to be parsed was '}', closing a ${} element. We want to parse the next string-fragment.
// There are only three possible values we can return:
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
            if (is_at_end(lexer)) {
                break;
            }
            if (is_valid_backslashed_escape(lexer)) {
                next_char(lexer);
                continue;
            }
            return make_error_token(lexer);
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

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated string literal"
    );

    return make_error_token(lexer);
}


// If this function is called, we're inside a ${expression;format_specifier} element. The last token
// to be parsed was ';'. We want to parse the format specifier.
static Token next_format_specifier_token(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);

        if (c == '}') {
            return make_token(lexer, TOKEN_FORMAT_SPECIFIER);
        }

        if (c == '\n') {
            lexer->line++;
        }

        next_char(lexer);
    }

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        "unterminated format specifier"
    );

    return make_error_token(lexer);
}


Token pyro_next_token(Lexer* lexer) {
    lexer->start = lexer->next;

    if (lexer->format_specifier_mode) {
        return next_format_specifier_token(lexer);
    }

    if (lexer->interpolated_string_mode) {
        return next_interpolated_string_token(lexer);
    }

    skip_whitespace_and_comments(lexer);
    lexer->start = lexer->next;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    char c = next_char(lexer);

    if (is_alpha_or_underscore(c) || c == '$') {
        return read_identifier(lexer);
    }

    if (c == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        next_char(lexer);
        while (pyro_is_ascii_hex_digit(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_HEX_INT);
    }

    if (c == '0' && (peek(lexer) == 'o' || peek(lexer) == 'O')) {
        next_char(lexer);
        while (pyro_is_ascii_octal_digit(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_OCTAL_INT);
    }

    if (c == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        next_char(lexer);
        while (pyro_is_ascii_binary_digit(peek(lexer)) || peek(lexer) == '_') {
            next_char(lexer);
        }
        return make_token(lexer, TOKEN_BINARY_INT);
    }

    if (pyro_is_ascii_decimal_digit(c)) {
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

        case '"': return read_double_quote_string(lexer);
        case '`': return read_backtick_string(lexer);
        case '\'': return read_rune_literal(lexer);

        case ':':
            if (match_char(lexer, ':')) return make_token(lexer, TOKEN_COLON_COLON);
            if (match_char(lexer, '|')) return make_token(lexer, TOKEN_COLON_BAR);
            if (match_char(lexer, '?')) return make_token(lexer, TOKEN_COLON_HOOK);
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
    if (pyro_is_ascii_printable(c)) {
        error_message = "unexpected character '%c' in input";
    }

    pyro_panic_with_syntax_error(
        lexer->vm,
        lexer->src_id,
        lexer->line,
        error_message,
        (unsigned char)c
    );

    return make_error_token(lexer);
}


void pyro_init_lexer(Lexer* lexer, PyroVM* vm, const char* src_code, size_t src_len, const char* src_id) {
    lexer->start = src_code;
    lexer->next = src_code;
    lexer->end = src_code + src_len;
    lexer->line = 1;
    lexer->vm = vm;
    lexer->src_id = src_id;
    lexer->interpolated_string_mode = false;
    lexer->format_specifier_mode = false;
}
