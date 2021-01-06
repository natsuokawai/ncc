#include "ncc.h"

static int depth;

static void push(void) {
    printf("  push %%rax\n");
    depth++;
}

static void pop(char *arg) {
    printf("  pop  %s\n", arg);
    depth--;
}

// Compute the absolute address of a given node
// It's an error if a given node dees not reside in memory
static void gen_addr(Node *node) {
    if (node->kind == ND_VAR) {
        int offset = (node->name - 'a' + 1) * 8;
        printf("  lea %d(%%rbp), %%rax\n", -offset);
        return;
    }

    error("not an lvalue");
}

static void gen_expr(Node *node) {
    // printf("kind: %d\n", node->kind);
    switch (node->kind) {
    case ND_NUM:
        printf("  mov $%d, %%rax\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("  neg %%rax\n");
        return;
    case ND_VAR:
        gen_addr(node);
        printf("  mov (%%rax), %%rax\n");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("%rdi");
        printf("  mov %%rax, (%%rdi)\n");
        return;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind) {
    case ND_ADD:
        printf("  add %%rdi, %%rax\n");
        return;
    case ND_SUB:
        printf("  sub %%rdi, %%rax\n");
        return;
    case ND_MUL:
        printf("  imul %%rdi, %%rax\n");
        return;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv %%rdi\n");
        return;
    case ND_EQ:
        printf("  cmp %%rdi, %%rax\n");
        printf("  sete %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    case ND_NE:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setne %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    case ND_LT:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setl %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    case ND_LE:
        printf("  cmp %%rdi, %%rax\n");
        printf("  setle %%al\n");
        printf("  movzb %%al, %%rax\n");
        return;
    }

    error("invalid expression");
}

static void gen_stmt(Node *node) {
    if (node->kind == ND_EXPR_STMT) {
        gen_expr(node->lhs);
        return;
    }

    error("invalid statement");
}

void codegen(Node *node) {
    printf("  .global main\n");
    printf("main:\n");

    // Prologue
    printf("  push %%rbp\n");
    printf("  mov %%rsp, %%rbp\n");
    printf("  sub $208, %%rsp\n");

    // Traverse the AST to emit assembly
		for (Node *n = node; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }

    printf("  mov %%rbp, %%rsp\n");
    printf("  pop %%rbp\n");
    printf("  ret\n");

}
