#include "ncc.h"

Obj *locals;

static Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->tok  = tok;
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node *new_var_node(Obj *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

static Obj *new_lvar(char *name) {
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->next = locals;
    locals = var;
    return var;
}

static Node *new_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Obj *find_var(Token *tok) {
    for (Obj *var = locals; var; var = var->next) {
        if (strlen(var->name) == tok->len && !strncmp(var->name, tok->loc, tok->len)) {
            return var;
        }
    }

    return NULL;
}

static Node *expr(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *return_stmt(Token **rest, Token *tok);
static Node *if_stmt(Token **rest, Token *tok);
static Node *for_stmt(Token **rest, Token *tok);
static Node *while_stmt(Token **rest, Token *tok);
static Node *block(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relation(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// stmt = expr-stmt | return-stmt | if-stmt | for-stmt | while-stmt
static Node *stmt(Token **rest, Token *tok) {
    if (equal(tok, "return")) {
        return return_stmt(rest, tok);
    }
    if (equal(tok, "if")) {
        return if_stmt(rest, tok);
    }
    if (equal(tok, "for")) {
        return for_stmt(rest, tok);
    }
    if (equal(tok, "while")) {
        return while_stmt(rest, tok);
    }
    if (equal(tok, "{")) {
        return block(rest, tok);
    }
    return expr_stmt(rest, tok);
}

// expr-stmt = expr? ";"
static Node *expr_stmt(Token **rest, Token *tok) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    Node *node = new_unary(ND_EXPR_STMT, expr(&tok, tok), tok);
    *rest = skip(tok, ";");
    return node;
}

// return-stmt = return expr ";"
static Node *return_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "return");

    Node *node = new_unary(ND_RET_STMT, expr(&tok, tok), tok);
    *rest = skip(tok, ";");
    return node;
}

// if-stmt = if "(" expr ")" stmt ( else stmt )
static Node *if_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "if");

    Node *node = new_node(ND_IF_STMT, tok);
    tok = skip(tok, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);

    if (equal(tok, "else")) {
        node->els = stmt(&tok, tok->next);
    }

    *rest = tok;
    return node;
}

// for-stmt = for "(" expr? ";" expr? ";" expr ")" stmt
static Node *for_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "for");

    Node *node = new_node(ND_FOR_STMT, tok);
    tok = skip(tok, "(");

    // parse initialize expression
    if (!equal(tok, ";")) {
        node->init = expr(&tok, tok);
    }
    tok = skip(tok, ";");

    // parse test expression
    if (!equal(tok, ";")) {
        node->cond = expr(&tok, tok);
    }
    tok = skip(tok, ";");

    // parse update expression
    if (!equal(tok, ")")) {
        node->update = expr(&tok, tok);
    }
    tok = skip(tok, ")");

    node->then = stmt(&tok, tok);

    *rest = tok;
    return node;
}

// while-stmt = while "(" expr ")" stmt
static Node *while_stmt(Token **rest, Token *tok) {
    tok = skip(tok, "while");

    Node *node = new_node(ND_FOR_STMT, tok);
    tok = skip(tok, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);

    *rest = tok;
    return node;
}

static Node *block(Token **rest, Token *tok) {
    tok = skip(tok, "{");

    Node *node = new_node(ND_BLOCK, tok);
    Node head = {};
    Node *body = &head;

    while (!equal(tok, "}")) {
        body = body->next = stmt(&tok, tok);
    }
    node->body = head.next;
    tok = skip(tok, "}");

    *rest = tok;
    return node;
}

// expr = assign
static Node *expr(Token **rest, Token *tok) {
    return assign(rest, tok);
}

// assign = equality ("=" assign)?
static Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);
    if (equal(tok, "=")) {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
    }
    *rest = tok;
    return node;
}

// equality = relation ("==" relation | "!=" relation)*
static Node *equality(Token **rest, Token *tok) {
    Node *node = relation(&tok, tok);

    for (;;) {
        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relation(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relation(&tok, tok->next), tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relation = add ("<=" add | "<" add | ">=" add | ">" add)*
static Node *relation(Token **rest, Token *tok) {
    Node *node = add(&tok, tok);

    for (;;) {
        if (equal(tok, "<=")) {
            node = new_binary(ND_LE, node, add(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LE, relation(&tok, tok->next), node, tok);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, relation(&tok, tok->next), node, tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (equal(tok, "+")) {
            node = new_binary(ND_ADD, node, mul(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_binary(ND_SUB, node, mul(&tok, tok->next), tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);

    for (;;) {
        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next), tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary = ("+" | "-" | "&" | "*") unary | primay
static Node *unary(Token **rest, Token *tok) {
    if (equal(tok, "+")) {
        return unary(rest, tok->next);
    }
    if (equal(tok, "-")) {
        return new_unary(ND_NEG, unary(rest, tok->next), tok);
    }
    if (equal(tok, "&")) {
        return new_unary(ND_ADDR, unary(rest, tok->next), tok);
    }
    if (equal(tok, "*")) {
        return new_unary(ND_DEREF, unary(rest, tok->next), tok);
    }

    return primary(rest, tok);
}

// primary = "(" expr ")" | num
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_IDENT) {
        Obj *var = find_var(tok);
        if (!var) {
            var = new_lvar(strndup(tok->loc, tok->len));
        }
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
}

Function *parse(Token **rest, Token *tok) {
    Function *prog = calloc(1, sizeof(Function));
    prog->body = block(&tok, tok);
    prog->locals = locals;

    return prog;
}
