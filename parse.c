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

static Obj *new_lvar(char *name, Type *ty) {
    Obj *var = calloc(1, sizeof(Obj));
    var->ty = ty;
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

static Type *declspec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *return_stmt(Token **rest, Token *tok);
static Node *if_stmt(Token **rest, Token *tok);
static Node *for_stmt(Token **rest, Token *tok);
static Node *while_stmt(Token **rest, Token *tok);
static Node *block(Token **rest, Token *tok);
static Node *declaration(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relation(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

// stmt = expr-stmt | return-stmt | if-stmt | for-stmt | while-stmt | block
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

static char *get_ident(Token *tok) {
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected an identifier");
    }
    return strndup(tok->loc, tok->len);
}

static int get_number(Token *tok) {
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected a number");
    }
    return tok->val;
}

// declspec = "int"
static Type *declspec(Token **rest, Token *tok) {
    *rest = skip(tok, "int");
    return ty_int;
}

// type-suffix = ("(" func-params? ")")?
// func-params = param ("," param)*
// param       = declspec declarator
static Type *type_suffix(Token **rest, Token *tok, Type *ty) {
    if (equal(tok, "(")) {
        tok = tok->next;

        Type head = {};
        Type *cur = &head;

        while (!equal(tok, ")")) {
            if (cur != &head) {
                tok = skip(tok, ",");
            }
            Type *basety = declspec(&tok, tok);
            Type *ty = declarator(&tok, tok, basety);
            cur = cur->next = copy_type(ty);
        }

        ty = func_type(ty);
        ty->params = head.next;
        *rest = tok->next;
        return ty;
    }

    if (equal(tok, "[")) {
        int sz = get_number(tok->next);
        tok = skip(tok->next->next, "]");
        ty = type_suffix(rest, tok, ty);
        return array_of(ty, sz);
    }

    *rest = tok;
    return ty;
}

// declarator = "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty) {
    while (consume(&tok, tok, "*")) {
        ty = pointer_to(ty);
    }
    if (tok->kind != TK_IDENT) {
        error_tok(tok, "expected a variable name");
    }

    ty = type_suffix(rest, tok->next, ty);
    ty->name = tok;
    return ty;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok) {
    Type *basety = declspec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0) {
            tok = skip(tok, ",");
        }
        Type *ty = declarator(&tok, tok, basety);
        Obj *var = new_lvar(get_ident(ty->name), ty);

        if (!equal(tok, "=")) {
            continue;
        }

        Node *lhs = new_var_node(var, ty->name);
        Node *rhs = assign(&tok, tok->next);
        Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// block = "{" (declaration | stmt)* "}"
static Node *block(Token **rest, Token *tok) {
    tok = skip(tok, "{");

    Node *node = new_node(ND_BLOCK, tok);
    Node head = {};
    Node *body = &head;

    while (!equal(tok, "}")) {
        if (equal(tok, "int")) {
            body = body->next = declaration(&tok, tok);
        } else {
            body = body->next = stmt(&tok, tok);
        }
        add_type(body);
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

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    // ptr + prt is invalid
    if (lhs->ty->base && rhs->ty->base) {
        error_tok(tok, "invalid operands");
    }

    // num + num
    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_ADD, lhs, rhs, tok);
    }

    // num + ptr is converted to ptr + num
    if (!lhs->ty->base && rhs->ty->base) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    // ptr + num
    rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    // num - num
    if (is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_SUB, lhs, rhs, tok);
    }

    // ptr - num
    if (lhs->ty->base && is_integer(rhs->ty)) {
        rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
        add_type(rhs);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }

    // ptr - ptr: the number of elements between then
    if (lhs->ty->base && rhs->ty->base) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
    }

    error_tok(tok, "invalid operands");
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        Token *start = tok;

        if (equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next), start);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next), start);
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

// funcall = ident "(" (assign ("," assign)*)? ")"
static Node *funcall(Token **rest, Token *tok) {
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head) {
            tok = skip(tok, ",");
        }
        cur = cur->next = assign(&tok, tok);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FUNCALL, start);
    node->funcname = strndup(start->loc, start->len);
    node->args = head.next;
    return node;
}

// primary = "(" expr ")" | ident func-args? | num
static Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_IDENT) {
        // Function call
        if (equal(tok->next, "(")) {
            return funcall(rest, tok);
        }

        // Variable
        Obj *var = find_var(tok);
        if (!var) {
            error_tok(tok, "undefined variable");
        }
        Node *node = new_var_node(var, tok);
        if (equal(tok->next, "[")) {
            node = new_add(node, expr(&tok, tok->next->next), tok);
            *rest = skip(tok, "]");
            return new_unary(ND_DEREF, node, tok);
        }
        *rest = tok->next;
        return node;
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok);
        if (equal(tok->next, "[")) {
            node = new_add(node, expr(&tok, tok->next->next), tok);
            *rest = skip(tok, "]");
            return new_unary(ND_DEREF, node, tok);
        }
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
}

static void create_param_lvars(Type *param) {
    if (param) {
        create_param_lvars(param->next);
        new_lvar(get_ident(param->name), param);
    }
}

static Function *function(Token **rest, Token *tok) {
    Type *ty = declspec(&tok, tok);
    ty = declarator(&tok, tok, ty);

    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = get_ident(ty->name);
    create_param_lvars(ty->params);
    fn->params = locals;

    fn->body = block(rest, tok);
    fn->locals = locals;
    return fn;
}

Function *parse(Token *tok) {
    Function head = {};
    Function *cur = &head;

    while (tok->kind != TK_EOF) {
        cur = cur->next = function(&tok, tok);
    }
    return head.next;
}
