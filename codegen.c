#include "Ccc.h"

void gen_expr(Node *node);

int labelCounter = 0;
int depth;

void push(void) {
  printf("  push %%rax\n");
  depth++;
}

void pop(char *arg) {
  printf("  pop %s\n", arg);
  depth--;
}

void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  }
  error("not an lvalue");
}

void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  mov $%d, %%rax\n", node->val);
    return;
  case ND_VAR:
    gen_addr(node);
    printf("  mov (%%rax), %%rax\n");
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    printf("  mov (%%rax), %%rax\n");
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
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
  case ND_NE:
  case ND_LT:
  case ND_LE:
    printf("  cmp %%rdi, %%rax\n");

    if (node->kind == ND_EQ) {
      printf("  sete %%al\n");
    } else if (node->kind == ND_NE) {
      printf("  setne %%al\n");
    } else if (node->kind == ND_LT) {
      printf("  setl %%al\n");
    } else if (node->kind == ND_LE) {
      printf("  setle %%al\n");
    }
    printf("  movzb %%al, %%rax\n");
    return;
  }

  error("invalid expression");
}

void gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    int counter = labelCounter++;
    gen_expr(node->cond);
    printf("  cmp $0, %%rax\n");
    printf("  je  .L.else.%d\n", counter);
    gen_stmt(node->then);
    printf("  jmp .L.end.%d\n", counter);
    printf(".L.else.%d:\n", counter);
    if (node->els)
      gen_stmt(node->els);
    printf(".L.end.%d:\n", counter);
    return;
  }
  case ND_FOR:
  case ND_WHILE: {
    int counter = labelCounter++;
    if (node->init)
      gen_expr(node->init);
    printf(".L.begin.%d:\n", counter);
    if (node->cond) {
      gen_expr(node->cond);
      printf("  cmp $0, %%rax\n");
      printf("  je  .L.end.%d\n", counter);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    printf("  jmp .L.begin.%d\n", counter);
    printf(".L.end.%d:\n", counter);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_RETURN:
    gen_expr(node->lhs);
    printf("  jmp .L.return\n");
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }

  switch (node->kind) {
  case ND_NUM:
    error("ND_ADD");
    break;
  case ND_FOR:
    error("ND_FOR");
    break;
  case ND_BLOCK:
    error("ND_BLOCK");
    break;
  case ND_EXPR_STMT:
    error("ND_EXPR_STMT");
    break;
  case ND_RETURN:
    error("ND_RETURN");
    break;
  case ND_WHILE:
  case ND_IF:
    error("stmt");
    break;
  case ND_ASSIGN:
    error("%d", node->rhs->val);
    error("assign");
    break;
  case ND_VAR:
    error("var");
    break;
  default:
    error("default");
    break;
  }

  error("invalid statement");
}

// nを繰り上げ、alignの倍数で最も近い数を返す
// align_to(5, 8) -> 8, align_to(11, 8)
int align_to(int n, int align) { return (n + align - 1) / align * align; }

void assign_lvar_offsets(Function *prog) {
  int offset = 0;
  for (Obj *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = -offset;
  }
  prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  // アセンブリの前半部分を出力
  printf(".globl main\n");
  printf("main:\n");

  // プロローグ
  // 変数26個分の領域を確保する
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");
  printf("  sub $%d, %%rsp\n", prog->stack_size);

  // コード生成
  gen_stmt(prog->body);
  assert(depth == 0);

  // エピローグ
  // 最後の式の結果がRAXに残っているのでそれが返り値になる
  printf(".L.return:\n");
  printf("  mov %%rbp, %%rsp\n");
  printf("  pop %%rbp\n");
  printf("  ret\n");
}
