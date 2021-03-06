#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;

//
// tokenize.c
//

// Token
typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords: return, if, for...
    TK_NUM,     // Numeric literals
    TK_STR,     // String literals
    TK_EOF,     // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
    TokenKind kind; // Token kind
    Token *next;    // Next token
    int val;        // If kind is TK_NUM, its value
    char *loc;      // Token location
    int len;        // Token length
    Type *ty;
    char *str;
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *s);
bool consume(Token **rest, Token *tok, char *str);
Token *new_token(TokenKind kind, char *start, char *end);
Token *tokenize_file(char *filename);


//
// parse.c
//

typedef struct Obj Obj;

// AST node
typedef enum {
    ND_ADD,        // '+'
    ND_SUB,        // '-'
    ND_MUL,        // '*'
    ND_DIV,        // '/'
    ND_EQ,         // '=='
    ND_NE,         // '!='
    ND_LT,         // '<'
    ND_LE,         // '<='
    ND_ASSIGN,     // '='
    ND_NEG,        // unary -
    ND_ADDR,       // unary &
    ND_DEREF,      // unary *
    ND_EXPR_STMT,  // Expression statement
    ND_RET_STMT,   // Return statement
    ND_IF_STMT,    // If statement
    ND_FOR_STMT,   // For statement
    ND_WHILE_STMT, // While statement
    ND_BLOCK,      // { ... }
    ND_FUNCTION,   // Function declaration
    ND_FUNCALL,    // Function call
    ND_VAR,        // Variable
    ND_NUM,        // Integer
} NodeKind;

// AST node type
typedef struct Node Node;
struct Node {
    NodeKind kind;  // Node kind
    Node *next;     // Next node
    Type *ty;       // Type, e.g. int or pointer to int
    Token *tok;     // Representative Token

    Node *lhs;      // Left-hand side
    Node *rhs;      // Right-hand side

    Node *body;     // Collection of statement Node

    char *funcname; // Functaion call
    Node *args;

    Node *cond;     // Used if "if" or "for"
    Node *then;     // Used if "if" or "for"
    Node *els;      // Used if "if" or "for"
    Node *init;     // Used if "for"
    Node *update;   // Used if "for"

    Obj *var;       // Used if kind == ND_VAR
    int val;        // Used if kind == ND_NUM
};

// Local variable or global varable/function
struct Obj {
    Obj *next;
    char *name;    // Variable name
    Type *ty;      // Variable type
    bool is_local; // local or global/function

    // Local variable
    int offset;

    // Global variable
    bool is_function;

    // Global variable
    char *init_data;

    // Function
    Obj *params;
    Node *body;
    Obj *locals;
    int stack_size;
};

Obj *parse(Token *tok);

//
// type.c
//

typedef enum {
    TY_INT,
    TY_CHAR,
    TY_PTR,
    TY_FUNC,
    TY_ARRAY,
} TypeKind;

struct Type {
    TypeKind kind;
    int size;
    Type *base;
    Token *name;
    int array_len;
    Type *return_ty;
    Type *params;
    Type *next;
};

extern Type *ty_int;
extern Type *ty_char;

bool is_integer(Type *ty);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
void add_type(Node *node);


//
// codegen.c
//

void codegen(Obj *prog);


//
// strings.c
//

char *format(char *fmt, ...);
