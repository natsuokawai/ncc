#include "ncc.h"

static int depth;
static char *argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Function *current_fn;

static void gen_expr(Node *node);

static void push(void) {
    printf("  push %%rax\n");
    depth++;
}

static void pop(char *arg) {
    printf("  pop  %s\n", arg);
    depth--;
}

// Round up `n` to the nearest multiple of `align`.
static int align_to(int n, int align) {
    return (n + align - 1) / align * align;
}

// Compute the absolute address of a given node
// It's an error if a given node dees not reside in memory
static void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// Align offsets to local variables
static void assign_lvar_offsets(Function *prog) {
    int offset = 0;
    for (Function *fn = prog; fn; fn = fn->next) {
        for (Obj *var = fn->locals; var; var = var->next) {
            offset += 8;
            var->offset = -offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
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
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        printf("  mov (%%rax), %%rax\n");
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
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; i--) {
            pop(argreg[i]);
        }

        printf("  mov $0, %%rax\n");
        printf("  call %s\n", node->funcname);
        return;
    }
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

    error_tok(node->tok, "invalid expression");
}

static int count(void) {
    static int i = 1;
    return i++;
}

static void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen_stmt(n);
        }
        return;
    case ND_IF_STMT: {
        int c = count();
        gen_expr(node->cond);
        printf("  cmp $0, %%rax\n");
        printf("  je .L.else.%d\n", c); // if cond == 0, jump to .L.else
        gen_stmt(node->then);
        printf("  jmp .L.end.%d\n", c); // jump to .L.end for not entering else block
        printf(".L.else.%d:\n", c);
        if (node->els) {
            gen_stmt(node->els);
        }
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR_STMT: {
        int c = count();
        if (node->init) {
            gen_expr(node->init);
        }
        printf(".L.loop.%d:\n", c);
        if (node->cond) {
            gen_expr(node->cond);
            printf("  cmp $0, %%rax\n");
            printf("  je .L.end.%d\n", c); // if cond == 0, jump to .L.end
        }
        gen_stmt(node->then);
        if (node->update) {
            gen_expr(node->update);
        }
        printf("  jmp .L.loop.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_RET_STMT:
        gen_expr(node->lhs);
        printf("  jmp .L.return.%s\n", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "invalid statement");
}

void codegen(Function *prog) {
    for (Function *fn = prog; fn; fn = fn->next) {
        assign_lvar_offsets(fn);
        printf("  .global main\n");
        printf("%s:\n", fn->name);
        current_fn = fn;

        // Prologue
        printf("  push %%rbp\n");
        printf("  mov %%rsp, %%rbp\n");
        printf("  sub $%d, %%rsp\n", fn->stack_size);

        // Save passed-by-register arguments to the stack
        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            printf("  mov %s, %d(%%rbp)\n", argreg[i++], var->offset);
        }

        // Traverse the AST to emit assembly
        gen_stmt(fn->body);
        assert(depth == 0);

        // Epilogue
        printf(".L.return.%s:\n", fn->name);
        printf("  mov %%rbp, %%rsp\n");
        printf("  pop %%rbp\n");
        printf("  ret\n");
    }
}
