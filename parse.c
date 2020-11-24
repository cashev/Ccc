#include "Ccc.h"

LVar *locals;
Token *token;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next)
    if (var->len == tok->len && !memcmp(tok->str, var->name, var->len))
      return var;
  return NULL;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char *op) {
  if (token->kind != TK_RESERVED || strlen(op) != token->len ||
      memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

// 次のトークンが識別子のときには、トークンを1つ読み進めて
// 識別子のトークンを返す。それ以外の場合にはNULLを返す
Token *consume_ident() {
  if (token->kind != TK_IDENT)
    return NULL;
  Token *t = token;
  token = token->next;
  return t;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

// Tokenが期待している記号のときには、Tokenを1つ読み進める。
// それ以外の場合にはエラーを報告する。
Token *skip(Token *tok, char *op) {
  if (!equal(tok, op)) {
    error_tok(tok, "expected '%s'", op);
  }
  return token->next;
}

bool at_eof() { return token->kind == TK_EOF; }

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

Node *new_node_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *new_var_node(LVar *var) {
  Node *node = new_node(ND_LVAR);
  node->var = var;
  return node;
}

LVar *new_lvar(char *name) {
  LVar *var = calloc(1, sizeof(LVar));
  var->name = name;
  var->next = locals;
  locals = var;
  return var;
}

Node *stmt();
Node *compound_stmt();
Node *expr_stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// stmt = return" expr ";"
//      | "{" compound-stmt
//      | "if" "(" expr ")" stmt ( "else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | expr-stmt
Node *stmt() {
  if (consume("return")) {
    Node *node = new_node(ND_RETURN);
    node->lhs = expr();

    token = skip(token, ";");
    return node;
  }

  if (consume("if")) {
    Node *node = new_node(ND_IF);
    token = skip(token, "(");
    node->cond = expr();
    token = skip(token, ")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  }

  if (consume("while")) {
    Node *node = new_node(ND_WHILE);
    token = skip(token, "(");
    node->cond = expr();
    token = skip(token, ")");
    node->then = stmt();
    return node;
  }

  if (consume("for")) {
    Node *node = new_node(ND_FOR);
    token = skip(token, "(");
    if (!consume(";")) {
      node->init = expr();
      token = skip(token, ";");
    }
    if (!consume(";")) {
      node->cond = expr();
      token = skip(token, ";");
    }
    if (!consume(")")) {
      node->inc = expr();
      token = skip(token, ")");
    }

    node->then = stmt();
    return node;
  }

  if (consume("{")) {
    return compound_stmt();
  }

  return expr_stmt();
}

// compound-stmt = stmt* "}"
Node *compound_stmt() {
  Node head = {};
  Node *cur = &head;
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;
  return node;
}

// expr-stmt = expr? ";"
Node *expr_stmt() {
  if (consume(";")) {
    return new_node(ND_BLOCK);
  }

  Node *node = new_unary(ND_EXPR_STMT, expr());
  token = skip(token, ";");
  return node;
}

// expr = assign
Node *expr() { return assign(); }

// assign = equality ("=" assign)?
Node *assign() {
  Node *node = equality();
  if (consume("="))
    node = new_binary(ND_ASSIGN, node, assign());
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume("==")) {
      node = new_binary(ND_EQ, node, relational());
      continue;
    }
    if (consume("!=")) {
      node = new_binary(ND_NE, node, relational());
      continue;
    }

    return node;
  }
}

// relational = add ("<" add | "<=" add | ">=" add)*
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume("<")) {
      node = new_binary(ND_LT, node, add());
      continue;
    }
    if (consume("<=")) {
      node = new_binary(ND_LE, node, add());
      continue;
    }
    if (consume(">")) {
      node = new_binary(ND_LT, add(), node);
      continue;
    }
    if (consume(">=")) {
      node = new_binary(ND_LE, add(), node);
      continue;
    }

    return node;
  }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume("+")) {
      node = new_binary(ND_ADD, node, mul());
      continue;
    }
    if (consume("-")) {
      node = new_binary(ND_SUB, node, mul());
      continue;
    }

    return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume("*")) {
      node = new_binary(ND_MUL, node, unary());
      continue;
    }
    if (consume("/")) {
      node = new_binary(ND_DIV, node, unary());
      continue;
    }

    return node;
  }
}

// unary = ("+" | "-") unary
//       | primary
Node *unary() {
  if (consume("+"))
    return unary();

  if (consume("-"))
    return new_binary(ND_SUB, new_node_num(0), unary());

  return primary();
}

// primary = num | ident | "(" expr ")"
Node *primary() {
  // 次のトークンが"("なら、"(" expr ")"のはず
  if (consume("(")) {
    Node *node = expr();
    token = skip(token, ")");
    return node;
  }

  // 識別子
  Token *tok = consume_ident();
  if (tok) {
    LVar *lvar = find_lvar(tok);
    if (!lvar) {
      lvar = new_lvar(tok->str);
      lvar->len = tok->len;
    }
    Node *node = new_var_node(lvar);
    return node;
  }

  // そうでなければ数値のはず
  return new_node_num(expect_number());
}

// program = stmt*
Function *parse(Token *tok) {
  token = tok;
  token = skip(token, "{");

  Function *prog = calloc(1, sizeof(Function));
  prog->body = compound_stmt();
  prog->locals = locals;
  return prog;
}
