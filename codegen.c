#include "Ccc.h"

int labelCounter = 0;

void gen_lval(Node *node) {
  if (node->kind != ND_LVAR)
    error("代入の左辺値が変数ではありません");

  printf("  mov %%rbp, %%rax\n");
  printf("  sub $%d, %%rax\n", node->var->offset);
  printf("  push %%rax\n");
}

void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  push $%d\n", node->val);
    return;
  case ND_LVAR:
    gen_lval(node);
    printf("  pop %%rax\n");
    printf("  mov (%%rax), %%rax\n");
    printf("  push %%rax\n");
    return;
  case ND_ASSIGN:
    gen_lval(node->lhs);
    gen_expr(node->rhs);

    printf("  pop %%rdi\n");
    printf("  pop %%rax\n");
    printf("  mov %%rdi, (%%rax)\n");
    printf("  push %%rdi\n");
    return;
  }

  gen_expr(node->lhs);
  gen_expr(node->rhs);

  printf("  pop %%rdi\n");
  printf("  pop %%rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add %%rdi, %%rax\n");
    break;
  case ND_SUB:
    printf("  sub %%rdi, %%rax\n");
    break;
  case ND_MUL:
    printf("  imul %%rdi, %%rax\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv %%rdi\n");
    break;
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
    break;
  }

  printf("  push %%rax\n");
}

void gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    int counter = labelCounter++;
    if (node->els) {
      gen_expr(node->cond);
      printf("  pop %%rax\n");
      printf("  cmp $0, %%rax\n");
      printf("  je .Lelse%d\n", counter);
      gen_stmt(node->then);
      printf("  jmp .Lend%d\n", counter);
      printf(".Lelse%d:\n", counter);
      gen_stmt(node->els);
      printf(".Lend%d:\n", counter);
    } else {
      gen_expr(node->cond);
      printf("  pop %%rax\n");
      printf("  cmp $0, %%rax\n");
      printf("  je .Lend%d\n", counter);
      gen_stmt(node->then);
      printf(".Lend%d:\n", counter);
    }
    return;
  }
  case ND_FOR: {
    int counter = labelCounter++;
    if (node->init)
      gen_stmt(node->init);
    printf(".Lbegin%d:\n", counter);
    if (node->cond) {
      gen_expr(node->cond);
      printf("  pop %%rax\n");
      printf("  cmp $0, %%rax\n");
      printf("  je .Lend%d\n", counter);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    printf("  jmp .Lbegin%d\n", counter);
    printf(".Lend%d:", counter);
    return;
  }
  case ND_WHILE: {
    int counter = labelCounter++;
    printf(".Lbegin%d:\n", counter);
    gen_expr(node->cond);
    printf("  pop %%rax\n");
    printf("  cmp $0, %%rax\n");
    printf("  je .Lend%d\n", counter);
    gen_stmt(node->then);
    printf("  jmp .Lbegin%d\n", counter);
    printf(".Lend%d:\n", counter);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_RETURN:
    gen_expr(node->lhs);
    printf("  pop %%rax\n");
    printf("  mov %%rbp, %%rsp\n");
    printf("  pop %%rbp\n");
    printf("  ret\n");
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }
}

// nを繰り上げ、alignの倍数で最も近い数を返す
// align_to(5, 8) -> 8, align_to(11, 8)
int align_to(int n, int align) { return (n + align - 1) / align * align; }

void assign_lvar_offsets(Function *prog) {
  int offset = 0;
  for (LVar *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = offset;
  }
  prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  // アセンブリの前半部分を出力
  printf("  .global main\n");
  printf("main:\n");

  // プロローグ
  // 変数26個分の領域を確保する
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");
  printf("  sub $%d, %%rsp\n", prog->stack_size);

  // コード生成
  gen_stmt(prog->body);

  // エピローグ
  // 最後の式の結果がRAXに残っているのでそれが返り値になる
  printf("  mov %%rbp, %%rsp\n");
  printf("  pop %%rbp\n");
  printf("  ret\n");
}
