#include "ncc.h"
#include <stdio.h>

// Input filename
static char *current_filename;

// Input string
static char *current_input;

// Reports an error and exit
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Reports an error location and exit
static void verror_at(char *loc, char *fmt, va_list ap) {
    // `line` is pointer to the beginning character of the line
    char *line = loc;
    while (current_input < line && line[-1] != '\n') {
        line--;
    }
    char *end = loc;
    while (*end != '\n') {
        end++;
    }
    int line_no = 1;
    for (char *p = current_input; p < line; p++) {
        if (*p == '\n') {
            line_no++;
        }
    }
    int ident = fprintf(stderr, "%s:%d: ", current_filename, line_no);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);
    int pos = loc - line + ident;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

// Checks the current token if it matches `s`
bool equal(Token *tok, char *op) {
    return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

// Ensure tha† the current token is `s`
Token *skip(Token *tok, char *s) {
    if (!equal(tok, s)) {
        error_tok(tok, "expected '%s'", s);
    }
    return tok->next;
}

bool consume(Token **rest, Token *tok, char *str) {
    if (equal(tok, str)) {
        *rest = tok->next;
        return true;
    }
    *rest = tok;
    return false;
}

// Ensure that the current token is TK_NUM
static int get_number(Token *tok) {
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected a number");
    }
    return tok->val;
}

// Create a new token
Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

static bool startswith(char *p, char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

static int read_punct(char *p) {
    if (startswith(p, "==") || startswith(p, "!=") ||
        startswith(p, "<=") || startswith(p, ">=")) {
        return 2;
    }

    return ispunct(*p) ? 1 : 0;
}

static bool is_keyword(Token *tok) {
    char *keywords[] = {
        "return", "if", "else", "for", "while", "int", "sizeof", "char"
    };
    for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
        if (equal(tok, keywords[i]))
            return true;
    }
    return false;
}

static int from_hex(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - 'A' + 10;
}

static int read_escaped_char(char **new_pos, char *p) {
    if ('0' <= *p && *p <= '7') {
        int c = *p++ - '0';
        if ('0' <= *p && *p <= '7') {
            c = (c << 3) + (*p++ - '0');
            if ('0' <= *p && *p <= '7') {
                c = (c << 3) + (*p++ - '0');
            }
        }
        *new_pos = p;
        return c;
    }

    if (*p == 'x') {
        p++;
        if (!isxdigit(*p)) {
            error_at(p, "invalid hex escape sequenct");
        }

        int c = 0;
        for (; isxdigit(*p); p++) {
            c = (c << 4) + from_hex(*p);
        }
        *new_pos = p;
        return c;
    }

    *new_pos = p + 1;

    switch (*p) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'e': return 27;
    default: return *p;
    }
}

static char *string_literal_end(char *p) {
    char *start = p;
    for (; *p != '"'; p++) {
        if (*p == '\n' || *p == '\0') {
            error_at(start, "unclosed string literal");
        }
        if (*p == '\\') {
            p++;
        }
    }
    return p;
}

static Token *read_string_literal(char *start) {
    char *end = string_literal_end(start + 1);
    char *buf = calloc(1, end - start);
    int len = 0;

    for (char *p = start + 1; p < end;) {
        if (*p == '\\') {
            buf[len++] = read_escaped_char(&p, p + 1);
        } else {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, start, end + 1);
    tok->ty = array_of(ty_char, len + 1);
    tok->str = buf;
    return tok;
}

static void convert_keywords(Token *tok) {
    for (Token *t = tok; t; t = t->next) {
        if (is_keyword(t)) {
            t->kind = TK_KEYWORD;
        }
    }
}

static Token *tokenize(char *filename, char *p) {
    current_filename = filename;
   current_input = p;
   Token head = {};
   Token *cur = &head;

    while (*p) {
        // Skip whitespace characters
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Numeric Literals
        if (isdigit(*p)) {
            // cur->next = new_token(TK_NUM, p, p);
            // cur = cur->next;
            cur = cur->next = new_token(TK_NUM, p, p);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        // String
        if (*p == '"') {
            cur = cur->next = read_string_literal(p);
            p += cur->len;
            continue;
        }

        // Identifier
        if (isalpha(*p) || *p == '_') {
            char *start = p;
            do {
                p++;
            } while (isalnum(*p) || *p == '_');
            cur = cur->next = new_token(TK_IDENT, start, p);
            continue;
        }

        // Punctuator
        int punct_len = read_punct(p);
        if (punct_len) {
            // cur->next = new_token(TK_PUNCT, p, p + 1);
            // cur = cur->next;
            cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
            p += punct_len;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    convert_keywords(head.next);
    return head.next;
}

static char *read_file(char *path) {
    FILE *fp;

    if (strcmp(path, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(path, "r");
        if (!fp) {
            error("cannot open %s: %s", path, strerror(errno));
        }
    }

    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen);

    for (;;) {
        char buf2[4096];
        int n = fread(buf2, 1, sizeof(buf2), fp);
        if (n == 0) {
            break;
        }
        fwrite(buf2, 1, n, out);
    }

    if (fp != stdin) {
        fclose(fp);
    }
    fflush(out);
    if (buflen == 0 || buf[buflen - 1] == '\n') {
        fputc('\n', out);
    }
    fclose(out);
    return buf;
}

Token *tokenize_file(char *path) {
    return tokenize(path, read_file(path));
}
