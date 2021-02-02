#include "ncc.h"

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
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
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
    char *keywords[] = { "return", "if", "else", "for", "while", "int", "sizeof" };
    for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
        if (equal(tok, keywords[i]))
            return true;
    }
    return false;
}

static void convert_keywords(Token *tok) {
    for (Token *t = tok; t; t = t->next) {
        if (is_keyword(t)) {
            t->kind = TK_KEYWORD;
        }
    }
}

Token *tokenize(char *input) {
   current_input = input;
   char *p = current_input;
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

